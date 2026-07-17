#ifndef slic3r_VoronoiVisualUtils_hpp_
#define slic3r_VoronoiVisualUtils_hpp_

#include <stack>

#include <libslic3r/Geometry.hpp>
#include <libslic3r/Line.hpp>
#include <libslic3r/Polygon.hpp>
#include <libslic3r/SVG.hpp>

#include "VoronoiOffset.hpp"

namespace boost { namespace polygon {

// 以下用于可视化boost Voronoi图的代码基于：
//
// Boost.Polygon库 voronoi_graphic_utils.hpp 头文件
//          Copyright Andrii Sydorchuk 2010-2012.
// 根据 Boost Software License, Version 1.0 分发。
//    （参见附带的 LICENSE_1_0.txt 文件或访问
//          http://www.boost.org/LICENSE_1_0.txt）
template <typename CT>
class voronoi_visual_utils {
 public:
  // 离散化抛物线Voronoi边。
  // 抛物线Voronoi边总是由初始输入集合中的一个点和一个线段构成。
  //
  // 参数:
  //   point: 输入点。
  //   segment: 输入线段。
  //   max_dist: 最大离散化距离。
  //   discretization: 给定Voronoi边的点离散化结果。
  //
  // 模板参数:
  //   InCT: 输入几何体的坐标类型（通常为整数）。
  //   Point: 点类型，应建模点概念。
  //   Segment: 线段类型，应建模线段概念。
  //
  // 重要:
  //   discretization 应初始包含边的两个端点。
  template <class InCT1, class InCT2,
            template<class> class Point,
            template<class> class Segment>
  static
  typename enable_if<
    typename gtl_and<
      typename gtl_if<
        typename is_point_concept<
          typename geometry_concept< Point<InCT1> >::type
        >::type
      >::type,
      typename gtl_if<
        typename is_segment_concept<
          typename geometry_concept< Segment<InCT2> >::type
        >::type
      >::type
    >::type,
    void
  >::type discretize(
      const Point<InCT1>& point,
      const Segment<InCT2>& segment,
      const CT max_dist,
      std::vector< Point<CT> >* discretization) {
    // 应用线性变换，将线段的起点移动到坐标(0, 0)，
    // 并将线段的方向与x轴正方向重合。
    CT segm_vec_x = cast(x(high(segment))) - cast(x(low(segment)));
    CT segm_vec_y = cast(y(high(segment))) - cast(y(low(segment)));
    CT sqr_segment_length = segm_vec_x * segm_vec_x + segm_vec_y * segm_vec_y;

    // 在变换空间中计算边端点的x坐标。
    CT projection_start = sqr_segment_length *
        get_point_projection((*discretization)[0], segment);
    CT projection_end = sqr_segment_length *
        get_point_projection((*discretization)[1], segment);
    assert(projection_start != projection_end);

    // 在变换空间中计算抛物线参数。
    // 抛物线具有以下表示形式：
    // f(x) = ((x-rot_x)^2 + rot_y^2) / (2.0*rot_y)。
    CT point_vec_x = cast(x(point)) - cast(x(low(segment)));
    CT point_vec_y = cast(y(point)) - cast(y(low(segment)));
    CT rot_x = segm_vec_x * point_vec_x + segm_vec_y * point_vec_y;
    CT rot_y = segm_vec_x * point_vec_y - segm_vec_y * point_vec_x;

    // 保存最后一个点。
    Point<CT> last_point = (*discretization)[1];
    discretization->pop_back();

    // 使用栈来避免递归。
    std::stack<CT> point_stack;
    point_stack.push(projection_end);
    CT cur_x = projection_start;
    CT cur_y = parabola_y(cur_x, rot_x, rot_y);

    // 在变换空间中调整max_dist参数。
    const CT max_dist_transformed = max_dist * max_dist * sqr_segment_length;
    while (!point_stack.empty()) {
      CT new_x = point_stack.top();
      CT new_y = parabola_y(new_x, rot_x, rot_y);

      // 计算抛物线上距当前线段最远的点的坐标。
      CT mid_x = (new_y - cur_y) / (new_x - cur_x) * rot_y + rot_x;
      CT mid_y = parabola_y(mid_x, rot_x, rot_y);
      assert(mid_x != cur_x || mid_y != cur_y);
      assert(mid_x != new_x || mid_y != new_y);

      // 计算给定抛物线弧与离散化它的线段之间的最大距离。
      CT dist = (new_y - cur_y) * (mid_x - cur_x) -
          (new_x - cur_x) * (mid_y - cur_y);
      CT div = (new_y - cur_y) * (new_y - cur_y) + (new_x - cur_x) * (new_x - cur_x);
      assert(div != 0);
      dist = dist * dist / div;
      if (dist <= max_dist_transformed) {
        // 抛物线与线段之间的距离小于max_dist。
        point_stack.pop();
        CT inter_x = (segm_vec_x * new_x - segm_vec_y * new_y) /
            sqr_segment_length + cast(x(low(segment)));
        CT inter_y = (segm_vec_x * new_y + segm_vec_y * new_x) /
            sqr_segment_length + cast(y(low(segment)));
        discretization->push_back(Point<CT>(inter_x, inter_y));
        cur_x = new_x;
        cur_y = new_y;
      } else {
        point_stack.push(mid_x);
      }
    }

    // 更新最后一个点。
    discretization->back() = last_point;
  }

