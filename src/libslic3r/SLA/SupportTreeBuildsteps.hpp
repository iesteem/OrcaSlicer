#ifndef SLASUPPORTTREEALGORITHM_H
#define SLASUPPORTTREEALGORITHM_H

#include <cstdint>
#include <optional>

#include <libslic3r/SLA/SupportTreeBuilder.hpp>
#include <libslic3r/SLA/Clustering.hpp>
#include <libslic3r/SLA/SpatIndex.hpp>

namespace Slic3r {
namespace sla {

// 两个支撑点保持有效的最小距离。
const double /*constexpr*/ D_SP = 0.1;

enum { // For indexing Eigen vectors as v(X), v(Y), v(Z) instead of numbers
    X, Y, Z
};

inline Vec2d to_vec2(const Vec3d &v3) { return {v3(X), v3(Y)}; }

inline std::pair<double, double> dir_to_spheric(const Vec3d &n, double norm = 1.)
{
    double z       = n.z();
    double r       = norm;
    double polar   = std::acos(z / r);
    double azimuth = std::atan2(n(1), n(0));
    return {polar, azimuth};
}

inline Vec3d spheric_to_dir(double polar, double azimuth)
{
    return {std::cos(azimuth) * std::sin(polar),
            std::sin(azimuth) * std::sin(polar), std::cos(polar)};
}

inline Vec3d spheric_to_dir(const std::tuple<double, double> &v)
{
    auto [plr, azm] = v;
    return spheric_to_dir(plr, azm);
}

inline Vec3d spheric_to_dir(const std::pair<double, double> &v)
{
    return spheric_to_dir(v.first, v.second);
}

inline Vec3d spheric_to_dir(const std::array<double, 2> &v)
{
    return spheric_to_dir(v[0], v[1]);
}

// Give points on a 3D ring with given center, radius and orientation
// method based on:
// https://math.stackexchange.com/questions/73237/parametric-equation-of-a-circle-in-3d-space
template<size_t N>
class PointRing {
    std::array<double, N> m_phis;

    // Two vectors that will be perpendicular to each other and to the
    // axis. Values for a(X) and a(Y) are now arbitrary, a(Z) is just a
    // placeholder.
    // a and b vectors are perpendicular to the ring direction and to each other.
    // Together they define the plane where we have to iterate with the
    // given angles in the 'm_phis' vector
    Vec3d a = {0, 1, 0}, b;
    double m_radius = 0.;

    static inline bool constexpr is_one(double val)
    {
        return std::abs(std::abs(val) - 1) < 1e-20;
    }

public:

    PointRing(const Vec3d &n)
    {
        m_phis = linspace_array<N>(0., 2 * PI);

        // We have to address the case when the direction vector v (same as
        // dir) is coincident with one of the world axes. In this case two of
        // its components will be completely zero and one is 1.0. Our method
        // becomes dangerous here due to division with zero. Instead, vector
        // 'a' can be an element-wise rotated version of 'v'
        if(is_one(n(X)) || is_one(n(Y)) || is_one(n(Z))) {
            a = {n(Z), n(X), n(Y)};
            b = {n(Y), n(Z), n(X)};
        }
        else {
            a(Z) = -(n(Y)*a(Y)) / n(Z); a.normalize();
            b = a.cross(n);
        }
    }

    Vec3d get(size_t idx, const Vec3d src, double r) const
    {
        double phi = m_phis[idx];
        double sinphi = std::sin(phi);
        double cosphi = std::cos(phi);

        double rpscos = r * cosphi;
        double rpssin = r * sinphi;

        // Point on the sphere
        return {src(X) + rpscos * a(X) + rpssin * b(X),
                src(Y) + rpscos * a(Y) + rpssin * b(Y),
                src(Z) + rpscos * a(Z) + rpssin * b(Z)};
    }
};

//IndexedMesh::hit_result query_hit(const SupportableMesh &msh, const Bridge &br, double safety_d = std::nan(""));
//IndexedMesh::hit_result query_hit(const SupportableMesh &msh, const Head &br, double safety_d = std::nan(""));

inline Vec3d dirv(const Vec3d& startp, const Vec3d& endp) {
    return (endp - startp).normalized();
}

class PillarIndex {
    PointIndex m_index;
    using Mutex = ccr::BlockingMutex;
    mutable Mutex m_mutex;

public:

    template<class...Args> inline void guarded_insert(Args&&...args)
    {
        std::lock_guard<Mutex> lck(m_mutex);
        m_index.insert(std::forward<Args>(args)...);
    }

