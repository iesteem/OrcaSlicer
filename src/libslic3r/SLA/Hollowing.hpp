#ifndef SLA_HOLLOWING_HPP
#define SLA_HOLLOWING_HPP

#include <memory>
#include <libslic3r/TriangleMesh.hpp>
#include <libslic3r/SLA/JobController.hpp>

namespace Slic3r {

namespace sla {

struct HollowingConfig
{
    double min_thickness    = 2.;
    double quality          = 0.5;
    double closing_distance = 0.5;
    bool enabled = true;
};

enum HollowingFlags { hfRemoveInsideTriangles = 0x1 };

// 与生成的网格内部相关的所有数据。包括3D网格、网格及各种元数据。无需从外部操作。
struct Interior;
struct InteriorDeleter { void operator()(Interior *p); };
using  InteriorPtr = std::unique_ptr<Interior, InteriorDeleter>;

indexed_triangle_set &      get_mesh(Interior &interior);
const indexed_triangle_set &get_mesh(const Interior &interior);

struct DrainHole
{
    Vec3f pos;
    Vec3f normal;
    float radius;
    float height;
    bool  failed = false;

    DrainHole()
        : pos(Vec3f::Zero()), normal(Vec3f::UnitZ()), radius(5.f), height(10.f)
    {}

    DrainHole(Vec3f p, Vec3f n, float r, float h, bool fl = false)
        : pos(p), normal(n), radius(r), height(h), failed(fl)
    {}

    DrainHole(const DrainHole& rhs) :
        DrainHole(rhs.pos, rhs.normal, rhs.radius, rhs.height, rhs.failed) {}
    
    bool operator==(const DrainHole &sp) const;
    
    bool operator!=(const DrainHole &sp) const { return !(sp == (*this)); }

    bool is_inside(const Vec3f& pt) const;

    bool get_intersections(const Vec3f& s, const Vec3f& dir,
                           std::array<std::pair<float, Vec3d>, 2>& out) const;
    
    indexed_triangle_set to_mesh() const;
    
    template<class Archive> inline void serialize(Archive &ar)
    {
        ar(pos, normal, radius, height, failed);
    }

    static constexpr size_t steps = 32;
};

using DrainHoles = std::vector<DrainHole>;

constexpr float HoleStickOutLength = 1.f;

InteriorPtr generate_interior(const TriangleMesh &mesh,
                              const HollowingConfig &  = {},
                              const JobController &ctl = {});

// 执行抽壳操作
void hollow_mesh(TriangleMesh &mesh, const HollowingConfig &cfg, int flags = 0);

// 在"interior"中准备好的抽壳，与原始网格合并
void hollow_mesh(TriangleMesh &mesh, const Interior &interior, int flags = 0);

void remove_inside_triangles(TriangleMesh &mesh, const Interior &interior,
                             const std::vector<bool> &exclude_mask = {});

double get_distance(const Vec3f &p, const Interior &interior);

template<class T>
FloatingOnly<T> get_distance(const Vec<3, T> &p, const Interior &interior)
{
    return get_distance(Vec3f(p.template cast<float>()), interior);
}

void cut_drainholes(std::vector<ExPolygons> & obj_slices,
                    const std::vector<float> &slicegrid,
                    float                     closing_radius,
                    const sla::DrainHoles &   holes,
                    std::function<void(void)> thr);

inline void swap_normals(indexed_triangle_set &its)
{
    for (auto &face : its.indices)
        std::swap(face(0), face(2));
}

} // namespace sla
} // namespace Slic3r

#endif // HOLLOWINGFILTER_H