 private:
  // 计算 y(x) = ((x - a) * (x - a) + b * b) / (2 * b)。
  static CT parabola_y(CT x, CT a, CT b) {
    return ((x - a) * (x - a) + b * b) / (b + b);
  }

  // 获取以下距离的归一化长度：
  //   1) 点到线段的投影
  //   2) 线段的起点
  // 返回该长度除以线段长度。这样做是为了避免
  // 在初始空间与变换空间之间相互转换时进行sqrt计算。
  // 假设点的投影位于线段的起点和终点之间。
  template <class InCT,
            template<class> class Point,
            template<class> class Segment>
  static
  typename enable_if<
    typename gtl_and<
      typename gtl_if<
        typename is_point_concept<
          typename geometry_concept< Point<int> >::type
        >::type
      >::type,
      typename gtl_if<
        typename is_segment_concept<
          typename geometry_concept< Segment<long> >::type
        >::type
      >::type
    >::type,
    CT
  >::type get_point_projection(
      const Point<CT>& point, const Segment<InCT>& segment) {
    CT segment_vec_x = cast(x(high(segment))) - cast(x(low(segment)));
    CT segment_vec_y = cast(y(high(segment))) - cast(y(low(segment)));
    CT point_vec_x = x(point) - cast(x(low(segment)));
    CT point_vec_y = y(point) - cast(y(low(segment)));
    CT sqr_segment_length =
        segment_vec_x * segment_vec_x + segment_vec_y * segment_vec_y;
    CT vec_dot = segment_vec_x * point_vec_x + segment_vec_y * point_vec_y;
    return vec_dot / sqr_segment_length;
  }

  template <typename InCT>
  static CT cast(const InCT& value) {
    return static_cast<CT>(value);
  }
};

} } // namespace boost::polygon


namespace Slic3r
{

// 以下用于可视化boost Voronoi图的代码基于：
//
// Boost.Polygon库 voronoi_visualizer.cpp 文件
//          Copyright Andrii Sydorchuk 2010-2012.
// 根据 Boost Software License, Version 1.0 分发。
//    （参见附带的 LICENSE_1_0.txt 文件或访问
//          http://www.boost.org/LICENSE_1_0.txt）
namespace Voronoi { namespace Internal {

    using VD = Geometry::VoronoiDiagram;
    typedef double coordinate_type;
    typedef boost::polygon::point_data<coordinate_type> point_type;
    typedef boost::polygon::segment_data<coordinate_type> segment_type;
    typedef boost::polygon::rectangle_data<coordinate_type> rect_type;
    typedef VD::cell_type cell_type;
    typedef VD::cell_type::source_index_type source_index_type;
    typedef VD::cell_type::source_category_type source_category_type;
    typedef VD::edge_type edge_type;
    typedef VD::cell_container_type cell_container_type;
    typedef VD::cell_container_type vertex_container_type;
    typedef VD::edge_container_type edge_container_type;
    typedef VD::const_cell_iterator const_cell_iterator;
    typedef VD::const_vertex_iterator const_vertex_iterator;
    typedef VD::const_edge_iterator const_edge_iterator;

