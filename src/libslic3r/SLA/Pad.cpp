#include <libslic3r/SLA/Pad.hpp>
#include <libslic3r/SLA/SpatIndex.hpp>
#include <libslic3r/SLA/BoostAdapter.hpp>
//#include <libslic3r/SLA/Contour3D.hpp>
#include <libslic3r/TriangleMeshSlicer.hpp>

#include "ConcaveHull.hpp"

#include "boost/log/trivial.hpp"
#include "ClipperUtils.hpp"
#include "Tesselate.hpp"
#include "MTUtils.hpp"

#include "TriangulateWall.hpp"

// 用于调试：
// #include <fstream>
// #include <libnest2d/tools/benchmark.h>
#include "SVG.hpp"

#include "I18N.hpp"
#include <boost/log/trivial.hpp>

//! macro used to mark string used at localization,
//! return same string
#define L(s) Slic3r::I18N::translate(s)

namespace Slic3r { namespace sla {

namespace {

indexed_triangle_set walls(
    const Polygon &lower,
    const Polygon &upper,
    double         lower_z_mm,
    double         upper_z_mm)
{
    indexed_triangle_set w;
    triangulate_wall(w.vertices, w.indices, lower, upper, lower_z_mm,
                     upper_z_mm);
    
    return w;
}

// 与 walls() 相同，但上下多边形相同。
inline indexed_triangle_set straight_walls(const Polygon &plate,
                                           double         lo_z,
                                           double         hi_z)
{
    return walls(plate, plate, lo_z, hi_z);
}

// 为给定多边形切割微小的连接器凹槽。输入多边形将按"padding"偏移，
// 并沿周长每隔"stride"距离插入小矩形凹槽。
// 杆状矩形的宽度约为"stick_width"。输入尺寸为世界坐标测量值，
// 而非缩放后的 clipper 单位。
void breakstick_holes(Points& pts,
                      double padding,
                      double stride,
                      double stick_width,
                      double penetration)
{
    if(stride <= EPSILON || stick_width <= EPSILON || padding <= EPSILON)
        return;

    // SVG svg("bridgestick_plate.svg");
    // svg.draw(poly);

    // 连接杆将是一个小矩形，尺寸为 stick_width x (penetration + padding)，
    // 以便对输入多边形有一定的穿透。

    Points out;
    out.reserve(2 * pts.size()); // 输出多边形点

    // 杆底部和右侧边缘尺寸
    double sbottom = scaled(stick_width);
    double sright  = scaled(penetration + padding);

    // 缩放后的步幅距离
    double sstride = scaled(stride);
    double t       = 0;

    // 将顶点对作为边处理，从最后一个和第一个点开始
    for (size_t i = pts.size() - 1, j = 0; j < pts.size(); i = j, ++j) {
        // 获取顶点和方向向量
        const Point &a = pts[i], &b = pts[j];
        Vec2d        dir = b.cast<double>() - a.cast<double>();
        double       nrm = dir.norm();
        dir /= nrm;
        Vec2d dirp(-dir(Y), dir(X));

        // 插入起始点
        out.emplace_back(a);

        // 避开起始点，不要在连接处制作杆
        while (t < sbottom) t += sbottom;
        double tend = nrm - sbottom;

        while (t < tend) { // 在多边形周长上插入杆

            // 计算杆矩形顶点并插入它们
            // into the output.
            Point p1 = a + (t * dir).cast<coord_t>();
            Point p2 = p1 + (sright * dirp).cast<coord_t>();
            Point p3 = p2 + (sbottom * dir).cast<coord_t>();
            Point p4 = p3 + (sright * -dirp).cast<coord_t>();
            out.insert(out.end(), {p1, p2, p3, p4});

            // 沿周长继续
            t += sstride;
        }

        t = t - nrm;

        // 插入边端点
        out.emplace_back(b);
    }

    // move the new points
    out.shrink_to_fit();
    pts.swap(out);
}

template<class...Args>
ExPolygons breakstick_holes(const ExPolygons &input, Args...args)
{
    ExPolygons ret = input;
    for (ExPolygon &p : ret) {
        breakstick_holes(p.contour.points, args...);
        for (auto &h : p.holes) breakstick_holes(h.points, args...);
    }

    return ret;
}

static inline coord_t get_waffle_offset(const PadConfig &c)
{
    return scaled(c.brim_size_mm + c.wing_distance());
}

static inline double get_merge_distance(const PadConfig &c)
{
    return 2. * (1.8 * c.wall_thickness_mm) + c.max_merge_dist_mm;
}

// 用于 3D 几何体生成的垫配置部分
struct PadConfig3D {
    double thickness, height, wing_height, slope;

