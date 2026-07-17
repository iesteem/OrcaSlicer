#ifndef PRUSASLICER_AABBMESH_H
#define PRUSASLICER_AABBMESH_H

#include <memory>
#include <vector>

#include <libslic3r/Point.hpp>
#include <libslic3r/TriangleMesh.hpp>

// 有一个支持孔洞感知的光线投射器的实现，但最终未在生产版本中使用。
// 它现在隐藏在以下宏定义之后，供将来可能使用。
// #define SLIC3R_HOLE_RAYCASTER

#ifdef SLIC3R_HOLE_RAYCASTER
  #include "libslic3r/SLA/Hollowing.hpp"
#endif

struct indexed_triangle_set;

namespace Slic3r {

class TriangleMesh;

// 一个与 AABB 索引耦合的索引三角形结构，用于支持光线投射和其他高级操作。
class AABBMesh {
    class AABBImpl;

    const indexed_triangle_set* m_tm;

    std::unique_ptr<AABBImpl> m_aabb;
    VertexFaceIndex m_vfidx;    // 顶点-面索引
    std::vector<Vec3i32> m_fnidx; // 面-邻接面索引

#ifdef SLIC3R_HOLE_RAYCASTER
    // 保存网格中孔洞的副本。由 load_mesh 设置器在外部初始化。
    std::vector<sla::DrainHole> m_holes;
#endif

    template<class M> void init(const M &mesh, bool calculate_epsilon);

public:

    // calculate_epsilon ... 根据平均三角形边长计算用于三角形-光线相交的 epsilon。
    // 如果设置为 false，则使用默认 epsilon，适用于"合理"的网格。
    explicit AABBMesh(const indexed_triangle_set &tmesh, bool calculate_epsilon = false);
    explicit AABBMesh(const TriangleMesh &mesh, bool calculate_epsilon = false);
    
    AABBMesh(const AABBMesh& other);
    AABBMesh& operator=(const AABBMesh&);

    AABBMesh(AABBMesh &&other);
    AABBMesh& operator=(AABBMesh &&other);

    ~AABBMesh();

    const std::vector<Vec3f>& vertices() const;
    const std::vector<Vec3i32>& indices()  const;
    const Vec3f& vertices(size_t idx) const;
    const Vec3i32& indices(size_t idx) const;

    // 光线投射的结果
    class hit_result {
        // m_t 保存从 m_source 到交点的距离。
        double m_t = infty();
        int m_face_id = -1;
        const AABBMesh *m_mesh = nullptr;
        Vec3d m_dir    = Vec3d::Zero();
        Vec3d m_source = Vec3d::Zero();
        Vec3d m_normal = Vec3d::Zero();
        friend class AABBMesh;

        // 此类的有效对象只能通过 IndexedMesh::query_ray_hit 方法获得。
        explicit inline hit_result(const AABBMesh& em): m_mesh(&em) {}
    public:
        // 表示未命中网格。
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
    // 将孔洞位置信息告知对象
    // 创建向量的内部副本
    void load_holes(const std::vector<sla::DrainHole>& holes) {
        m_holes = holes;
    }

    // 遍历命中和孔洞，返回真实的命中点，可能
    // 在孔洞内部。
    // 此函数目前未在任何地方使用，它是在我们开始使用 CGAL
    // 实际切割网格上的孔洞之前编写的，当时孔洞是在切片上减去的。
    hit_result filter_hits(const std::vector<AABBMesh::hit_result>& obj_hits) const;
#endif

    // 在网格上投射光线，返回命中发生处的距离。
    hit_result query_ray_hit(const Vec3d &s, const Vec3d &dir) const;

    // 在网格上投射光线并返回所有命中点
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

    const VertexFaceIndex &vertex_face_index() const { return m_vfidx; }
    const std::vector<Vec3i32> &face_neighbor_index() const { return m_fnidx; }
};


} // namespace Slic3r::sla

#endif // INDEXEDMESH_H