    static const std::size_t EXTERNAL_COLOR = 1;

    inline void color_exterior(const VD::edge_type* edge)
    {
        if (edge->color() == EXTERNAL_COLOR)
            return;
        edge->color(EXTERNAL_COLOR);
        edge->twin()->color(EXTERNAL_COLOR);
        const VD::vertex_type* v = edge->vertex1();
        if (v == NULL || !edge->is_primary())
            return;
        v->color(EXTERNAL_COLOR);
        const VD::edge_type* e = v->incident_edge();
        do {
            color_exterior(e);
            e = e->rot_next();
        } while (e != v->incident_edge());
    }

    inline point_type retrieve_point(const Points &points, const std::vector<segment_type> &segments, const cell_type& cell)
    {
        assert(cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT || cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_END_POINT ||
               cell.source_category() == boost::polygon::SOURCE_CATEGORY_SINGLE_POINT);
        return cell.source_category() == boost::polygon::SOURCE_CATEGORY_SINGLE_POINT ?
                    Voronoi::Internal::point_type(double(points[cell.source_index()].x()), double(points[cell.source_index()].y())) :
                    (cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT) ?
                        low(segments[cell.source_index()]) : high(segments[cell.source_index()]);
    }

    inline void clip_infinite_edge(const Points &points, const std::vector<segment_type> &segments, const edge_type& edge, coordinate_type bbox_max_size, std::vector<point_type>* clipped_edge)
    {
        assert(edge.is_infinite());
        assert((edge.vertex0() == nullptr) != (edge.vertex1() == nullptr));

        const cell_type& cell1 = *edge.cell();
        const cell_type& cell2 = *edge.twin()->cell();
        // 无限边不能由两个线段站点创建。
        assert(cell1.contains_point() || cell2.contains_point());
        if (! cell1.contains_point() && ! cell2.contains_point()) {
            printf("Error! clip_infinite_edge - infinite edge separates two segment cells\n");
            return;
        }
        point_type direction;
        if (cell1.contains_point() && cell2.contains_point()) {
            assert(! edge.is_secondary());
            point_type p1 = retrieve_point(points, segments, cell1);
            point_type p2 = retrieve_point(points, segments, cell2);
            if (edge.vertex0() == nullptr)
                std::swap(p1, p2);
            direction.x(p1.y() - p2.y());
            direction.y(p2.x() - p1.x());
        } else {
            assert(edge.is_secondary());
            segment_type segment = cell1.contains_segment() ? segments[cell1.source_index()] : segments[cell2.source_index()];
            direction.x(high(segment).y() - low(segment).y());
            direction.y(low(segment).x() - high(segment).x());
        }
        coordinate_type koef = bbox_max_size / (std::max)(fabs(direction.x()), fabs(direction.y()));
        if (edge.vertex0() == nullptr) {
            clipped_edge->push_back(point_type(edge.vertex1()->x() + direction.x() * koef, edge.vertex1()->y() + direction.y() * koef));
            clipped_edge->push_back(point_type(edge.vertex1()->x(), edge.vertex1()->y()));
        } else {
            clipped_edge->push_back(point_type(edge.vertex0()->x(), edge.vertex0()->y()));
            clipped_edge->push_back(point_type(edge.vertex0()->x() + direction.x() * koef, edge.vertex0()->y() + direction.y() * koef));
        }
    }

