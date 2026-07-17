#ifndef slic3r_CutSurface_hpp_
#define slic3r_CutSurface_hpp_

#include <vector>
#include <admesh/stl.h> // indexed_triangle_set
#include "ExPolygon.hpp"
#include "Emboss.hpp" // IProjection

namespace Slic3r{

/// <summary>
/// 表示从对象切割的表面
/// 通过轮廓扩展索引三角形集合
/// </summary>
struct SurfaceCut : public indexed_triangle_set
{
    // 顶点索引（指向网格顶点的索引）
    using Index = unsigned int;
    using Contour = std::vector<Index>;
    using Contours = std::vector<Contour>;
    // 循环开放表面列表
    Contours contours;
};

/// <summary>
/// 从模型中切割表面形状。
/// </summary>
/// <param name="shapes">从模型中切割的多个形状</param>
/// <param name="models">要切割的多网格，需在同一坐标系中</param>
/// <param name="projection">定义将2D形状变换为3D</param>
/// <param name="projection_ratio">定义切割时前后投影的理想比例
/// 0 .. 表示使用最近的前投影
/// 1 .. 表示使用最近的后投影
/// 值在 <0, 1> 范围内
/// </param>
/// <returns>从模型切割的表面</returns>
SurfaceCut cut_surface(const ExPolygons                        &shapes,
                       const std::vector<indexed_triangle_set> &models,
                       const Emboss::IProjection               &projection,
                       float projection_ratio);

/// <summary>
/// 通过投影从表面切割创建模型
/// </summary>
/// <param name="cut">带有轮廓的模型表面</param>
/// <param name="projection">浮雕方式</param>
/// <returns>网格</returns>
indexed_triangle_set cut2model(const SurfaceCut         &cut,
                               const Emboss::IProject3d &projection);

/// <summary>
/// 从模型中分离感兴趣区域（AoI）
/// 注意：仅2D过滤，不按Z坐标过滤
/// </summary>
/// <param name="its">输入模型</param>
/// <param name="bb">投影到空间的边界框</param>
/// <param name="projection">定义BB到空间的变换</param>
/// <returns>至少部分位于投影边界框内的三角形</returns>
indexed_triangle_set its_cut_AoI(const indexed_triangle_set &its,
                                 const BoundingBox          &bb,
                                 const Emboss::IProjection  &projection);

/// <summary>
/// 按掩码分离三角形
/// </summary>
/// <param name="its">输入模型</param>
/// <param name="mask">掩码 - 与 its::indices 大小相同</param>
/// <returns>按掩码复制索引（及其顶点）</returns>
indexed_triangle_set its_mask(const indexed_triangle_set &its, const std::vector<bool> &mask);

bool corefine_test(const std::string &model_path, const std::string &shape_path);

} // namespace Slic3r
#endif // slic3r_CutSurface_hpp_