    explicit PadConfig3D(const PadConfig &cfg2d)
        : thickness{cfg2d.wall_thickness_mm}
        , height{cfg2d.full_height()}
        , wing_height{cfg2d.wall_height_mm}
        , slope{cfg2d.wall_slope}
    {}

    inline double bottom_offset() const
    {
        return (thickness + wing_height) / std::tan(slope);
    }
};

// 骨架的外部部分用于生成垫的华夫格边。
// 内部部分不会被华夫格化或偏移。内部部分仅在垫围绕对象生成时使用，
// 对应于模型蓝图中的孔和内多边形。
struct PadSkeleton { ExPolygons inner, outer; };

PadSkeleton divide_blueprint(const ExPolygons &bp)
{
    ClipperLib::PolyTree ptree = union_pt(bp);

    PadSkeleton ret;
    ret.inner.reserve(size_t(ptree.Total()));
    ret.outer.reserve(size_t(ptree.Total()));

    for (ClipperLib::PolyTree::PolyNode *node : ptree.Childs) {
        ExPolygon poly;
        poly.contour.points = std::move(node->Contour);
        for (ClipperLib::PolyTree::PolyNode *child : node->Childs) {
            poly.holes.emplace_back(std::move(child->Contour));

            traverse_pt(child->Childs, &ret.inner);
        }

        ret.outer.emplace_back(poly);
    }
    
    return ret;
}

// 用于存储多边形并维护其边界框空间索引的辅助类。
class Intersector {
    BoxIndex       m_index;
    ExPolygons     m_polys;

public:

    // 向索引添加新多边形
    void add(const ExPolygon &ep)
    {
        m_polys.emplace_back(ep);
        m_index.insert(get_extents(ep), unsigned(m_index.size()));
    }

    // 检查任意多边形与索引多边形的相交情况
    bool intersects(const ExPolygon &poly)
    {
        // 创建合适的查询边界框。
        auto bb = poly.contour.bounding_box();

        std::vector<BoxIndexEl> qres = m_index.query(bb, BoxIndex::qtIntersects);

        // 现在检查实际多边形上的相交（不仅仅是边界框）
        bool is_overlap = false;
        auto qit        = qres.begin();
        while (!is_overlap && qit != qres.end())
            is_overlap = is_overlap || poly.overlaps(m_polys[(qit++)->second]);

        return is_overlap;
    }
};

// 此虚拟交叉器用于实现"强制处处垫"功能
struct DummyIntersector
{
    inline void add(const ExPolygon &) {}
    inline bool intersects(const ExPolygon &) { return true; }
};

template<class _Intersector>
class _AroundPadSkeleton : public PadSkeleton
{
    // 一个空间索引，用于高效找到支撑多边形与模型多边形的相交。
    _Intersector m_intersector;

public:
    _AroundPadSkeleton(const ExPolygons &support_blueprint,
                       const ExPolygons &model_blueprint,
                       const PadConfig & cfg,
                       ThrowOnCancel     thr)
    {
        // 我们需要以特殊方式合并支撑和模型轮廓，
        // 其中模型轮廓必须从支撑轮廓中减去。
        // 垫必须有一个孔，使模型可以完美贴合（因此使用 diff_ex 进行相减）。
        // 此外，由于缺少支撑，必须从不需要垫的区域中消除垫。

        add_supports_to_index(support_blueprint);

        auto model_bp_offs =
            offset_ex(model_blueprint,
                      scaled<float>(cfg.embed_object.object_gap_mm),
                      ClipperLib::jtMiter, 1);

        ExPolygons fullcvh =
            wafflized_concave_hull(support_blueprint, model_bp_offs, cfg, thr);

        auto model_bp_sticks =
            breakstick_holes(model_bp_offs, cfg.embed_object.object_gap_mm,
                             cfg.embed_object.stick_stride_mm,
                             cfg.embed_object.stick_width_mm,
                             cfg.embed_object.stick_penetration_mm);

        ExPolygons fullpad = diff_ex(fullcvh, model_bp_sticks);

        PadSkeleton divided = divide_blueprint(fullpad);
        
        remove_redundant_parts(divided.outer);
        remove_redundant_parts(divided.inner);

        outer = std::move(divided.outer);
        inner = std::move(divided.inner);
    }

private:

    // 将支撑蓝图添加到搜索索引中，以便稍后查询
    void add_supports_to_index(const ExPolygons &supp_bp)
    {
        for (auto &ep : supp_bp) m_intersector.add(ep);
    }