    inline void sample_curved_edge(const Points &points, const std::vector<segment_type> &segments, const edge_type& edge, std::vector<point_type> &sampled_edge, coordinate_type max_dist)
    {
        point_type point = edge.cell()->contains_point() ?
            retrieve_point(points, segments, *edge.cell()) :
            retrieve_point(points, segments, *edge.twin()->cell());
        segment_type segment = edge.cell()->contains_point() ?
            segments[edge.twin()->cell()->source_index()] :
            segments[edge.cell()->source_index()];
        ::boost::polygon::voronoi_visual_utils<coordinate_type>::discretize(point, segment, max_dist, &sampled_edge);
    }

} /* namespace Internal */ } // namespace Voronoi

BoundingBox get_extents(const Lines &lines);

static inline void dump_voronoi_to_svg(
    const char          *path,
    const Geometry::VoronoiDiagram &vd,
    const Points        &points,
    const Lines         &lines,
    const Polygons      &offset_curves = Polygons(),
    const Lines         &helper_lines = Lines(),
    double               scale = 0)
{
    const bool          internalEdgesOnly           = false;

    BoundingBox bbox;
    bbox.merge(get_extents(points));
    bbox.merge(get_extents(lines));
    bbox.merge(get_extents(offset_curves));
    bbox.merge(get_extents(helper_lines));
    for (boost::polygon::voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
        if (! internalEdgesOnly || it->color() != Voronoi::Internal::EXTERNAL_COLOR)
            bbox.merge(Point(it->x(), it->y()));
    bbox.min -= (0.01 * bbox.size().cast<double>()).cast<coord_t>();
    bbox.max += (0.01 * bbox.size().cast<double>()).cast<coord_t>();

    if (scale == 0)
        scale =
//                0.1
                0.01
                * std::min(bbox.size().x(), bbox.size().y());
    else
        scale *= SCALING_FACTOR;

    const std::string   inputSegmentPointColor      = "lightseagreen";
    const coord_t       inputSegmentPointRadius     = std::max<coord_t>(1, coord_t(0.09 * scale));
    const std::string   inputSegmentColor           = "lightseagreen";
    const coord_t       inputSegmentLineWidth       = coord_t(0.03 * scale);

    const std::string   voronoiPointColor           = "black";
    const std::string   voronoiPointColorOutside    = "red";
    const std::string   voronoiPointColorInside     = "blue";
    const coord_t       voronoiPointRadius          = std::max<coord_t>(1, coord_t(0.06 * scale));
    const std::string   voronoiLineColorPrimary     = "black";
    const std::string   voronoiLineColorSecondary   = "green";
    const std::string   voronoiArcColor             = "red";
    const coord_t       voronoiLineWidth            = coord_t(0.02 * scale);

    const std::string   offsetCurveColor            = "magenta";
    const coord_t       offsetCurveLineWidth        = coord_t(0.02 * scale);

    const std::string   helperLineColor             = "orange";
    const coord_t       helperLineWidth             = coord_t(0.04 * scale);

    const bool          primaryEdgesOnly            = false;

    ::Slic3r::SVG svg(path, bbox);

    // 将半线裁剪到合理的值。
    // 然后SVG查看器无论如何都会裁剪该线。
    const double bbox_dim_max = double(std::max(bbox.size().x(), bbox.size().y()));
    // 用于Voronoi抛物线段的离散化。
    const double discretization_step = 0.0002 * bbox_dim_max;

    // 使用double类型复制输入线段。
    std::vector<Voronoi::Internal::segment_type> segments;
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++ it)
        segments.push_back(Voronoi::Internal::segment_type(
            Voronoi::Internal::point_type(double(it->a(0)), double(it->a(1))),
            Voronoi::Internal::point_type(double(it->b(0)), double(it->b(1)))));

    // 对外部边着色。
    if (internalEdgesOnly) {
        for (boost::polygon::voronoi_diagram<double>::const_edge_iterator it = vd.edges().begin(); it != vd.edges().end(); ++it)
            if (!it->is_finite())
                Voronoi::Internal::color_exterior(&(*it));
    }

    // 绘制输入多边形的端点。
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++it) {
        svg.draw(it->a, inputSegmentPointColor, inputSegmentPointRadius);
        svg.draw(it->b, inputSegmentPointColor, inputSegmentPointRadius);
    }
    // Draw the input polygon.
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++it)
        svg.draw(Line(Point(coord_t(it->a(0)), coord_t(it->a(1))), Point(coord_t(it->b(0)), coord_t(it->b(1)))), inputSegmentColor, inputSegmentLineWidth);

