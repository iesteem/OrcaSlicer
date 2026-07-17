#ifndef slic3r_TriangleMeshSlicer_hpp_
#define slic3r_TriangleMeshSlicer_hpp_

#include <functional>
#include <vector>
#include "Polygon.hpp"
#include "ExPolygon.hpp"

namespace Slic3r {

struct MeshSlicingParams
{
    enum class SlicingMode : uint32_t {
        // 常规切片，保持所有轮廓及其方向。
        // slice_mesh_ex()对slice_mesh()的结果应用ClipperLib::pftNonZero规则。
        Regular,
        // 用于切片3DLabPrints平面模型（即与S3D默认策略兼容）。
        // slice_mesh_ex()应用ClipperLib::pftEvenOdd规则。slice_mesh()将EvenOdd作为Regular切片。
        EvenOdd,
        // 保持所有轮廓，将所有轮廓定向为逆时针。
        // slice_mesh_ex()应用ClipperLib::pftNonZero规则，因此孔洞将被闭合。
        Positive,
        // 将所有轮廓定向为逆时针，仅保留面积最大的轮廓。
        // 此模式在花瓶模式下切片复杂对象时很有用。
        PositiveLargestContour,
    };

    SlicingMode   mode { SlicingMode::Regular };
    // 用于花瓶模式：在此层以下将使用不同的切片模式来生成单个轮廓。
    // 0 = 忽略。
    size_t        slicing_mode_normal_below_layer { 0 };
    // 在slicing_mode_normal_below_layer以下应用的模式。如果slicing_mode_nromal_below_layer == 0则忽略。
    SlicingMode   mode_below { SlicingMode::Regular };
    // 切片过程中变换面。
    Transform3d   trafo { Transform3d::Identity() };
};

struct MeshSlicingParamsEx : public MeshSlicingParams
{
    // 创建输出expolygons时的形态学闭合操作，未缩放。
    float         closing_radius { 0 };
    // 创建输出expolygons时应用的正偏移，未缩放。
    float         extra_offset { 0 };
    // 轮廓简化的分辨率，未缩放。
    // 0 = 不简化。
    double        resolution { 0 };
};

// 以下所有切片函数应在相同网格、相同变换矩阵和切片参数下产生一致的结果。
// Namely, slice_mesh_slabs() shall produce consistent results with slice_mesh() and slice_mesh_ex() in the sense, that projections made by 
// slice_mesh_slabs() shall fall onto slicing planes produced by slice_mesh().
//
// 如果切片平面对网格的水平面进行精确切片，
// 朝上的水平面被认为在切片平面上，
// 而朝下的水平面被认为不在切片平面上。
// 
// slice_mesh_slabs()因此将朝上的水平切片投影到切片平面，
// 而slice_mesh_slabs()将朝下的水平切片投影到其上方的切片平面（如果存在）。

std::vector<Polygons>           slice_mesh(
    const indexed_triangle_set       &mesh,
    const std::vector<float>         &zs,
    const MeshSlicingParams          &params,
    std::function<void()>             throw_on_cancel = []{});

// 仅用于单个切片平面的专用版本，在单线程上运行。
Polygons                        slice_mesh(
    const indexed_triangle_set       &mesh,
    const float                       plane_z,
    const MeshSlicingParams          &params);

std::vector<ExPolygons>         slice_mesh_ex(
    const indexed_triangle_set       &mesh,
    const std::vector<float>         &zs,
    const MeshSlicingParamsEx        &params,
    std::function<void()>             throw_on_cancel = []{});

inline std::vector<ExPolygons>  slice_mesh_ex(
    const indexed_triangle_set       &mesh,
    const std::vector<float>         &zs,
    std::function<void()>             throw_on_cancel = []{})
{
    return slice_mesh_ex(mesh, zs, MeshSlicingParamsEx{}, throw_on_cancel);
}

inline std::vector<ExPolygons>  slice_mesh_ex(
    const indexed_triangle_set       &mesh,
    const std::vector<float>         &zs,
    float                             closing_radius,
    std::function<void()>             throw_on_cancel = []{})
{
    MeshSlicingParamsEx params;
    params.closing_radius = closing_radius;
    return slice_mesh_ex(mesh, zs, params, throw_on_cancel);
}

// 使用一组Z板层（厚层）对三角形集合进行切片。
// 效果类似于通过以下方式从切片网格生成通常的顶部/底部层：
// 分别从layer[i - 1]中减去layer[i]以获取顶面，
// 从layer[i + 1]中减去layer[i]以获取底面，
// 不同之处在于此函数处理的三角形集合可能不覆盖整个顶面或底面。
// 仅当out_top或out_bottom不为空时，才计算顶面或底面。
void slice_mesh_slabs(
    const indexed_triangle_set       &mesh,
    // 未缩放的Z值
    const std::vector<float>         &zs,
    const Transform3d                &trafo,
    std::vector<Polygons>            *out_top,
    std::vector<Polygons>            *out_bottom,
    std::vector<std::pair<Vec3f, Vec3f>>   *vertical_points,
    std::function<void()>             throw_on_cancel);

// 将网格的朝上表面/朝下表面投影到2D多边形。
void project_mesh(
    const indexed_triangle_set       &mesh,
    const Transform3d                &trafo,
    Polygons                         *out_top,
    Polygons                         *out_bottom,
    std::function<void()>             throw_on_cancel);

// 将网格投影到2D多边形。
Polygons project_mesh(
    const indexed_triangle_set       &mesh,
    const Transform3d                &trafo,
    std::function<void()>             throw_on_cancel);

void cut_mesh(
    const indexed_triangle_set      &mesh,
    float                            z,
    indexed_triangle_set            *upper,
    indexed_triangle_set            *lower,
    bool                             triangulate_caps = true);

} // namespace Slic3r

#endif // slic3r_TriangleMeshSlicer_hpp_
