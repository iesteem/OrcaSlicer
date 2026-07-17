// 使用boost::polygon生成的Voronoi图进行多边形偏移。

#ifndef slic3r_VoronoiOffset_hpp_
#define slic3r_VoronoiOffset_hpp_

#include <boost/polygon/polygon.hpp>
#include <cmath>
#include <vector>

#include "libslic3r/libslic3r.h"
#include "Voronoi.hpp"
#include "libslic3r/Line.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"

namespace Slic3r {

namespace Voronoi {

using VD = Slic3r::Geometry::VoronoiDiagram;

inline const Point& contour_point(const VD::cell_type &cell, const Line &line)
	{ return ((cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT) ? line.a : line.b); }
inline Point&       contour_point(const VD::cell_type &cell, Line &line)
	{ return ((cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT) ? line.a : line.b); }

inline const Point& contour_point(const VD::cell_type &cell, const Lines &lines)
	{ return contour_point(cell, lines[cell.source_index()]); }
inline Point&       contour_point(const VD::cell_type &cell, Lines &lines)
	{ return contour_point(cell, lines[cell.source_index()]); }

inline Vec2d 		vertex_point(const VD::vertex_type &v) { return Vec2d(v.x(), v.y()); }
inline Vec2d 		vertex_point(const VD::vertex_type *v) { return Vec2d(v->x(), v->y()); }

// 存储在boost::polygon Voronoi顶点中的"颜色"。
enum class VertexCategory : unsigned char
{
	// Voronoi顶点位于输入轮廓上。
	// VD::vertex_type以double存储坐标，但转换为int32_t时坐标应与输入轮廓的坐标完全匹配。
	OnContour,
	// 顶点位于CCW输入轮廓内部，考虑孔洞。
	Inside,
	// 顶点位于CCW输入轮廓外部，考虑孔洞。
	Outside,
	// 尚不清楚。
	Unknown,
};

// 存储在boost::polygon Voronoi边中的"颜色"。
// boost::polygon Voronoi模块表示的Voronoi边实际上是一个半边，
// 半边根据目标顶点（VD::vertex_type::vertex1()）进行分类。
enum class EdgeCategory : unsigned char
{
	// 此半边指向轮廓，其VD::edge_type::vertex1().color()为OnContour。
	PointsToContour,
	// 此半边指向内部，其VD::edge_type::vertex1().color()为Inside。
	PointsInside,
	// 此半边指向外部，其VD::edge_type::vertex1().color()为Outside。
	PointsOutside,
	// 尚不清楚。
	Unknown
};

// 存储在boost::polygon Voronoi单元中的"颜色"。
enum class CellCategory : unsigned char
{
	// 此Voronoi单元被输入线段分成两半，一半在内部，一半在外部。
	Boundary,
	// 此Voronoi单元完全在内部。
	Inside,
	// 此Voronoi单元完全在外部。
	Outside,
	// 尚不清楚。
	Unknown
};

inline VertexCategory 	vertex_category(const VD::vertex_type &v)
	{ return static_cast<VertexCategory>(v.color()); }
inline VertexCategory 	vertex_category(const VD::vertex_type *v)
	{ return static_cast<VertexCategory>(v->color()); }
inline void 		  	set_vertex_category(VD::vertex_type &v, VertexCategory c)
	{ v.color(static_cast<VD::vertex_type::color_type>(c)); }
inline void 		  	set_vertex_category(VD::vertex_type *v, VertexCategory c)
	{ v->color(static_cast<VD::vertex_type::color_type>(c)); }

inline EdgeCategory 	edge_category(const VD::edge_type &e)
	{ return static_cast<EdgeCategory>(e.color()); }
inline EdgeCategory 	edge_category(const VD::edge_type *e)
	{ return static_cast<EdgeCategory>(e->color()); }
inline void 			set_edge_category(VD::edge_type &e, EdgeCategory c)
	{ e.color(static_cast<VD::edge_type::color_type>(c)); }
inline void 			set_edge_category(VD::edge_type *e, EdgeCategory c)
	{ e->color(static_cast<VD::edge_type::color_type>(c)); }

inline CellCategory   	cell_category(const VD::cell_type &v)
	{ return static_cast<CellCategory>(v.color()); }
inline CellCategory   	cell_category(const VD::cell_type *v)
	{ return static_cast<CellCategory>(v->color()); }
inline void 		  	set_cell_category(const VD::cell_type &v, CellCategory c)
	{ v.color(static_cast<VD::cell_type::color_type>(c)); }
inline void 		  	set_cell_category(const VD::cell_type *v, CellCategory c)
	{ v->color(static_cast<VD::cell_type::color_type>(c)); }

// 将VD顶点、边和单元的"颜色"标记为Unknown。
void reset_inside_outside_annotations(VD &vd);

// 为VD顶点、边和单元分配"颜色"，表示实体位于Lines定义的输入多边形内部还是外部。
void annotate_inside_outside(VD &vd, const Lines &lines);

// 返回从输入多边形到Voronoi顶点的有符号距离。
// （内部为负距离，外部为正距离）。
std::vector<double> signed_vertex_distances(const VD &vd, const Lines &lines);

static inline bool edge_offset_no_intersection(const Vec2d &intersection_point)
	{ return std::isnan(intersection_point.x()); }
static inline bool edge_offset_has_intersection(const Vec2d &intersection_point)
	{ return ! edge_offset_no_intersection(intersection_point); }
std::vector<Vec2d> edge_offset_contour_intersections(
	const VD &vd, const Lines &lines, const std::vector<double> &distances,
	double offset_distance);

std::vector<Vec2d> skeleton_edges_rough(
    const VD                    &vd,
    const Lines                 &lines,
    const double                 threshold_alpha);

Polygons offset(
    const Geometry::VoronoiDiagram  &vd,
    const Lines                     &lines,
    const std::vector<double>       &signed_vertex_distances,
    double                           offset_distance,
    double                           discretization_error);

// 通过遍历Voronoi图来偏移多边形或可能带孔的多边形集合。
// 输入多边形存储在lines中，lines由vd引用。
// 正偏移距离将提取外曲线，
// 负偏移距离将提取内曲线。
// 圆弧将被离散化以达到discretization_error。
Polygons offset(
	const VD 		&vd, 
	const Lines 	&lines, 
	double 			 offset_distance, 
	double 			 discretization_error);

} // namespace Voronoi

} // namespace Slic3r

#endif // slic3r_VoronoiOffset_hpp_