#if 1
    // Draw voronoi vertices.
    for (boost::polygon::voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
        if (! internalEdgesOnly || it->color() != Voronoi::Internal::EXTERNAL_COLOR) {
            const std::string *color = nullptr;
            switch (Voronoi::vertex_category(*it)) {
            case Voronoi::VertexCategory::OnContour:    color = &voronoiPointColor;         break;
            case Voronoi::VertexCategory::Outside:      color = &voronoiPointColorOutside;  break;
            case Voronoi::VertexCategory::Inside:       color = &voronoiPointColorInside;   break;
            default: color = &voronoiPointColor; // assert(false);
            }
            Point pt(coord_t(it->x()), coord_t(it->y()));
            if (it->x() * pt.x() >= 0. && it->y() * pt.y() >= 0.)
                // Conversion to coord_t is valid.
                svg.draw(Point(coord_t(it->x()), coord_t(it->y())), *color, voronoiPointRadius);
        }

    for (boost::polygon::voronoi_diagram<double>::const_edge_iterator it = vd.edges().begin(); it != vd.edges().end(); ++it) {
        if (primaryEdgesOnly && !it->is_primary())
            continue;
        if (internalEdgesOnly && (it->color() == Voronoi::Internal::EXTERNAL_COLOR))
            continue;
        std::vector<Voronoi::Internal::point_type> samples;
        std::string color = voronoiLineColorPrimary;
        if (!it->is_finite()) {
            Voronoi::Internal::clip_infinite_edge(points, segments, *it, bbox_dim_max, &samples);
            if (! it->is_primary())
                color = voronoiLineColorSecondary;
        } else {
            // Store both points of the segment into samples. sample_curved_edge will split the initial line
            // until the discretization_step is reached.
            samples.push_back(Voronoi::Internal::point_type(it->vertex0()->x(), it->vertex0()->y()));
            samples.push_back(Voronoi::Internal::point_type(it->vertex1()->x(), it->vertex1()->y()));
            if (it->is_curved()) {
                Voronoi::Internal::sample_curved_edge(points, segments, *it, samples, discretization_step);
                color = voronoiArcColor;
            } else if (! it->is_primary())
                color = voronoiLineColorSecondary;
        }
        for (std::size_t i = 0; i + 1 < samples.size(); ++ i) {
            Vec2d a(samples[i].x(), samples[i].y());
            Vec2d b(samples[i+1].x(), samples[i+1].y());
            // Convert to coord_t.
            Point ia = a.cast<coord_t>();
            Point ib = b.cast<coord_t>();
            // Is the conversion possible? Do the resulting points fit into int32_t?
            auto  in_range = [](const Point &ip, const Vec2d &p) { return p.x() * ip.x() >= 0. && p.y() * ip.y() >= 0.; };
            bool  a_in_range = in_range(ia, a);
            bool  b_in_range = in_range(ib, b);
            if (! a_in_range || ! b_in_range) {
                if (! a_in_range && ! b_in_range)
                    // None fits, ignore.
                    continue;
                // One fit, the other does not. Try to clip.
                Vec2d v = b - a;
                v.normalize();
                v *= bbox.size().cast<double>().norm();
                auto p = a_in_range ? Vec2d(a + v) : Vec2d(b - v);
                Point ip = p.cast<coord_t>();
                if (! in_range(ip, p))
                    continue;
                (a_in_range ? ib : ia) = ip;
            }
            svg.draw(Line(ia, ib), color, voronoiLineWidth);
        }
    }
#endif

    svg.draw_outline(offset_curves, offsetCurveColor, offsetCurveLineWidth);
    svg.draw(helper_lines, helperLineColor, helperLineWidth);

    svg.Close();
}

} // namespace Slic3r

#endif // slic3r_VoronoiVisualUtils_hpp_
