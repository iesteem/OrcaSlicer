#ifndef slic3r_Geometry_ConvexHull_hpp_
#define slic3r_Geometry_ConvexHull_hpp_

#include <vector>

#include "../Polygon.hpp"

namespace Slic3r {

class ExPolygon;
using ExPolygons = std::vector<ExPolygon>;

namespace Geometry {

Pointf3s convex_hull(Pointf3s points);
Polygon convex_hull(Points points);
Polygon convex_hull(const Polygons &polygons);
Polygon convex_hull(const ExPolygons &expolygons);
Polygon convex_hulll(const Polylines &polylines);

// 如果两个凸多边形A和B的交集不是空集，则返回true。
bool convex_polygons_intersect(const Polygon &A, const Polygon &B);

// 将源凸包点分解为x单调递增的上/下链，
// 创建源凸多边形的隐式梯形分解。
// 源凸多边形必须为CCW方向。O(n)时间复杂度。
std::pair<std::vector<Vec2d>, std::vector<Vec2d>> decompose_convex_polygon_top_bottom(const std::vector<Vec2d> &src);

// 使用上下链分解的凸多边形检查，O(log n)时间复杂度。
bool inside_convex_polygon(const std::pair<std::vector<Vec2d>, std::vector<Vec2d>> &top_bottom_decomposition, const Vec2d &pt);

} } // namespace Slicer::Geometry

#endif