    template<class...Args>
    inline std::vector<PointIndexEl> guarded_query(Args&&...args) const
    {
        std::lock_guard<Mutex> lck(m_mutex);
        return m_index.query(std::forward<Args>(args)...);
    }

    template<class...Args> inline void insert(Args&&...args)
    {
        m_index.insert(std::forward<Args>(args)...);
    }

    template<class...Args>
    inline std::vector<PointIndexEl> query(Args&&...args) const
    {
        return m_index.query(std::forward<Args>(args)...);
    }

    template<class Fn> inline void foreach(Fn fn) { m_index.foreach(fn); }
    template<class Fn> inline void guarded_foreach(Fn fn)
    {
        std::lock_guard<Mutex> lck(m_mutex);
        m_index.foreach(fn);
    }

    PointIndex guarded_clone()
    {
        std::lock_guard<Mutex> lck(m_mutex);
        return m_index;
    }
};

// Helper function for pillar interconnection where pairs of already connected
// pillars should be checked for not to be processed again. This can be done
// in constant time with a set of hash values uniquely representing a pair of
// integers. The order of numbers within the pair should not matter, it has
// the same unique hash. The hash value has to have twice as many bits as the
// arguments need. If the same integral type is used for args and return val,
// make sure the arguments use only the half of the type's bit depth.
template<class I, class DoubleI = IntegerOnly<I>>
IntegerOnly<DoubleI> pairhash(I a, I b)
{
    using std::ceil; using std::log2; using std::max; using std::min;
    static const auto constexpr Ibits = int(sizeof(I) * CHAR_BIT);
    static const auto constexpr DoubleIbits = int(sizeof(DoubleI) * CHAR_BIT);
    static const auto constexpr shift = DoubleIbits / 2 < Ibits ? Ibits / 2 : Ibits;

    I g = min(a, b), l = max(a, b);

    // Assume the hash will fit into the output variable
    assert((g ? (ceil(log2(g))) : 0) <= shift);
    assert((l ? (ceil(log2(l))) : 0) <= shift);

    return (DoubleI(g) << shift) + l;
}

class SupportTreeBuildsteps {
    const SupportTreeConfig& m_cfg;
    const IndexedMesh& m_mesh;
    const std::vector<SupportPoint>& m_support_pts;

    using PtIndices = std::vector<unsigned>;

    PtIndices m_iheads;            // support points with pinhead
    PtIndices m_iheads_onmodel;
    PtIndices m_iheadless;         // headless support points
    
    std::map<unsigned, IndexedMesh::hit_result> m_head_to_ground_scans;

    // normals for support points from model faces.
    PointSet  m_support_nmls;

    // Clusters of points which can reach the ground directly and can be
    // bridged to one central pillar
    std::vector<PtIndices> m_pillar_clusters;

    // This algorithm uses the SupportTreeBuilder class to fill gradually
    // the support elements (heads, pillars, bridges, ...)
    SupportTreeBuilder& m_builder;

    // support points in Eigen/IGL format
    PointSet m_points;

    // throw if canceled: It will be called many times so a shorthand will
    // come in handy.
    ThrowOnCancel m_thr;

    // A spatial index to easily find strong pillars to connect to.
    PillarIndex m_pillar_index;

    // When bridging heads to pillars... TODO: find a cleaner solution
    ccr::BlockingMutex m_bridge_mutex;

    inline IndexedMesh::hit_result ray_mesh_intersect(const Vec3d& s, 
                                                      const Vec3d& dir)
    {
        return m_mesh.query_ray_hit(s, dir);
    }

    // 此函数将测试未来的钉头是否会与模型几何体碰撞。
    // 它不接收 'Head' 对象，因为那些是在此测试之后创建的。
    // 参数：s: 模型表面上的接触点。dir: 头部从钉尖到背部的方向。
    // r_pin, r_back: 钉尖和背部球体的半径。width: 从钉尖中心到背部中心的完整宽度。
    // m: 对象网格。
    // 返回值是射线投射的结果。如果起点在模型内部，
    // 将返回一个距离值为零的"无效" hit_result，而不是 NAN。
    // 这样结果可以安全地用于与其他距离进行比较。
    IndexedMesh::hit_result pinhead_mesh_intersect(
        const Vec3d& s,
        const Vec3d& dir,
        double r_pin,
        double r_back,
        double width,
        double safety_d);

    IndexedMesh::hit_result pinhead_mesh_intersect(
        const Vec3d& s,
        const Vec3d& dir,
        double r_pin,
        double r_back,
        double width)
    {
        return pinhead_mesh_intersect(s, dir, r_pin, r_back, width,
                                      r_back * m_cfg.safety_distance_mm /
                                          m_cfg.head_back_radius_mm);
    }

