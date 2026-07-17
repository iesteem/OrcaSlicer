#ifndef OPENVDBUTILS_HPP
#define OPENVDBUTILS_HPP

#include <libslic3r/TriangleMesh.hpp>

#ifdef _MSC_VER
// 抑制 include/gmp.h(2177,31) 中的警告 C4146：对无符号类型应用一元负号运算符，结果仍为无符号
#pragma warning(push)
#pragma warning(disable : 4146)
#endif // _MSC_VER
#include <openvdb/openvdb.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

namespace Slic3r {

inline Vec3f to_vec3f(const openvdb::Vec3s &v) { return Vec3f{v.x(), v.y(), v.z()}; }
inline Vec3d to_vec3d(const openvdb::Vec3s &v) { return to_vec3f(v).cast<double>(); }
inline Vec3i32 to_vec3i(const openvdb::Vec3I &v) { return Vec3i32{int(v[0]), int(v[1]), int(v[2])}; }

// 这里 voxel_scale 定义影响体素计数的体素缩放比例。
// 1.0 值表示每个单位立方体一个体素。2 表示模型缩放为 2 倍大，
// 体素计数按缩放体积的增量增加，因此为 4 倍。
// 这种采样精度选择无法通过 Transform 参数实现。（TODO：或者可以吗？）
// 生成的网格将在其元数据中包含 voxel_scale，
// 位于 "voxel_scale" 键下，供 grid_to_mesh 函数使用。
openvdb::FloatGrid::Ptr mesh_to_grid(const indexed_triangle_set &    mesh,
                                     const openvdb::math::Transform &tr = {},
                                     float voxel_scale                  = 1.f,
                                     float exteriorBandWidth = 3.0f,
                                     float interiorBandWidth = 3.0f,
                                     int   flags             = 0);

indexed_triangle_set grid_to_mesh(const openvdb::FloatGrid &grid,
                                  double                    isovalue   = 0.0,
                                  double                    adaptivity = 0.0,
                                  bool relaxDisorientedTriangles = true);

openvdb::FloatGrid::Ptr redistance_grid(const openvdb::FloatGrid &grid,
                                        double                    iso,
                                        double ext_range = 3.,
                                        double int_range = 3.);

} // namespace Slic3r

#endif // OPENVDBUTILS_HPP
