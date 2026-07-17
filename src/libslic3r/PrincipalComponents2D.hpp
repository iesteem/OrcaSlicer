#ifndef slic3r_PrincipalComponents2D_hpp_
#define slic3r_PrincipalComponents2D_hpp_

#include "AABBTreeLines.hpp"
#include "BoundingBox.hpp"
#include "libslic3r.h"
#include <vector>
#include "Polygon.hpp"

namespace Slic3r {

// 返回三角形面积、面积一阶矩_xy、面积二阶矩_xy、面积二阶矩_协方差
// 所有值均未除以/归一化面积。
// 函数计算三角形面积上的积分，其中函数 f(x,y) = x 用于面积一阶矩（y 类似）
// f(x,y) = x^2 用于面积二阶矩
// f(x,y) = x*y 用于面积二阶矩协方差
std::tuple<float, Vec2f, Vec2f, float> compute_moments_of_area_of_triangle(const Vec2f &a, const Vec2f &b, const Vec2f &c);

// 返回给定多边形覆盖面积的两个特征向量。向量按其对应的特征值排序，最大的在前
std::tuple<Vec2f, Vec2f> compute_principal_components(const Polygons &polys);

}

#endif