    // 检查桥（以及柱和杆）与模型的相交情况。
    // 如果函数用于无头杆，ins_check 参数必须为 true，
    // 因为杆的起始点可能在模型几何体内部。
    // 返回值是射线投射的结果。如果起点在模型内部，
    // 将返回一个距离值为零的"无效" hit_result，而不是 NAN。
    // 这样结果可以安全地用于与其他距离进行比较。
    IndexedMesh::hit_result bridge_mesh_intersect(
        const Vec3d& s,
        const Vec3d& dir,
        double r,
        double safety_d);

    IndexedMesh::hit_result bridge_mesh_intersect(
        const Vec3d& s,
        const Vec3d& dir,
        double r)
    {
        return bridge_mesh_intersect(s, dir, r,
                                     r * m_cfg.safety_distance_mm /
                                         m_cfg.head_back_radius_mm);
    }
    
    template<class...Args>
    inline double bridge_mesh_distance(Args&&...args) {
        return bridge_mesh_intersect(std::forward<Args>(args)...).distance();
    }

    // 辅助函数，用于用锯齿形桥连接两个柱。
    bool interconnect(const Pillar& pillar, const Pillar& nextpillar);

    // 用于将头连接到附近的柱。
    bool connect_to_nearpillar(const Head& head, long nearpillar_id);
    
    // 找到头到地面的路径。如果无法直接创建柱，
    // 则从头到柱插入额外的桥。
    // 可选的 dir 参数是桥的方向，如果省略则为钉头的方向。
    bool connect_to_ground(Head& head, const Vec3d &dir);
    inline bool connect_to_ground(Head& head);
    
    bool connect_to_model_body(Head &head);

    bool search_pillar_and_connect(const Head& source);
    
    // 这是一个用于创建柱的代理函数，在零抬高模式下会考虑
    // 垫与模型底部之间的间隙。
    // jp 是起始连接点，需要向下路由。
    // sourcedir 是 jp 连接点与最终柱之间可选桥的允许方向。
    bool create_ground_pillar(const Vec3d &jp,
                              const Vec3d &sourcedir,
                              double       radius,
                              long         head_id = SupportTreeNode::ID_UNSET);

    void add_pillar_base(long pid)
    {
        m_builder.add_pillar_base(pid, m_cfg.base_height_mm, m_cfg.base_radius_mm);
    }

    std::optional<DiffBridge> search_widening_path(const Vec3d &jp,
                                                   const Vec3d &dir,
                                                   double       radius,
                                                   double       new_radius);

public:
    SupportTreeBuildsteps(SupportTreeBuilder & builder, const SupportableMesh &sm);

    // 现在定义支撑生成算法的各个步骤

    // 过滤步骤：这里我们将丢弃不合适的支撑点，
    // 并决定合适支撑点的后续处理。我们将检查钉头是否适用，
    // 并在每个支撑点调整其角度。
    // 我们还将合并那些距离过近、可视为一个的支撑点。
    void filter();

    // 钉头创建：根据过滤结果，将构建 Head 对象（以及它们的三角形网格）。
    void add_pinheads();

    // 对带有钉头的支撑点进行进一步分类。如果通过平行于 Z 轴的垂直线
    // 可以直接到达地面，我们将支撑点视为柱候选。
    // 如果接触到模型几何体，它将被标记为非地面朝向，
    // 后续步骤将处理它。此外，柱将被分组为可用桥互连的聚类。
    // 这些组的元素可能互连，也可能不互连。这里我们只运行
    // 聚类算法。
    void classify();

    // 步骤：路由地面连接的钉头，并使用额外的（倾斜）桥将它们互连。
    // 并非所有这些钉头都会成为完整的柱（地面连接）。
    // 有些将使用桥连接到附近的柱。中心柱的此类侧头
    // 的最大数量是有限的，以避免不良的重量分布。
    void routing_to_ground();

    // 步骤：路由将沿 Z 轴向下连接到模型表面的钉头。
    // 目前这些将实际上使用翻转的钉头连接到模型表面。
    // 未来这里我们可以使用一些智能算法来搜索到地面或到
    // 可以承受支撑重量的附近柱的安全路径。
    void routing_to_model();

    void interconnect_pillars();

    inline void merge_result() { m_builder.merged_mesh(); }

    static bool execute(SupportTreeBuilder & builder, const SupportableMesh &sm);
};

}
}

#endif // SLASUPPORTTREEALGORITHM_H
