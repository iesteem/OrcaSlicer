#ifndef SLA_INDEXEDMESH_H
#define SLA_INDEXEDMESH_H

#include <memory>
#include <vector>

#include <libslic3r/Point.hpp>

// 有一个孔洞感知光线投射器的实现，最终未在生产版本中使用。
// 现在隐藏在以下 define 后面，以备将来可能使用。
// #define SLIC3R_HOLE_RAYCASTER

#ifdef SLIC3R_HOLE_RAYCASTER
  #include "libslic3r/SLA/Hollowing.hpp"
#endif

struct indexed_triangle_set;

namespace Slic3r {

class TriangleMesh;

namespace sla {

using PointSet = Eigen::MatrixXd;

/// 用于 libIGL 函数的索引三角形结构。也用作 SLASupportTree 的
/// 替代（原始）输入格式。
//  实现在 libslic3r/SLA/Common.cpp 中
class IndexedMesh {
    class AABBImpl;
    
    const indexed_triangle_set* m_tm;
    double m_ground_level = 0, m_gnd_offset = 0;
    
    std::unique_ptr<AABBImpl> m_aabb;

#ifdef SLIC3R_HOLE_RAYCASTER
    // 保存网格中孔洞的副本。由 load_mesh setter 在外部初始化。
    std::vector<DrainHole> m_holes;
#endif

    template<class M> void init(const M &mesh, bool calculate_epsilon);

public:
    
    // calculate_epsilon ... 根据平均三角形边长计算三角形-射线相交的 epsilon。
    // 如果设置为 false，则使用默认 epsilon，适用于"合理"的网格。
    explicit IndexedMesh(const indexed_triangle_set &tmesh, bool calculate_epsilon = false);
    explicit IndexedMesh(const TriangleMesh &mesh, bool calculate_epsilon = false);
    
    IndexedMesh(const IndexedMesh& other);
    IndexedMesh& operator=(const IndexedMesh&);
    
    IndexedMesh(IndexedMesh &&other);
    IndexedMesh& operator=(IndexedMesh &&other);
    
    ~IndexedMesh();
    
    inline double ground_level() const { return m_ground_level + m_gnd_offset; }
    inline void ground_level_offset(double o) { m_gnd_offset = o; }
    inline double ground_level_offset() const { return m_gnd_offset; }
    
    const std::vector<Vec3f>& vertices() const;
    const std::vector<Vec3i32>& indices()  const;
    const Vec3f& vertices(size_t idx) const;
    const Vec3i32& indices(size_t idx) const;
    
    // 射线投射的结果
    class hit_result {
        // m_t 保存从 m_source 到交点的距离。
        double m_t = infty();
        int m_face_id = -1;
        const IndexedMesh *m_mesh = nullptr;
        Vec3d m_dir;
        Vec3d m_source;
        Vec3d m_normal;
        friend class IndexedMesh;

        // 此类的有效对象只能通过 IndexedMesh::query_ray_hit 方法获取。
        explicit inline hit_result(const IndexedMesh& em): m_mesh(&em) {}
    public:
        // 表示在网格上没有命中。
        static inline constexpr double infty() { return std::numeric_limits<double>::infinity(); }
        
        explicit inline hit_result(double val = infty()) : m_t(val) {}
        
        inline double distance() const { return m_t; }
        inline const Vec3d& direction() const { return m_dir; }
        inline const Vec3d& source() const { return m_source; }
        inline Vec3d position() const { return m_source + m_dir * m_t; }
        inline int face() const { return m_face_id; }
        inline bool is_valid() const { return m_mesh != nullptr; }
        inline bool is_hit() const { return m_face_id >= 0 && !std::isinf(m_t); }

        inline const Vec3d& normal() const {
            assert(is_valid());
            return m_normal;
        }

        inline bool is_inside() const {
            return is_hit() && normal().dot(m_dir) > 0;
        }
    };

#ifdef SLIC3R_HOLE_RAYCASTER
    // 告知对象孔洞的位置
    // 创建向量的内部副本
    void load_holes(const std::vector<DrainHole>& holes) {
        m_holes = holes;
    }

    // 遍历命中和孔洞，返回真正的命中点，
    // 可能在孔洞内部。
    // 此函数目前未在任何地方使用，它是在孔洞在切片上相减时编写的，
    // 即在我们开始使用 CGAL 实际将孔洞切割到网格中之前。
    hit_result filter_hits(const std::vector<IndexedMesh::hit_result>& obj_hits) const;
#endif

    // 在网格上投射射线，返回命中发生的距离。
    hit_result query_ray_hit(const Vec3d &s, const Vec3d &dir) const;

    // 在网格上投射射线并返回所有命中
    std::vector<hit_result> query_ray_hits(const Vec3d &s, const Vec3d &dir) const;

    double squared_distance(const Vec3d& p, int& i, Vec3d& c) const;
    inline double squared_distance(const Vec3d &p) const
    {
        int   i;
        Vec3d c;
        return squared_distance(p, i, c);
    }

    Vec3d normal_by_face_id(int face_id) const;

    const indexed_triangle_set * get_triangle_mesh() const { return m_tm; }
};

// 计算网格上选定点集（来自 'points' 集合）的法线。
// 这将为每个点调用平方距离函数。
PointSet normals(const PointSet& points,
    const IndexedMesh& convert_mesh,
    double eps = 0.05,  // min distance from edges
    std::function<void()> throw_on_cancel = [](){},
    const std::vector<unsigned>& selected_points = {});

}} // namespace Slic3r::sla

#endif // INDEXEDMESH_H