    // 在场景中的所有对象周围创建华夫格垫。此垫还没有任何孔。
    ExPolygons wafflized_concave_hull(const ExPolygons &supp_bp,
                                       const ExPolygons &model_bp,
                                       const PadConfig  &cfg,
                                       ThrowOnCancel     thr)
    {
        auto allin = reserve_vector<ExPolygon>(supp_bp.size() + model_bp.size());

        for (auto &ep : supp_bp) allin.emplace_back(ep.contour);
        for (auto &ep : model_bp) allin.emplace_back(ep.contour);

        ConcaveHull cchull{allin, get_merge_distance(cfg), thr};
        return offset_waffle_style_ex(cchull, get_waffle_offset(cfg));
    }

    // 移除不承载任何支撑的垫骨架部分
    void remove_redundant_parts(ExPolygons &parts)
    {
        auto endit = std::remove_if(parts.begin(), parts.end(),
                                    [this](const ExPolygon &p) {
                                        return !m_intersector.intersects(p);
                                    });

        parts.erase(endit, parts.end());
    }
};

using AroundPadSkeleton = _AroundPadSkeleton<Intersector>;
using BrimPadSkeleton   = _AroundPadSkeleton<DummyIntersector>;

class BelowPadSkeleton : public PadSkeleton
{
public:
    BelowPadSkeleton(const ExPolygons &support_blueprint,
                     const ExPolygons &model_blueprint,
                     const PadConfig & cfg,
                     ThrowOnCancel     thr)
    {
        outer.reserve(support_blueprint.size() + model_blueprint.size());

        for (auto &ep : support_blueprint) outer.emplace_back(ep.contour);
        for (auto &ep : model_blueprint) outer.emplace_back(ep.contour);

        ConcaveHull ochull{outer, get_merge_distance(cfg), thr};

        outer = offset_waffle_style_ex(ochull, get_waffle_offset(cfg));
    }
};

// 仅偏移轮廓，保持孔洞不变
template<class...Args>
ExPolygon offset_contour_only(const ExPolygon &poly, coord_t delta, Args...args)
{
    Polygons tmp = offset(poly.contour, float(delta), args...);

    if (tmp.empty()) return {};

    Polygons holes = poly.holes;
    for (auto &h : holes) h.reverse();

    ExPolygons tmp2 = diff_ex(tmp, holes);

    if (tmp2.empty()) return {};

    return std::move(tmp2.front());
}

bool add_cavity(indexed_triangle_set &pad,
                ExPolygon &           top_poly,
                const PadConfig3D &   cfg,
                ThrowOnCancel         thr)
{
    auto logerr = []{BOOST_LOG_TRIVIAL(error)<<"Could not create pad cavity";};

    double    wing_distance = cfg.wing_height / std::tan(cfg.slope);
    coord_t   delta_inner   = -scaled(cfg.thickness + wing_distance);
    coord_t   delta_middle  = -scaled(cfg.thickness);
    ExPolygon inner_base    = offset_contour_only(top_poly, delta_inner);
    ExPolygon middle_base   = offset_contour_only(top_poly, delta_middle);

    if (inner_base.empty() || middle_base.empty()) { logerr(); return false; }

    ExPolygons pdiff = diff_ex(top_poly, middle_base.contour);

    if (pdiff.size() != 1) { logerr(); return false; }

    top_poly = pdiff.front();

    double z_min = -cfg.wing_height, z_max = 0;
    its_merge(pad, walls(inner_base.contour, middle_base.contour, z_min, z_max));
    thr();
    its_merge(pad, triangulate_expolygon_3d(inner_base, z_min, NORMALS_UP));

    return true;
}

indexed_triangle_set create_outer_pad_geometry(const ExPolygons & skeleton,
                                               const PadConfig3D &cfg,
                                               ThrowOnCancel      thr)
{
    indexed_triangle_set ret;

    for (const ExPolygon &pad_part : skeleton) {
        ExPolygon top_poly{pad_part};
        ExPolygon bottom_poly =
            offset_contour_only(pad_part, -scaled(cfg.bottom_offset()));

        if (bottom_poly.empty()) continue;
        thr();
        
        double z_min = -cfg.height, z_max = 0;
        its_merge(ret, walls(top_poly.contour, bottom_poly.contour, z_max, z_min));

        if (cfg.wing_height > 0. && add_cavity(ret, top_poly, cfg, thr))
            z_max = -cfg.wing_height;

        for (auto &h : bottom_poly.holes)
            its_merge(ret, straight_walls(h, z_max, z_min));
        
        its_merge(ret, triangulate_expolygon_3d(bottom_poly, z_min, NORMALS_DOWN));
        its_merge(ret, triangulate_expolygon_3d(top_poly, NORMALS_UP));
    }

    return ret;
}

indexed_triangle_set create_inner_pad_geometry(const ExPolygons & skeleton,
                                               const PadConfig3D &cfg,
                                               ThrowOnCancel      thr)
{
    indexed_triangle_set ret;

    double z_max = 0., z_min = -cfg.height;
    for (const ExPolygon &pad_part : skeleton) {
        thr();
        its_merge(ret, straight_walls(pad_part.contour, z_max, z_min));

        for (auto &h : pad_part.holes)
            its_merge(ret, straight_walls(h, z_max, z_min));
    
        its_merge(ret, triangulate_expolygon_3d(pad_part, z_min, NORMALS_DOWN));
        its_merge(ret, triangulate_expolygon_3d(pad_part, z_max, NORMALS_UP));
    }

    return ret;
}

indexed_triangle_set create_pad_geometry(const PadSkeleton &skelet,
                                         const PadConfig &  cfg,
                                         ThrowOnCancel      thr)
{
#ifndef NDEBUG
    SVG svg("pad_skeleton.svg");
    svg.draw(skelet.outer, "green");
    svg.draw(skelet.inner, "blue");
    svg.Close();
#endif

    PadConfig3D cfg3d(cfg);
    auto pg = create_outer_pad_geometry(skelet.outer, cfg3d, thr);
    its_merge(pg, create_inner_pad_geometry(skelet.inner, cfg3d, thr));

    return pg;
}

indexed_triangle_set create_pad_geometry(const ExPolygons &supp_bp,
                                         const ExPolygons &model_bp,
                                         const PadConfig & cfg,
                                         ThrowOnCancel     thr)
{
    PadSkeleton skelet;

    if (cfg.embed_object.enabled) {
        if (cfg.embed_object.everywhere)
            skelet = BrimPadSkeleton(supp_bp, model_bp, cfg, thr);
        else
            skelet = AroundPadSkeleton(supp_bp, model_bp, cfg, thr);
    } else
        skelet = BelowPadSkeleton(supp_bp, model_bp, cfg, thr);

    return create_pad_geometry(skelet, cfg, thr);
}

} // namespace

void pad_blueprint(const indexed_triangle_set &mesh,
                   ExPolygons &                output,
                   const std::vector<float> &  heights,
                   ThrowOnCancel               thrfn)
{
    if (mesh.empty()) return;

    std::vector<ExPolygons> out = slice_mesh_ex(mesh, heights, thrfn);

    size_t count = 0;
    for(auto& o : out) count += o.size();

    // 合并操作代价高昂，简化操作也能加速垫的生成
    auto tmp = reserve_vector<ExPolygon>(count);
    for(ExPolygons& o : out)
        for(ExPolygon& e : o) {
            auto&& exss = e.simplify(scaled<double>(0.1));
            for(ExPolygon& ep : exss) tmp.emplace_back(std::move(ep));
        }

    ExPolygons utmp = union_ex(tmp);

    for(auto& o : utmp) {
        auto&& smp = o.simplify(scaled<double>(0.1));
        output.insert(output.end(), smp.begin(), smp.end());
    }
}

void pad_blueprint(const indexed_triangle_set &mesh,
                   ExPolygons &                output,
                   float                       h,
                   float                       layerh,
                   ThrowOnCancel               thrfn)
{
    float gnd = float(bounding_box(mesh).min(Z));

    std::vector<float> slicegrid = grid(gnd, gnd + h, layerh);
    pad_blueprint(mesh, output, slicegrid, thrfn);
}

void create_pad(const ExPolygons &    sup_blueprint,
                const ExPolygons &    model_blueprint,
                indexed_triangle_set &out,
                const PadConfig &     cfg,
                ThrowOnCancel         thr)
{
    auto t = create_pad_geometry(sup_blueprint, model_blueprint, cfg, thr);
    its_merge(out, t);
}

std::string PadConfig::validate() const
{
    static const double constexpr MIN_BRIM_SIZE_MM = .1;

    if (brim_size_mm < MIN_BRIM_SIZE_MM ||
        bottom_offset() > brim_size_mm + wing_distance() ||
        get_waffle_offset(*this) <= MIN_BRIM_SIZE_MM)
        return L("Pad brim size is too small for the current configuration.");

    return "";
}

}} // namespace Slic3r::sla
