#define NOMINMAX
#include "OpenVDBUtils.hpp"

#ifdef _MSC_VER
// 抑制 OpenVDB 中的警告 C4146：对无符号类型应用一元负号运算符，结果仍为无符号
#pragma warning(push)
#pragma warning(disable : 4146)
#endif // _MSC_VER
#include <openvdb/tools/MeshToVolume.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Composite.h>
#include <openvdb/tools/LevelSetRebuild.h>

//#include "MTUtils.hpp"

namespace Slic3r {

class TriangleMeshDataAdapter {
public:
    const indexed_triangle_set &its;
    float voxel_scale;

    size_t polygonCount() const { return its.indices.size(); }
    size_t pointCount() const   { return its.vertices.size(); }
    size_t vertexCount(size_t) const { return 3; }

    // 返回多边形 n 和顶点 v 在局部网格索引空间中的位置 pos
    // 实际网格在 openvdb 中将显示为按 voxel_size 均匀缩放
    // 每单位体积的体素计数可以这样受影响。
    void getIndexSpacePoint(size_t n, size_t v, openvdb::Vec3d& pos) const
    {
        auto vidx = size_t(its.indices[n](Eigen::Index(v)));
        Slic3r::Vec3d p = its.vertices[vidx].cast<double>() * voxel_scale;
        pos = {p.x(), p.y(), p.z()};
    }

    TriangleMeshDataAdapter(const indexed_triangle_set &m, float voxel_sc = 1.f)
        : its{m}, voxel_scale{voxel_sc} {};
};

// TODO：我需要调用 initialize 吗？似乎没有它也能工作，但
// 文档说应该调用一次。即使之前已经调用过，
// 它也会执行互斥锁-解锁序列。
openvdb::FloatGrid::Ptr mesh_to_grid(const indexed_triangle_set &    mesh,
                                     const openvdb::math::Transform &tr,
                                     float voxel_scale,
                                     float exteriorBandWidth,
                                     float interiorBandWidth,
                                     int   flags)
{
    openvdb::initialize();

    std::vector<indexed_triangle_set> meshparts = its_split(mesh);

    auto it = std::remove_if(meshparts.begin(), meshparts.end(),
                             [](auto &m) { return its_volume(m) < EPSILON; });

    meshparts.erase(it, meshparts.end());

    openvdb::FloatGrid::Ptr grid;
    for (auto &m : meshparts) {
        auto subgrid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
            TriangleMeshDataAdapter{m, voxel_scale}, tr, exteriorBandWidth,
            interiorBandWidth, flags);

        if (grid && subgrid) openvdb::tools::csgUnion(*grid, *subgrid);
        else if (subgrid) grid = std::move(subgrid);
    }

    if (grid) {
        grid = openvdb::tools::levelSetRebuild(*grid, 0., exteriorBandWidth,
                                               interiorBandWidth);
    } else if(meshparts.empty()) {
        // Splitting failed, fall back to hollow the original mesh
        grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
            TriangleMeshDataAdapter{mesh}, tr, exteriorBandWidth,
            interiorBandWidth, flags);
    }

    grid->insertMeta("voxel_scale", openvdb::FloatMetadata(voxel_scale));

    return grid;
}

indexed_triangle_set grid_to_mesh(const openvdb::FloatGrid &grid,
                          double                    isovalue,
                          double                    adaptivity,
                          bool                      relaxDisorientedTriangles)
{
    openvdb::initialize();

    std::vector<openvdb::Vec3s> points;
    std::vector<openvdb::Vec3I> triangles;
    std::vector<openvdb::Vec4I> quads;

    openvdb::tools::volumeToMesh(grid, points, triangles, quads, isovalue,
                                 adaptivity, relaxDisorientedTriangles);

    float scale = 1.;
    try {
        scale = grid.template metaValue<float>("voxel_scale");
    }  catch (...) { }

    indexed_triangle_set ret;
    ret.vertices.reserve(points.size());
    ret.indices.reserve(triangles.size() + quads.size() * 2);

    for (auto &v : points) ret.vertices.emplace_back(to_vec3f(v) / scale);
    for (auto &v : triangles) ret.indices.emplace_back(to_vec3i(v));
    for (auto &quad : quads) {
        ret.indices.emplace_back(quad(0), quad(1), quad(2));
        ret.indices.emplace_back(quad(2), quad(3), quad(0));
    }

    return ret;
}

openvdb::FloatGrid::Ptr redistance_grid(const openvdb::FloatGrid &grid,
                                        double                    iso,
                                        double                    er,
                                        double                    ir)
{
    auto new_grid = openvdb::tools::levelSetRebuild(grid, float(iso),
                                                    float(er), float(ir));

    // Copies voxel_scale metadata, if it exists.
    new_grid->insertMeta(*grid.deepCopyMeta());

    return new_grid;
}

} // namespace Slic3r
