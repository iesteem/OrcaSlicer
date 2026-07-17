#include <libslic3r/SLA/SupportTreeBuildsteps.hpp>

#include <libslic3r/SLA/SpatIndex.hpp>
#include <libslic3r/Optimize/NLoptOptimizer.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r {
namespace sla {

using Slic3r::opt::initvals;
using Slic3r::opt::bounds;
using Slic3r::opt::StopCriteria;
using Slic3r::opt::Optimizer;
using Slic3r::opt::AlgNLoptSubplex;
using Slic3r::opt::AlgNLoptGenetic;

StopCriteria get_criteria(const SupportTreeConfig &cfg)
{
    return StopCriteria{}
        .rel_score_diff(cfg.optimizer_rel_score_diff)
        .max_iterations(cfg.optimizer_max_iterations);
}

template<class C, class Hit = IndexedMesh::hit_result>
static Hit min_hit(const C &hits)
{
    auto mit = std::min_element(hits.begin(), hits.end(),
                                [](const Hit &h1, const Hit &h2) {
                                    return h1.distance() < h2.distance();
                                });

    return *mit;
}

SupportTreeBuildsteps::SupportTreeBuildsteps(SupportTreeBuilder &   builder,
                                             const SupportableMesh &sm)
    : m_cfg(sm.cfg)
    , m_mesh(sm.emesh)
    , m_support_pts(sm.pts)
    , m_support_nmls(sm.pts.size(), 3)
    , m_builder(builder)
    , m_points(sm.pts.size(), 3)
    , m_thr(builder.ctl().cancelfn)
{
    // 也将支撑点准备为 Eigen/IGL 格式，我们将主要使用此形式。

    long i = 0;
    for (const SupportPoint &sp : m_support_pts) {
        m_points.row(i)(X) = double(sp.pos(X));
        m_points.row(i)(Y) = double(sp.pos(Y));
        m_points.row(i)(Z) = double(sp.pos(Z));
        ++i;
    }
}

bool SupportTreeBuildsteps::execute(SupportTreeBuilder &   builder,
                                    const SupportableMesh &sm)
{
    if(sm.pts.empty()) return false;

    builder.ground_level = sm.emesh.ground_level() - sm.cfg.object_elevation_mm;

    SupportTreeBuildsteps alg(builder, sm);

    // 定义处理的各个步骤。我们稍后可以试验它们的顺序和依赖关系。
    enum Steps {
        BEGIN,
        FILTER,
        PINHEADS,
        CLASSIFY,
        ROUTING_GROUND,
        ROUTING_NONGROUND,
        CASCADE_PILLARS,
        MERGE_RESULT,
        DONE,
        ABORT,
        NUM_STEPS
        //...
    };

    // Collect the algorithm steps into a nice sequence
    std::array<std::function<void()>, NUM_STEPS> program = {
        [] () {
            // Begin...
            // Potentially clear up the shared data (not needed for now)
        },

        std::bind(&SupportTreeBuildsteps::filter, &alg),

        std::bind(&SupportTreeBuildsteps::add_pinheads, &alg),

        std::bind(&SupportTreeBuildsteps::classify, &alg),

        std::bind(&SupportTreeBuildsteps::routing_to_ground, &alg),

        std::bind(&SupportTreeBuildsteps::routing_to_model, &alg),

        std::bind(&SupportTreeBuildsteps::interconnect_pillars, &alg),

        std::bind(&SupportTreeBuildsteps::merge_result, &alg),

        [] () {
            // Done
        },

        [] () {
            // Abort
        }
    };

    Steps pc = BEGIN;

    if(sm.cfg.ground_facing_only) {
        program[ROUTING_NONGROUND] = []() {
            BOOST_LOG_TRIVIAL(info)
                << "Skipping model-facing supports as requested.";
        };
    }

    // 定义一个简单的自动机来运行我们的程序。
    auto progress = [&builder, &pc] () {
        static const std::array<std::string, NUM_STEPS> stepstr {
            "Starting",
            "Filtering",
            "Generate pinheads",
            "Classification",
            "Routing to ground",
            "Routing supports to model surface",
            "Interconnecting pillars",
            "Merging support mesh",
            "Done",
            "Abort"
        };

        static const std::array<unsigned, NUM_STEPS> stepstate {
            0,
            10,
            30,
            50,
            60,
            70,
            80,
            99,
            100,
            0
        };

        if(builder.ctl().stopcondition()) pc = ABORT;

        switch(pc) {
        case BEGIN: pc = FILTER; break;
        case FILTER: pc = PINHEADS; break;
        case PINHEADS: pc = CLASSIFY; break;
        case CLASSIFY: pc = ROUTING_GROUND; break;
        case ROUTING_GROUND: pc = ROUTING_NONGROUND; break;
        case ROUTING_NONGROUND: pc = CASCADE_PILLARS; break;
        case CASCADE_PILLARS: pc = MERGE_RESULT; break;
        case MERGE_RESULT: pc = DONE; break;
        case DONE:
        case ABORT: break;
        default: ;
        }

        builder.ctl().statuscb(stepstate[pc], stepstr[pc]);
    };

    // Just here we run the computation...
    while(pc < DONE) {
        progress();
        program[pc]();
    }

    return pc == ABORT;
}

IndexedMesh::hit_result SupportTreeBuildsteps::pinhead_mesh_intersect(
    const Vec3d &s,
    const Vec3d &dir,
    double       r_pin,
    double       r_back,
    double       width,
    double       sd)
{
    static const size_t SAMPLES = 8;

    // 稍微远离接触点，以避免在网格内表面进行光线投射。

    auto& m = m_mesh;
    using HitResult = IndexedMesh::hit_result;

    // Hit results
    std::array<HitResult, SAMPLES> hits;

    struct Rings {
        double rpin;
        double rback;
        Vec3d  spin;
        Vec3d  sback;
        PointRing<SAMPLES> ring;

        Vec3d backring(size_t idx) { return ring.get(idx, sback, rback); }
        Vec3d pinring(size_t idx) { return ring.get(idx, spin, rpin); }
    } rings {r_pin + sd, r_back + sd, s, s + width * dir, dir};

    // 我们将从头部钉尖沿钉头连接体（侧面）表面方向发射多条射线。
    // 结果将是最小的命中距离。

    ccr::for_each(size_t(0), hits.size(),
                  [&m, &rings, sd, &hits](size_t i) {

       // 钉球上的圆上的点
       Vec3d ps = rings.pinring(i);
       // 这是背部球体上的圆上的点
       Vec3d p = rings.backring(i);

       auto &hit = hits[i];

       // Point ps is not on mesh but can be inside or
       // outside as well. This would cause many problems
       // with ray-casting. To detect the position we will
       // use the ray-casting result (which has an is_inside
       // predicate).

       Vec3d n = (p - ps).normalized();
       auto  q = m.query_ray_hit(ps + sd * n, n);

       if (q.is_inside()) { // the hit is inside the model
           if (q.distance() > rings.rpin) {
               // 如果我们在模型内部且命中距离大于钉圆直径，
               // 这可能表明支撑点已经在模型内部，
               // 或者点周围确实没有空间。
               // 我们将为这些情况分配零命中距离，
               // 这将使函数返回值为具有零命中距离的无效射线。
               // （参见最后的 min_element）
               hit = HitResult(0.0);
           } else {
               // 从对象外部重新投射射线。
               // 起始点有 2*safety_distance 的偏移，
               // 因为原始射线也有一个偏移
               auto q2 = m.query_ray_hit(ps + (q.distance() + 2 * sd) * n, n);
               hit = q2;
           }
       } else
           hit = q;
    });

    return min_hit(hits);
}

IndexedMesh::hit_result SupportTreeBuildsteps::bridge_mesh_intersect(
    const Vec3d &src, const Vec3d &dir, double r, double sd)
{
    static const size_t SAMPLES = 8;
    PointRing<SAMPLES> ring{dir};

    using Hit = IndexedMesh::hit_result;

    // Hit results
    std::array<Hit, SAMPLES> hits;

    ccr::for_each(size_t(0), hits.size(),
                 [this, r, src, /*ins_check,*/ &ring, dir, sd, &hits] (size_t i)
    {
        Hit &hit = hits[i];

        // 钉球上的圆上的点
        Vec3d p = ring.get(i, src, r + sd);

        auto hr = m_mesh.query_ray_hit(p + r * dir, dir);

        if(/*ins_check && */hr.is_inside()) {
            if(hr.distance() > 2 * r + sd) hit = Hit(0.0);
            else {
                // 从对象外部重新投射射线
                hit = m_mesh.query_ray_hit(p + (hr.distance() + EPSILON) * dir, dir);
            }
        } else hit = hr;
    });

    return min_hit(hits);
}

bool SupportTreeBuildsteps::interconnect(const Pillar &pillar,
                                         const Pillar &nextpillar)
{
    // 我们需要获取锯齿模式的起点。我们需要注意两个柱头连接点的高度不同。
    // 我们可以从最低的连接点开始，但这种策略会使许多柱对保持未连接状态，
    // 其中较短的柱太短而无法开始新桥，但较长的柱仍然可以与较短的柱桥接。
    bool was_connected = false;

    Vec3d supper = pillar.startpoint();
    Vec3d slower = nextpillar.startpoint();
    Vec3d eupper = pillar.endpoint();
    Vec3d elower = nextpillar.endpoint();

    double zmin = m_builder.ground_level + m_cfg.base_height_mm;
    eupper(Z) = std::max(eupper(Z), zmin);
    elower(Z) = std::max(elower(Z), zmin);

    // 两个柱的可用长度应为正数
    if(slower(Z) - elower(Z) < 0) return false;
    if(supper(Z) - eupper(Z) < 0) return false;

    double pillar_dist = distance(Vec2d{slower(X), slower(Y)},
                                  Vec2d{supper(X), supper(Y)});
    double bridge_distance = pillar_dist / std::cos(-m_cfg.bridge_slope);
    double zstep = pillar_dist * std::tan(-m_cfg.bridge_slope);

    if(pillar_dist < 2 * m_cfg.head_back_radius_mm ||
        pillar_dist > m_cfg.max_pillar_link_distance_mm) return false;

    if(supper(Z) < slower(Z)) supper.swap(slower);
    if(eupper(Z) < elower(Z)) eupper.swap(elower);

    double startz = 0, endz = 0;

    startz = slower(Z) - zstep < supper(Z) ? slower(Z) - zstep : slower(Z);
    endz = eupper(Z) + zstep > elower(Z) ? eupper(Z) + zstep : eupper(Z);

    if(slower(Z) - eupper(Z) < std::abs(zstep)) {
        // 没有空间容纳一个交叉

        // 获取最大可用空间
        startz = std::min(supper(Z), slower(Z) - zstep);
        endz = std::max(eupper(Z) + zstep, elower(Z));

        // 居中对齐
        double available_dist = (startz - endz);
        double rounds = std::floor(available_dist / std::abs(zstep));
        startz -= 0.5 * (available_dist - rounds * std::abs(zstep));
    }

    auto pcm = m_cfg.pillar_connection_mode;
    bool docrosses =
        pcm == PillarConnectionMode::cross ||
        (pcm == PillarConnectionMode::dynamic &&
         pillar_dist > 2*m_cfg.base_radius_mm);

    // 'sj' 表示起始连接点，'ej' 是桥的末端连接点。
    // 它们将在每次迭代中交换，从而形成锯齿模式。
    // 根据配置参数，可以添加第二座桥，从而在柱之间形成交叉连接。
    Vec3d sj = supper, ej = slower; sj(Z) = startz; ej(Z) = sj(Z) + zstep;

    // TODO: 这是一个变通方法，用于避免错误的最后一个桥
    while(ej(Z) >= eupper(Z) /*endz*/) {
        if(bridge_mesh_distance(sj, dirv(sj, ej), pillar.r) >= bridge_distance)
        {
            m_builder.add_crossbridge(sj, ej, pillar.r);
            was_connected = true;
        }

        // double bridging: (crosses)
        if(docrosses) {
            Vec3d sjback(ej(X), ej(Y), sj(Z));
            Vec3d ejback(sj(X), sj(Y), ej(Z));
            if (sjback(Z) <= slower(Z) && ejback(Z) >= eupper(Z) &&
                bridge_mesh_distance(sjback, dirv(sjback, ejback),
                                      pillar.r) >= bridge_distance) {
                // 需要检查交叉杆的碰撞
                m_builder.add_crossbridge(sjback, ejback, pillar.r);
                was_connected = true;
            }
        }

        sj.swap(ej);
        ej(Z) = sj(Z) + zstep;
    }

    return was_connected;
}

bool SupportTreeBuildsteps::connect_to_nearpillar(const Head &head,
                                                  long        nearpillar_id)
{
    auto nearpillar = [this, nearpillar_id]() -> const Pillar& {
        return m_builder.pillar(nearpillar_id);
    };

    if (m_builder.bridgecount(nearpillar()) > m_cfg.max_bridges_on_pillar)
        return false;

    Vec3d headjp = head.junction_point();
    Vec3d nearjp_u = nearpillar().startpoint();
    Vec3d nearjp_l = nearpillar().endpoint();

    double r = head.r_back_mm;
    double d2d = distance(to_2d(headjp), to_2d(nearjp_u));
    double d3d = distance(headjp, nearjp_u);

    double hdiff = nearjp_u(Z) - headjp(Z);
    double slope = std::atan2(hdiff, d2d);

    Vec3d bridgestart = headjp;
    Vec3d bridgeend = nearjp_u;
    double max_len = r * m_cfg.max_bridge_length_mm / m_cfg.head_back_radius_mm;
    double max_slope = m_cfg.bridge_slope;
    double zdiff = 0.0;

    // check the default situation if feasible for a bridge
    if(d3d > max_len || slope > -max_slope) {
        // 无法连接两个头部连接点。我们必须搜索合适的接触点。

        double Zdown = headjp(Z) + d2d * std::tan(-max_slope);
        Vec3d touchjp = bridgeend; touchjp(Z) = Zdown;
        double D = distance(headjp, touchjp);
        zdiff = Zdown - nearjp_u(Z);

        if(zdiff > 0) {
            Zdown -= zdiff;
            bridgestart(Z) -= zdiff;
            touchjp(Z) = Zdown;

            double t = bridge_mesh_distance(headjp, DOWN, r);

            // 我们无法在源头部下方插入柱来连接到附近柱的起始连接点
            if(t < zdiff) return false;
        }

        if(Zdown <= nearjp_u(Z) && Zdown >= nearjp_l(Z) && D < max_len)
            bridgeend(Z) = Zdown;
        else
            return false;
    }

    // 桥允许连接的位置与地面之间将有一个最小距离。这是一个经验值。
    double minz = m_builder.ground_level + 4 * head.r_back_mm;
    if(bridgeend(Z) < minz) return false;

    double t = bridge_mesh_distance(bridgestart, dirv(bridgestart, bridgeend), r);

    // 无法插入桥。（进一步搜索可能不值得麻烦）
    if(t < distance(bridgestart, bridgeend)) return false;

    std::lock_guard<ccr::BlockingMutex> lk(m_bridge_mutex);

    if (m_builder.bridgecount(nearpillar()) < m_cfg.max_bridges_on_pillar) {
        // 起始头部下方需要一个部分柱。
        if(zdiff > 0) {
            m_builder.add_pillar(head.id, headjp.z() - bridgestart.z());
            m_builder.add_junction(bridgestart, r);
            m_builder.add_bridge(bridgestart, bridgeend, r);
        } else {
            m_builder.add_bridge(head.id, bridgeend);
        }

        m_builder.increment_bridges(nearpillar());
    } else return false;

    return true;
}

bool SupportTreeBuildsteps::create_ground_pillar(const Vec3d &hjp,
                                                 const Vec3d &sourcedir,
                                                 double       radius,
                                                 long         head_id)
{
    Vec3d  jp           = hjp, endp = jp, dir = sourcedir;
    long   pillar_id    = SupportTreeNode::ID_UNSET;
    bool   can_add_base = false, non_head = false;

    double gndlvl = 0.; // 底座应处的 Z 层级
    double jp_gnd = 0.; // 连接点中心可以达到的最低 Z
    double gap_dist = 0.; // 模型与垫之间的间隙距离

    auto to_floor = [&gndlvl](const Vec3d &p) { return Vec3d{p.x(), p.y(), gndlvl}; };

    auto eval_limits = [this, &radius, &can_add_base, &gndlvl, &gap_dist, &jp_gnd]
        (bool base_en = true)
    {
        can_add_base  = base_en && radius >= m_cfg.head_back_radius_mm;
        double base_r = can_add_base ? m_cfg.base_radius_mm : 0.;
        gndlvl        = m_builder.ground_level;
        if (!can_add_base) gndlvl -= m_mesh.ground_level_offset();
        jp_gnd   = gndlvl + (can_add_base ? 0. : m_cfg.head_back_radius_mm);
        gap_dist = m_cfg.pillar_base_safety_distance_mm + base_r + EPSILON;
    };

    eval_limits();

    // 我们正在处理一个可能太长的小柱
    if (radius < m_cfg.head_back_radius_mm && jp.z() - gndlvl > 20 * radius)
    {
        std::optional<DiffBridge> diffbr =
            search_widening_path(jp, dir, radius, m_cfg.head_back_radius_mm);

        if (diffbr && diffbr->endp.z() > jp_gnd) {
            auto &br = m_builder.add_diffbridge(*diffbr);
            if (head_id >= 0) m_builder.head(head_id).bridge_id = br.id;
            endp = diffbr->endp;
            radius = diffbr->end_r;
            m_builder.add_junction(endp, radius);
            non_head = true;
            dir = diffbr->get_dir();
            eval_limits();
        } else return false;
    }

    if (m_cfg.object_elevation_mm < EPSILON)
    {
        // 获取校正桥的合适方向。它是原始 sourcedir 的方位角，
        // 但极角被饱和到配置的桥斜率。
        auto [polar, azimuth] = dir_to_spheric(dir);
        polar = PI - m_cfg.bridge_slope;
        Vec3d d = spheric_to_dir(polar, azimuth).normalized();
        double t = bridge_mesh_distance(endp, d, radius);
        double tmax = std::min(m_cfg.max_bridge_length_mm, t);
        t = 0.;

        double zd = endp.z() - jp_gnd;
        double tmax2 = zd / std::sqrt(1 - m_cfg.bridge_slope * m_cfg.bridge_slope);
        tmax = std::min(tmax, tmax2);

        Vec3d nexp = endp;
        double dlast = 0.;
        while (((dlast = std::sqrt(m_mesh.squared_distance(to_floor(nexp)))) < gap_dist ||
                !std::isinf(bridge_mesh_distance(nexp, DOWN, radius))) && t < tmax) {
            t += radius;
            nexp = endp + t * d;
        }

        if (dlast < gap_dist && can_add_base) {
            nexp         = endp;
            t            = 0.;
            can_add_base = false;
            eval_limits(can_add_base);

            zd = endp.z() - jp_gnd;
            tmax2 = zd / std::sqrt(1 - m_cfg.bridge_slope * m_cfg.bridge_slope);
            tmax = std::min(tmax, tmax2);

            while (((dlast = std::sqrt(m_mesh.squared_distance(to_floor(nexp)))) < gap_dist ||
                    !std::isinf(bridge_mesh_distance(nexp, DOWN, radius))) && t < tmax) {
                t += radius;
                nexp = endp + t * d;
            }
        }

        // 找不到避免垫间隙的路径
        if (dlast < gap_dist) return false;

        if (t > 0.) { // 需要制作额外的桥
            const Bridge& br = m_builder.add_bridge(endp, nexp, radius);
            if (head_id >= 0) m_builder.head(head_id).bridge_id = br.id;

            m_builder.add_junction(nexp, radius);
            endp = nexp;
            non_head = true;
        }
    }

    Vec3d gp = to_floor(endp);
    double h = endp.z() - gp.z();

    pillar_id = head_id >= 0 && !non_head ? m_builder.add_pillar(head_id, h) :
                                            m_builder.add_pillar(gp, h, radius);

    if (can_add_base)
        add_pillar_base(pillar_id);

    if(pillar_id >= 0) // 将柱端点保存到空间索引中
        m_pillar_index.guarded_insert(m_builder.pillar(pillar_id).endpt,
                                      unsigned(pillar_id));

    return true;
}

std::optional<DiffBridge> SupportTreeBuildsteps::search_widening_path(
    const Vec3d &jp, const Vec3d &dir, double radius, double new_radius)
{
    double w = radius + 2 * m_cfg.head_back_radius_mm;
    double stopval = w + jp.z() - m_builder.ground_level;
    Optimizer<AlgNLoptSubplex> solver(get_criteria(m_cfg).stop_score(stopval));

    auto [polar, azimuth] = dir_to_spheric(dir);

    double fallback_ratio = radius / m_cfg.head_back_radius_mm;

    auto oresult = solver.to_max().optimize(
        [this, jp, radius, new_radius](const opt::Input<3> &input) {
            auto &[plr, azm, t] = input;

            auto   d   = spheric_to_dir(plr, azm).normalized();
            double ret = pinhead_mesh_intersect(jp, d, radius, new_radius, t)
                             .distance();
            double down = bridge_mesh_distance(jp + t * d, d, new_radius);

            if (ret > t && std::isinf(down))
                ret += jp.z() - m_builder.ground_level;

            return ret;
        },
        initvals({polar, azimuth, w}), // 从已有的开始
        bounds({
            {PI - m_cfg.bridge_slope, PI}, // 不得超过斜率限制
            {-PI, PI}, // 方位角可以全范围搜索
            {radius + m_cfg.head_back_radius_mm,
                  fallback_ratio * m_cfg.max_bridge_length_mm}
        }));

    if (oresult.score >= stopval) {
        polar       = std::get<0>(oresult.optimum);
        azimuth     = std::get<1>(oresult.optimum);
        double t    = std::get<2>(oresult.optimum);
        Vec3d  endp = jp + t * spheric_to_dir(polar, azimuth);

        return DiffBridge(jp, endp, radius, m_cfg.head_back_radius_mm);
    }

    return {};
}

void SupportTreeBuildsteps::filter()
{
    // 获取相互距离太近的点，只保留第一个
    auto aliases = cluster(m_points, D_SP, 2);

    PtIndices filtered_indices;
    filtered_indices.reserve(aliases.size());
    m_iheads.reserve(aliases.size());
    m_iheadless.reserve(aliases.size());
    for(auto& a : aliases) {
        // 这里我们只保留聚类中的前点。
        filtered_indices.emplace_back(a.front());
    }

    // calculate the normals to the triangles for filtered points
    auto nmls = sla::normals(m_points, m_mesh, m_cfg.head_front_radius_mm,
                             m_thr, filtered_indices);

    // Not all of the support points have to be a valid position for
    // support creation. The angle may be inappropriate or there may
    // not be enough space for the pinhead. Filtering is applied for
    // these reasons.

    std::vector<Head> heads; heads.reserve(m_support_pts.size());
    for (const SupportPoint &sp : m_support_pts) {
        m_thr();
        heads.emplace_back(
            std::nan(""),
            sp.head_front_radius,
            0.,
            m_cfg.head_penetration_mm,
            Vec3d::Zero(),         // dir
            sp.pos.cast<double>() // displacement
            );
    }

    std::function<void(unsigned, size_t, double)> filterfn;
    filterfn = [this, &nmls, &heads, &filterfn](unsigned fidx, size_t i, double back_r) {
        m_thr();

        auto n = nmls.row(Eigen::Index(i));

        // for all normals we generate the spherical coordinates and
        // saturate the polar angle to 45 degrees from the bottom then
        // convert back to standard coordinates to get the new normal.
        // 然后我们只需从两个法线创建四元数 (Quaternion::FromTwoVectors)
        // 并将旋转应用到箭头头部。

        auto [polar, azimuth] = dir_to_spheric(n);

        // skip if the tilt is not sane
        if (polar < PI - m_cfg.normal_cutoff_angle) return;

        // 我们将极角饱和到 3pi/4
        polar = std::max(polar, PI - m_cfg.bridge_slope);

        // save the head (pinpoint) position
        Vec3d hp = m_points.row(fidx);

        double lmin = m_cfg.head_width_mm, lmax = lmin;

        if (back_r < m_cfg.head_back_radius_mm) {
            lmin = 0., lmax = m_cfg.head_penetration_mm;
        }

        // 钉头不与模型碰撞所需的距离。
        double w = lmin + 2 * back_r + 2 * m_cfg.head_front_radius_mm -
                   m_cfg.head_penetration_mm;

        double pin_r = double(m_support_pts[fidx].head_front_radius);

        // Reassemble the now corrected normal
        auto nn = spheric_to_dir(polar, azimuth).normalized();

        // check available distance
        IndexedMesh::hit_result t = pinhead_mesh_intersect(hp, nn, pin_r,
                                                           back_r, w);

        if (t.distance() < w) {
            // 让我们尝试优化这个角度，可能存在一个不与模型几何体碰撞
            // 且非常接近默认值的可行法线。

            Optimizer<AlgNLoptGenetic> solver(get_criteria(m_cfg));
            solver.seed(0); // 我们希望行为是确定性的

            auto oresult = solver.to_max().optimize(
                [this, pin_r, back_r, hp](const opt::Input<3> &input)
                {
                    auto &[plr, azm, l] = input;

                    auto dir = spheric_to_dir(plr, azm).normalized();

                    return pinhead_mesh_intersect(
                        hp, dir, pin_r, back_r, l).distance();
                },
                initvals({polar, azimuth, (lmin + lmax) / 2.}), // 从已有的开始
                bounds({
                    {PI - m_cfg.bridge_slope, PI},    // 不得超过斜率限制
                    {-PI, PI}, // 方位角可以全范围搜索
                    {lmin, lmax}
                }));

            if(oresult.score > w) {
                polar = std::get<0>(oresult.optimum);
                azimuth = std::get<1>(oresult.optimum);
                nn = spheric_to_dir(polar, azimuth).normalized();
                lmin = std::get<2>(oresult.optimum);
                t = IndexedMesh::hit_result(oresult.score);
            }
        }

        if (t.distance() > w && hp(Z) + w * nn(Z) >= m_builder.ground_level) {
            Head &h = heads[fidx];
            h.id = fidx; h.dir = nn; h.width_mm = lmin; h.r_back_mm = back_r;
        } else if (back_r > m_cfg.head_fallback_radius_mm) {
            filterfn(fidx, i, m_cfg.head_fallback_radius_mm);
        }
    };

    ccr::for_each(size_t(0), filtered_indices.size(),
                  [this, &filterfn, &filtered_indices] (size_t i) {
                      filterfn(filtered_indices[i], i, m_cfg.head_back_radius_mm);
                  });

    for (size_t i = 0; i < heads.size(); ++i)
        if (heads[i].is_valid()) {
            m_builder.add_head(i, heads[i]);
            m_iheads.emplace_back(i);
        }

    m_thr();
}

void SupportTreeBuildsteps::add_pinheads()
{
}

void SupportTreeBuildsteps::classify()
{
    // 我们应首先获取直接到达地面的头部
    PtIndices ground_head_indices;
    ground_head_indices.reserve(m_iheads.size());
    m_iheads_onmodel.reserve(m_iheads.size());

    // First we decide which heads reach the ground and can be full
    // pillars and which shall be connected to the model surface (or
    // search a suitable path around the surface that leads to the
    // ground -- TODO)
    for(unsigned i : m_iheads) {
        m_thr();

        Head &head = m_builder.head(i);
        double r = head.r_back_mm;
        Vec3d headjp = head.junction_point();

        // collision check
        auto hit = bridge_mesh_intersect(headjp, DOWN, r);

        if(std::isinf(hit.distance())) ground_head_indices.emplace_back(i);
        else if(m_cfg.ground_facing_only)  head.invalidate();
        else m_iheads_onmodel.emplace_back(i);

        m_head_to_ground_scans[i] = hit;
    }

    // 我们想搜索在 XY 平面上彼此距离足够远、不会交叉其柱基的点聚类。
    // 这些支撑点聚类将合并到一个柱中，可能在其质心支撑点处。

    auto pointfn = [this](unsigned i) {
        return m_builder.head(i).junction_point();
    };

    auto predicate = [this](const PointIndexEl &e1,
                            const PointIndexEl &e2) {
        double d2d = distance(to_2d(e1.first), to_2d(e2.first));
        double d3d = distance(e1.first, e2.first);
        return d2d < 2 * m_cfg.base_radius_mm
               && d3d < m_cfg.max_bridge_length_mm;
    };

    m_pillar_clusters = cluster(ground_head_indices, pointfn, predicate,
                                m_cfg.max_bridges_on_pillar);
}

void SupportTreeBuildsteps::routing_to_ground()
{
    ClusterEl cl_centroids;
    cl_centroids.reserve(m_pillar_clusters.size());

    for (auto &cl : m_pillar_clusters) {
        m_thr();

        // place all the centroid head positions into the index. We
        // 将查询替代的柱位置。如果侧头无法连接到聚类质心，
        // 我们必须搜索另一个具有完整柱的头部。
        // 此外，当聚类中有两个元素时，质心是任意的，
        // 允许侧头连接到附近的柱以增加结构稳定性。

        if (cl.empty()) continue;

        // get the current cluster centroid
        auto &      thr    = m_thr;
        const auto &points = m_points;

        long lcid = cluster_centroid(
            cl, [&points](size_t idx) { return points.row(long(idx)); },
            [thr](const Vec3d &p1, const Vec3d &p2) {
                thr();
                return distance(Vec2d(p1(X), p1(Y)), Vec2d(p2(X), p2(Y)));
            });

        assert(lcid >= 0);
        unsigned hid = cl[size_t(lcid)]; // Head ID

        cl_centroids.emplace_back(hid);

        Head &h = m_builder.head(hid);

        if (!create_ground_pillar(h.junction_point(), h.dir, h.r_back_mm, h.id)) {
            BOOST_LOG_TRIVIAL(warning)
                << "Pillar cannot be created for support point id: " << hid;
            m_iheads_onmodel.emplace_back(h.id);
            continue;
        }
    }

    // now we will go through the clusters ones again and connect the
    // sidepoints with the cluster centroid (which is a ground pillar)
    // or a nearby pillar if the centroid is unreachable.
    size_t ci = 0;
    for (auto cl : m_pillar_clusters) {
        m_thr();

        auto cidx = cl_centroids[ci++];

        auto q = m_pillar_index.query(m_builder.head(cidx).junction_point(), 1);
        if (!q.empty()) {
            long centerpillarID = q.front().second;
            for (auto c : cl) {
                m_thr();
                if (c == cidx) continue;

                auto &sidehead = m_builder.head(c);

                if (!connect_to_nearpillar(sidehead, centerpillarID) &&
                    !search_pillar_and_connect(sidehead)) {
                    Vec3d pstart = sidehead.junction_point();
                    // Vec3d pend = Vec3d{pstart(X), pstart(Y), gndlvl};
                    // Could not find a pillar, create one
                    create_ground_pillar(pstart, sidehead.dir, sidehead.r_back_mm, sidehead.id);
                }
            }
        }
    }
}

bool SupportTreeBuildsteps::connect_to_ground(Head &head, const Vec3d &dir)
{
    auto hjp = head.junction_point();
    double r = head.r_back_mm;
    double t = bridge_mesh_distance(hjp, dir, head.r_back_mm);
    double d = 0, tdown = 0;
    t = std::min(t, m_cfg.max_bridge_length_mm * r / m_cfg.head_back_radius_mm);

    while (d < t && !std::isinf(tdown = bridge_mesh_distance(hjp + d * dir, DOWN, r)))
        d += r;

    if(!std::isinf(tdown)) return false;

    Vec3d endp = hjp + d * dir;
    bool ret = false;

    if ((ret = create_ground_pillar(endp, dir, head.r_back_mm))) {
        m_builder.add_bridge(head.id, endp);
        m_builder.add_junction(endp, head.r_back_mm);
    }

    return ret;
}

bool SupportTreeBuildsteps::connect_to_ground(Head &head)
{
    if (connect_to_ground(head, head.dir)) return true;

    // Optimize bridge direction:
    // Straight path failed so we will try to search for a suitable
    // direction out of the cavity.
    auto [polar, azimuth] = dir_to_spheric(head.dir);

    Optimizer<AlgNLoptGenetic> solver(get_criteria(m_cfg).stop_score(1e6));
    solver.seed(0); // 我们希望行为是确定性的

    double r_back = head.r_back_mm;
    Vec3d hjp = head.junction_point();
    auto oresult = solver.to_max().optimize(
        [this, hjp, r_back](const opt::Input<2> &input) {
            auto &[plr, azm] = input;
            Vec3d n = spheric_to_dir(plr, azm).normalized();
            return bridge_mesh_distance(hjp, n, r_back);
        },
        initvals({polar, azimuth}),  // 让我们从已有的开始
        bounds({ {PI - m_cfg.bridge_slope, PI}, {-PI, PI} })
    );

    Vec3d bridgedir = spheric_to_dir(oresult.optimum).normalized();
    return connect_to_ground(head, bridgedir);
}

bool SupportTreeBuildsteps::connect_to_model_body(Head &head)
{
    if (head.id <= SupportTreeNode::ID_UNSET) return false;

    auto it = m_head_to_ground_scans.find(unsigned(head.id));
    if (it == m_head_to_ground_scans.end()) return false;

    auto &hit = it->second;

    if (!hit.is_hit()) {
        // TODO scan for potential anchor points on model surface
        return false;
    }

    Vec3d hjp = head.junction_point();
    double zangle = std::asin(hit.direction()(Z));
    zangle = std::max(zangle, PI/4);
    double h = std::sin(zangle) * head.fullwidth();

    // 我们想要的尾部头部的宽度...
    h = std::min(hit.distance() - head.r_back_mm, h);

    // 如果这是小柱，不用管尾部宽度，可以为 0。
    if (head.r_back_mm < m_cfg.head_back_radius_mm) h = std::max(h, 0.);
    else if (h <= 0.) return false;

    Vec3d endp{hjp(X), hjp(Y), hjp(Z) - hit.distance() + h};
    auto center_hit = m_mesh.query_ray_hit(hjp, DOWN);

    double hitdiff = center_hit.distance() - hit.distance();
    Vec3d hitp = std::abs(hitdiff) < 2*head.r_back_mm?
                     center_hit.position() : hit.position();

    long pillar_id = m_builder.add_pillar(head.id, hjp.z() - endp.z());
    Pillar &pill = m_builder.pillar(pillar_id);

    Vec3d taildir = endp - hitp;
    double dist = (hitp - endp).norm() + m_cfg.head_penetration_mm;
    double w = dist - 2 * head.r_pin_mm - head.r_back_mm;

    if (w < 0.) {
        BOOST_LOG_TRIVIAL(error) << "Pinhead width is negative!";
        w = 0.;
    }

    m_builder.add_anchor(head.r_back_mm, head.r_pin_mm, w,
                         m_cfg.head_penetration_mm, taildir, hitp);

    m_pillar_index.guarded_insert(pill.endpoint(), pill.id);

    return true;
}

bool SupportTreeBuildsteps::search_pillar_and_connect(const Head &source)
{
    // Hope that a local copy takes less time than the whole search loop.
    // 我们还需要从复制的索引中逐步删除元素。
    PointIndex spindex = m_pillar_index.guarded_clone();

    long nearest_id = SupportTreeNode::ID_UNSET;

    Vec3d querypt = source.junction_point();

    while(nearest_id < 0 && !spindex.empty()) { m_thr();
        // loop until a suitable head was not found
        // if there is a pillar closer than the cluster center
        // (this may happen as the clustering is not perfect)
        // than we will bridge to this closer pillar

        Vec3d qp(querypt(X), querypt(Y), m_builder.ground_level);
        auto qres = spindex.nearest(qp, 1);
        if(qres.empty()) break;

        auto ne = qres.front();
        nearest_id = ne.second;

        if(nearest_id >= 0) {
            if (size_t(nearest_id) < m_builder.pillarcount()) {
                if(!connect_to_nearpillar(source, nearest_id) ||
                    m_builder.pillar(nearest_id).r < source.r_back_mm) {
                    nearest_id = SupportTreeNode::ID_UNSET;    // continue searching
                    spindex.remove(ne);       // without the current pillar
                }
            }
        }
    }

    return nearest_id >= 0;
}

void SupportTreeBuildsteps::routing_to_model()
{
    // 我们需要检查是否有到热床表面的简单出路。
    // 如果可以用短于最小桥接距离的桥接路由到那里。

    ccr::for_each(m_iheads_onmodel.begin(), m_iheads_onmodel.end(),
                  [this] (const unsigned idx) {
        m_thr();

        auto& head = m_builder.head(idx);

        // Search nearby pillar
        if (search_pillar_and_connect(head)) { return; }

        // Cannot connect to nearby pillar. We will try to search for
        // a route to the ground.
        if (connect_to_ground(head)) { return; }

        // No route to the ground, so connect to the model body as a last resort
        if (connect_to_model_body(head)) { return; }

        // 我们未能路由此头部。
        BOOST_LOG_TRIVIAL(warning)
                << "Failed to route model facing support point. ID: " << idx;

        head.invalidate();
    });
}

void SupportTreeBuildsteps::interconnect_pillars()
{
    // 现在介绍连接柱与柱的算法。
    // 理想情况下，每个柱应至少与一个在其 max_pillar_link_distance 范围内的邻居连接

    // 高度超过 H1 的柱将需要至少一个邻居连接。高度超过 H2 则需要两个邻居。
    double H1 = m_cfg.max_solo_pillar_height_mm;
    double H2 = m_cfg.max_dual_pillar_height_mm;
    double d = m_cfg.max_pillar_link_distance_mm;

    // 只有当高度比大于 50% 时，两个柱之间的连接才有效
    double min_height_ratio = 0.5;

    std::set<unsigned long> pairs;

    // 将柱与其邻居连接的函数。邻居数量由配置指定。
    // 此函数为 pillar 索引中的每个柱调用。
    // 一对柱不会被多次连接，这是由记录已处理柱对的 'pairs' 集合保证的。
    auto cascadefn =
        [this, d, &pairs, min_height_ratio, H1] (const PointIndexEl& el)
    {
        Vec3d qp = el.first;    // endpoint of the pillar

        const Pillar& pillar = m_builder.pillar(el.second); // actual pillar

        // 获取柱应连接的最大邻居数
        unsigned neighbors = m_cfg.pillar_cascade_neighbors;

        // connections are already enough for the pillar
        if(pillar.links >= neighbors) return;

        double max_d = d * pillar.r / m_cfg.head_back_radius_mm;
        // Query all remaining points within reach
        auto qres = m_pillar_index.query([qp, max_d](const PointIndexEl& e){
            return distance(e.first, qp) < max_d;
        });

        // 按距离排序结果（需要检查是否有必要）
        std::sort(qres.begin(), qres.end(),
                  [qp](const PointIndexEl& e1, const PointIndexEl& e2){
                      return distance(e1.first, qp) < distance(e2.first, qp);
                  });

        for(auto& re : qres) { // process the queried neighbors

            if(re.second == el.second) continue; // Skip self

            auto a = el.second, b = re.second;

            // 获取给定对的唯一哈希值（顺序不重要）
            auto hashval = pairhash(a, b);

            // Search for the pair amongst the remembered pairs
            if(pairs.find(hashval) != pairs.end()) continue;

            const Pillar& neighborpillar = m_builder.pillar(re.second);

            // 此邻居已被占用，跳过
            if (neighborpillar.links >= neighbors) continue;
            if (neighborpillar.r < pillar.r) continue;

            if(interconnect(pillar, neighborpillar)) {
                pairs.insert(hashval);

                // 如果两个柱之间的互连长度小于较长柱高度的 50%，则不计数
                if(pillar.height < H1 ||
                    neighborpillar.height / pillar.height > min_height_ratio)
                    m_builder.increment_links(pillar);

                if(neighborpillar.height < H1 ||
                    pillar.height / neighborpillar.height > min_height_ratio)
                    m_builder.increment_links(neighborpillar);

            }

            // 一个柱的连接已经足够
            if(pillar.links >= neighbors) break;
        }
    };

    // Run the cascade for the pillars in the index
    m_pillar_index.foreach(cascadefn);

    // 如果我们允许某些柱不与任何邻居连接，这里就完成了。
    // 但这可能使支撑树无法打印。
    //
    // 当前的解决方案是在这些孤立柱旁边插入额外的柱。
    // 根据孤立柱的长度，可能会插入一个甚至两个额外的柱。

    size_t pillarcount = m_builder.pillarcount();

    // 再次遍历所有柱，这次是在整个支撑树中，而不仅仅是索引。
    for(size_t pid = 0; pid < pillarcount; pid++) {
        auto pillar = [this, pid]() { return m_builder.pillar(pid); };

        // 决定需要多少额外柱：

        unsigned needpillars = 0;
        if (pillar().bridges > m_cfg.max_bridges_on_pillar)
            needpillars = 3;
        else if (pillar().links < 2 && pillar().height > H2) {
            // Not enough neighbors to support this pillar
            needpillars = 2;
        } else if (pillar().links < 1 && pillar().height > H1) {
            // No neighbors could be found and the pillar is too long.
            needpillars = 1;
        }

        needpillars = std::max(pillar().links, needpillars) - pillar().links;
        if (needpillars == 0) continue;

        // 搜索新的柱位置：

        bool   found    = false;
        double alpha    = 0; // goes to 2Pi
        double r        = 2 * m_cfg.base_radius_mm;
        Vec3d  pillarsp = pillar().startpoint();

        // 起始点检测的临时值
        Vec3d sp(pillarsp(X), pillarsp(Y), pillarsp(Z) - r);

        // A vector of bool for placement feasbility
        std::vector<bool>  canplace(needpillars, false);
        std::vector<Vec3d> spts(needpillars); // vector of starting points

        double gnd      = m_builder.ground_level;
        double min_dist = m_cfg.pillar_base_safety_distance_mm +
                          m_cfg.base_radius_mm + EPSILON;

        while(!found && alpha < 2*PI) {
            for (unsigned n = 0;
                 n < needpillars && (!n || canplace[n - 1]);
                 n++)
            {
                double a = alpha + n * PI / 3;
                Vec3d  s = sp;
                s(X) += std::cos(a) * r;
                s(Y) += std::sin(a) * r;
                spts[n] = s;

                // 垂直向下检查路径
                Vec3d check_from = s + Vec3d{0., 0., pillar().r};
                auto hr = bridge_mesh_intersect(check_from, DOWN, pillar().r);
                Vec3d gndsp{s(X), s(Y), gnd};

                // 如果路径畅通，检查柱基碰撞
                canplace[n] = std::isinf(hr.distance()) &&
                              std::sqrt(m_mesh.squared_distance(gndsp)) >
                                  min_dist;
            }

            found = std::all_of(canplace.begin(), canplace.end(),
                                [](bool v) { return v; });

            // 将尝试 20 个角度...
            alpha += 0.1 * PI;
        }

        std::vector<long> newpills;
        newpills.reserve(needpillars);

        if (found)
            for (unsigned n = 0; n < needpillars; n++) {
                Vec3d s = spts[n];
                Pillar p(Vec3d{s.x(), s.y(), gnd}, s.z() - gnd, pillar().r);

                if (interconnect(pillar(), p)) {
                    Pillar &pp = m_builder.pillar(m_builder.add_pillar(p));

                    add_pillar_base(pp.id);

                    m_pillar_index.insert(pp.endpoint(), unsigned(pp.id));

                    m_builder.add_junction(s, pillar().r);
                    double t = bridge_mesh_distance(pillarsp, dirv(pillarsp, s),
                                                    pillar().r);
                    if (distance(pillarsp, s) < t)
                        m_builder.add_bridge(pillarsp, s, pillar().r);

                    if (pillar().endpoint()(Z) > m_builder.ground_level + pillar().r)
                        m_builder.add_junction(pillar().endpoint(), pillar().r);

                    newpills.emplace_back(pp.id);
                    m_builder.increment_links(pillar());
                    m_builder.increment_links(pp);
                }
            }

        if(!newpills.empty()) {
            for(auto it = newpills.begin(), nx = std::next(it);
                 nx != newpills.end(); ++it, ++nx) {
                const Pillar& itpll = m_builder.pillar(*it);
                const Pillar& nxpll = m_builder.pillar(*nx);
                if(interconnect(itpll, nxpll)) {
                    m_builder.increment_links(itpll);
                    m_builder.increment_links(nxpll);
                }
            }

            m_pillar_index.foreach(cascadefn);
        }
    }
}

}} // namespace Slic3r::sla
