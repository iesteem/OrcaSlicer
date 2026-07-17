#include <boost/log/trivial.hpp>
#include "MedialAxis.hpp"

#include <boost/log/trivial.hpp>
#include <boost/polygon/polygon.hpp>
#include <cassert>
#include <cmath>

#include "VoronoiOffset.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/libslic3r.h"

#ifdef SLIC3R_DEBUG
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
  //   discretization: 给定Voronoi边的点离散化。
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

      // 计算给定抛物线弧与离散化它的线段之间的最大距离。
      CT dist = (new_y - cur_y) * (mid_x - cur_x) -
          (new_x - cur_x) * (mid_y - cur_y);
      dist = dist * dist / ((new_y - cur_y) * (new_y - cur_y) +
          (new_x - cur_x) * (new_x - cur_x));
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
#endif // SLIC3R_DEBUG

namespace Slic3r { namespace Geometry {


#ifdef SLIC3R_DEBUG
// 以下用于可视化boost Voronoi图的代码基于：
//
// Boost.Polygon库 voronoi_visualizer.cpp 文件
//          Copyright Andrii Sydorchuk 2010-2012.
// 根据 Boost Software License, Version 1.0 分发。
//    （参见附带的 LICENSE_1_0.txt 文件或访问
//          http://www.boost.org/LICENSE_1_0.txt）
namespace Voronoi { namespace Internal {

    typedef double coordinate_type;
    typedef boost::polygon::point_data<coordinate_type> point_type;
    typedef boost::polygon::segment_data<coordinate_type> segment_type;
    typedef boost::polygon::rectangle_data<coordinate_type> rect_type;
    typedef boost::polygon::voronoi_diagram<coordinate_type> VD;
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

    inline point_type retrieve_point(const std::vector<segment_type> &segments, const cell_type& cell) 
    {
        assert(cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT || cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_END_POINT);
        return (cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT) ? low(segments[cell.source_index()]) : high(segments[cell.source_index()]);
    }

    inline void clip_infinite_edge(const std::vector<segment_type> &segments, const edge_type& edge, coordinate_type bbox_max_size, std::vector<point_type>* clipped_edge) 
    {
        const cell_type& cell1 = *edge.cell();
        const cell_type& cell2 = *edge.twin()->cell();
        point_type origin, direction;
        // 无限边不能由两个线段站点创建。
        if (cell1.contains_point() && cell2.contains_point()) {
            point_type p1 = retrieve_point(segments, cell1);
            point_type p2 = retrieve_point(segments, cell2);
            origin.x((p1.x() + p2.x()) * 0.5);
            origin.y((p1.y() + p2.y()) * 0.5);
            direction.x(p1.y() - p2.y());
            direction.y(p2.x() - p1.x());
        } else {
            origin = cell1.contains_segment() ? retrieve_point(segments, cell2) : retrieve_point(segments, cell1);
            segment_type segment = cell1.contains_segment() ? segments[cell1.source_index()] : segments[cell2.source_index()];
            coordinate_type dx = high(segment).x() - low(segment).x();
            coordinate_type dy = high(segment).y() - low(segment).y();
            if ((low(segment) == origin) ^ cell1.contains_point()) {
                direction.x(dy);
                direction.y(-dx);
            } else {
                direction.x(-dy);
                direction.y(dx);
            }
        }
        coordinate_type koef = bbox_max_size / (std::max)(fabs(direction.x()), fabs(direction.y()));
        if (edge.vertex0() == NULL) {
            clipped_edge->push_back(point_type(
                origin.x() - direction.x() * koef,
                origin.y() - direction.y() * koef));
        } else {
            clipped_edge->push_back(
                point_type(edge.vertex0()->x(), edge.vertex0()->y()));
        }
        if (edge.vertex1() == NULL) {
            clipped_edge->push_back(point_type(
                origin.x() + direction.x() * koef,
                origin.y() + direction.y() * koef));
        } else {
            clipped_edge->push_back(
                point_type(edge.vertex1()->x(), edge.vertex1()->y()));
        }
    }

    inline void sample_curved_edge(const std::vector<segment_type> &segments, const edge_type& edge, std::vector<point_type> &sampled_edge, coordinate_type max_dist) 
    {
        point_type point = edge.cell()->contains_point() ?
            retrieve_point(segments, *edge.cell()) :
            retrieve_point(segments, *edge.twin()->cell());
        segment_type segment = edge.cell()->contains_point() ?
            segments[edge.twin()->cell()->source_index()] :
            segments[edge.cell()->source_index()];
        ::boost::polygon::voronoi_visual_utils<coordinate_type>::discretize(point, segment, max_dist, &sampled_edge);
    }

} /* namespace Internal */ } // namespace Voronoi

void dump_voronoi_to_svg(const Lines &lines, /* const */ boost::polygon::voronoi_diagram<double> &vd, const ThickPolylines *polylines, const char *path)
{
    const double        scale                       = 0.2;
    const std::string   inputSegmentPointColor      = "lightseagreen";
    const coord_t       inputSegmentPointRadius     = coord_t(0.09 * scale / SCALING_FACTOR); 
    const std::string   inputSegmentColor           = "lightseagreen";
    const coord_t       inputSegmentLineWidth       = coord_t(0.03 * scale / SCALING_FACTOR);

    const std::string   voronoiPointColor           = "black";
    const coord_t       voronoiPointRadius          = coord_t(0.06 * scale / SCALING_FACTOR);
    const std::string   voronoiLineColorPrimary     = "black";
    const std::string   voronoiLineColorSecondary   = "green";
    const std::string   voronoiArcColor             = "red";
    const coord_t       voronoiLineWidth            = coord_t(0.02 * scale / SCALING_FACTOR);

    const bool          internalEdgesOnly           = false;
    const bool          primaryEdgesOnly            = false;

    BoundingBox bbox = BoundingBox(lines);
    bbox.min(0) -= coord_t(1. / SCALING_FACTOR);
    bbox.min(1) -= coord_t(1. / SCALING_FACTOR);
    bbox.max(0) += coord_t(1. / SCALING_FACTOR);
    bbox.max(1) += coord_t(1. / SCALING_FACTOR);

    ::Slic3r::SVG svg(path, bbox);

    if (polylines != NULL)
        svg.draw(*polylines, "lime", "lime", voronoiLineWidth);

//    bbox.scale(1.2);
    // 将半线裁剪到合理的值。
    // 然后SVG查看器无论如何都会裁剪该线。
    const double bbox_dim_max = double(bbox.max(0) - bbox.min(0)) + double(bbox.max(1) - bbox.min(1));
    // 用于Voronoi抛物线段的离散化。
    const double discretization_step = 0.0005 * bbox_dim_max;

    // 使用double类型复制输入线段。
    std::vector<Voronoi::Internal::segment_type> segments;
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++ it)
        segments.push_back(Voronoi::Internal::segment_type(
            Voronoi::Internal::point_type(double(it->a(0)), double(it->a(1))), 
            Voronoi::Internal::point_type(double(it->b(0)), double(it->b(1)))));
    
    // 对外部边着色。
    for (boost::polygon::voronoi_diagram<double>::const_edge_iterator it = vd.edges().begin(); it != vd.edges().end(); ++it)
        if (!it->is_finite())
            Voronoi::Internal::color_exterior(&(*it));

    // 绘制输入多边形的端点。
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++it) {
        svg.draw(it->a, inputSegmentPointColor, inputSegmentPointRadius);
        svg.draw(it->b, inputSegmentPointColor, inputSegmentPointRadius);
    }
    // 绘制输入多边形。
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++it)
        svg.draw(Line(Point(coord_t(it->a(0)), coord_t(it->a(1))), Point(coord_t(it->b(0)), coord_t(it->b(1)))), inputSegmentColor, inputSegmentLineWidth);

#if 1
    // 绘制Voronoi顶点。
    for (boost::polygon::voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
        if (! internalEdgesOnly || it->color() != Voronoi::Internal::EXTERNAL_COLOR)
            svg.draw(Point(coord_t(it->x()), coord_t(it->y())), voronoiPointColor, voronoiPointRadius);

    for (boost::polygon::voronoi_diagram<double>::const_edge_iterator it = vd.edges().begin(); it != vd.edges().end(); ++it) {
        if (primaryEdgesOnly && !it->is_primary())
            continue;
        if (internalEdgesOnly && (it->color() == Voronoi::Internal::EXTERNAL_COLOR))
            continue;
        std::vector<Voronoi::Internal::point_type> samples;
        std::string color = voronoiLineColorPrimary;
        if (!it->is_finite()) {
            Voronoi::Internal::clip_infinite_edge(segments, *it, bbox_dim_max, &samples);
            if (! it->is_primary())
                color = voronoiLineColorSecondary;
        } else {
            // Store both points of the segment into samples. sample_curved_edge will split the initial line
            // until the discretization_step is reached.
            samples.push_back(Voronoi::Internal::point_type(it->vertex0()->x(), it->vertex0()->y()));
            samples.push_back(Voronoi::Internal::point_type(it->vertex1()->x(), it->vertex1()->y()));
            if (it->is_curved()) {
                Voronoi::Internal::sample_curved_edge(segments, *it, samples, discretization_step);
                color = voronoiArcColor;
            } else if (! it->is_primary())
                color = voronoiLineColorSecondary;
        }
        for (std::size_t i = 0; i + 1 < samples.size(); ++i)
            svg.draw(Line(Point(coord_t(samples[i].x()), coord_t(samples[i].y())), Point(coord_t(samples[i+1].x()), coord_t(samples[i+1].y()))), color, voronoiLineWidth);
    }
#endif

    if (polylines != NULL)
        svg.draw(*polylines, "blue", voronoiLineWidth);

    svg.Close();
}
#endif // SLIC3R_DEBUG

template<typename VD, typename SEGMENTS>
inline const typename VD::point_type retrieve_cell_point(const typename VD::cell_type& cell, const SEGMENTS &segments)
{
    assert(cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT || cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_END_POINT);
    return (cell.source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT) ? low(segments[cell.source_index()]) : high(segments[cell.source_index()]);
}

template<typename VD, typename SEGMENTS>
inline std::pair<typename VD::coord_type, typename VD::coord_type> measure_edge_thickness(const VD &vd, const typename VD::edge_type& edge, const SEGMENTS &segments)
{
    typedef typename VD::coord_type T;
    const typename VD::point_type  pa(edge.vertex0()->x(), edge.vertex0()->y());
    const typename VD::point_type  pb(edge.vertex1()->x(), edge.vertex1()->y());
    const typename VD::cell_type  &cell1 = *edge.cell();
    const typename VD::cell_type  &cell2 = *edge.twin()->cell();
    if (cell1.contains_segment()) {
        if (cell2.contains_segment()) {
            // 两个单元都包含线性线段，左侧/右侧单元是对称的。
            // 将pa, pb投影到左侧线段。
            const typename VD::segment_type segment1 = segments[cell1.source_index()];
            const typename VD::point_type p1a = project_point_to_segment(segment1, pa);
            const typename VD::point_type p1b = project_point_to_segment(segment1, pb);
            return std::pair<T, T>(T(2.)*dist(pa, p1a), T(2.)*dist(pb, p1b));
        } else {
            // 第一个单元包含线性线段，第二个单元包含一个点。
            // 单元之间的中轴是抛物线弧。
            // 将pa, pb投影到左侧线段。
            const typename  VD::point_type p2 = retrieve_cell_point<VD>(cell2, segments);
            return std::pair<T, T>(T(2.)*dist(pa, p2), T(2.)*dist(pb, p2));
        }
    } else if (cell2.contains_segment()) {
        // 第一个单元包含一个点，第二个单元包含线性线段。
        // 单元之间的中轴是抛物线弧。
        const typename VD::point_type p1 = retrieve_cell_point<VD>(cell1, segments);
        return std::pair<T, T>(T(2.)*dist(pa, p1), T(2.)*dist(pb, p1));
    } else {
        // 两个单元都包含一个点。左侧/右侧区域是三角形且对称的。
        const typename VD::point_type p1 = retrieve_cell_point<VD>(cell1, segments);
        return std::pair<T, T>(T(2.)*dist(pa, p1), T(2.)*dist(pb, p1));
    }
}

// 将Lines向量中的Line实例转换为VD::segment_type。
template<typename VD>
class Lines2VDSegments
{
public:
    Lines2VDSegments(const Lines &alines) : lines(alines) {}
    typename VD::segment_type operator[](size_t idx) const {
        return typename VD::segment_type(
            typename VD::point_type(typename VD::coord_type(lines[idx].a(0)), typename VD::coord_type(lines[idx].a(1))),
            typename VD::point_type(typename VD::coord_type(lines[idx].b(0)), typename VD::coord_type(lines[idx].b(1))));
    }
private:
    const Lines &lines;
};

MedialAxis::MedialAxis(double min_width, double max_width, const ExPolygon &expolygon) :
    m_expolygon(expolygon), m_lines(expolygon.lines()), m_min_width(min_width), m_max_width(max_width)
{}

void MedialAxis::build(ThickPolylines* polylines)
{
    m_vd.construct_voronoi(m_lines.begin(), m_lines.end());

    // 对于SPE-1729中的一些ExPolygon，产生了无效的Voronoi图，无法通过旋转输入数据修复。
    // 这些ExPolygon包含非常细的线条和由非常接近（1-5nm）的顶点形成的孔洞，这些顶点处于我们分辨率极限。
    // 这些细线条和孔洞既不可打印，也导致Voronoi图无效。
    // 因此我们过滤掉这些细线条和孔洞，并尝试重新计算Voronoi图。
    if (!m_vd.is_valid()) {
        m_lines = to_lines(closing_ex({m_expolygon}, float(2. * SCALED_EPSILON)));
        m_vd.construct_voronoi(m_lines.begin(), m_lines.end());

        if (!m_vd.is_valid())
            BOOST_LOG_TRIVIAL(error) << "MedialAxis - Invalid Voronoi diagram even after morphological closing.";
    }

    Slic3r::Voronoi::annotate_inside_outside(m_vd, m_lines);
//    static constexpr double threshold_alpha = M_PI / 12.; // 30 degrees
//    std::vector<Vec2d> skeleton_edges = Slic3r::Voronoi::skeleton_edges_rough(vd, lines, threshold_alpha);
    
    /*
    // DEBUG: dump all Voronoi edges
    {
        for (VD::const_edge_iterator edge = m_vd.edges().begin(); edge != m_vd.edges().end(); ++edge) {
            if (edge->is_infinite()) continue;
            
            ThickPolyline polyline;
            polyline.points.push_back(Point( edge->vertex0()->x(), edge->vertex0()->y() ));
            polyline.points.push_back(Point( edge->vertex1()->x(), edge->vertex1()->y() ));
            polylines->push_back(polyline);
        }
        return;
    }
    */
    
    // 收集有效边（即剪枝不属于MAT的边）
    // 注意：这保留了孪生边，因此插入的有效边数量翻倍
    m_edge_data.assign(m_vd.edges().size() / 2, EdgeData{});
    for (VD::const_edge_iterator edge = m_vd.edges().begin(); edge != m_vd.edges().end(); edge += 2)
        if (edge->is_primary() && edge->is_finite() &&
            (Voronoi::vertex_category(edge->vertex0()) == Voronoi::VertexCategory::Inside ||
             Voronoi::vertex_category(edge->vertex1()) == Voronoi::VertexCategory::Inside) &&
            this->validate_edge(&*edge)) {
            // 有效骨架边。
            this->edge_data(*edge).first.active = true;
        }
    
    // 遍历有效边以构建多段线
    ThickPolyline reverse_polyline;
    for (VD::const_edge_iterator seed_edge = m_vd.edges().begin(); seed_edge != m_vd.edges().end(); seed_edge += 2)
        if (EdgeData &seed_edge_data = this->edge_data(*seed_edge).first; seed_edge_data.active) {
            // 标记此边为已访问。
            seed_edge_data.active = false;

            // 开始一条多段线。
            ThickPolyline polyline;
            polyline.points.emplace_back(seed_edge->vertex0()->x(), seed_edge->vertex0()->y());
            polyline.points.emplace_back(seed_edge->vertex1()->x(), seed_edge->vertex1()->y());
            polyline.width.emplace_back(seed_edge_data.width_start);
            polyline.width.emplace_back(seed_edge_data.width_end);        
            // 向前方向增长多段线。
            this->process_edge_neighbors(&*seed_edge, &polyline);
            assert(polyline.width.size() == polyline.points.size() * 2 - 2);
        
            // 向后方向增长多段线。
            reverse_polyline.clear();
            this->process_edge_neighbors(seed_edge->twin(), &reverse_polyline);
            polyline.points.insert(polyline.points.begin(), reverse_polyline.points.rbegin(), reverse_polyline.points.rend());
            polyline.width.insert(polyline.width.begin(), reverse_polyline.width.rbegin(), reverse_polyline.width.rend());
            polyline.endpoints.first = reverse_polyline.endpoints.second;
            assert(polyline.width.size() == polyline.points.size() * 2 - 2);
        
            // 防止循环端点被扩展。
            if (polyline.first_point() == polyline.last_point()) {
                polyline.endpoints.first = false;
                polyline.endpoints.second = false;
            }

            // 将多段线追加到结果中。
            polylines->emplace_back(std::move(polyline));
        }

    #ifdef SLIC3R_DEBUG
    {
        static int iRun = 0;
        dump_voronoi_to_svg(m_lines, m_vd, polylines, debug_out_path("MedialAxis-%d.svg", iRun ++).c_str());
        printf("Thick lines: ");
        for (ThickPolylines::const_iterator it = polylines->begin(); it != polylines->end(); ++ it) {
            ThickLines lines = it->thicklines();
            for (ThickLines::const_iterator it2 = lines.begin(); it2 != lines.end(); ++ it2) {
                printf("%f,%f ", it2->a_width, it2->b_width);
            }
        }
        printf("\n");
    }
    #endif /* SLIC3R_DEBUG */
}

void MedialAxis::build(Polylines* polylines)
{
    ThickPolylines tp;
    this->build(&tp);
    polylines->reserve(polylines->size() + tp.size());
    for (auto &pl : tp)
        polylines->emplace_back(pl.points);
}

void MedialAxis::process_edge_neighbors(const VD::edge_type *edge, ThickPolyline* polyline)
{
    for (;;) {
        // 由于rot_next()作用于边的起点，但我们想要
        // 查找终点上的邻居，所以只需将边与其孪生边交换。
        const VD::edge_type *twin = edge->twin();
    
        // 统计此边的邻居数
        size_t               num_neighbors  = 0;
        const VD::edge_type *first_neighbor = nullptr;
        for (const VD::edge_type *neighbor = twin->rot_next(); neighbor != twin; neighbor = neighbor->rot_next())
            if (this->edge_data(*neighbor).first.active) {
                if (num_neighbors == 0)
                    first_neighbor = neighbor;
                ++ num_neighbors;
            }
    
        // 如果只有一个邻居，则可以递归继续
        if (num_neighbors == 1) {
            if (std::pair<EdgeData&, bool> neighbor_data = this->edge_data(*first_neighbor);
                neighbor_data.first.active) {
                neighbor_data.first.active = false;
                polyline->points.emplace_back(first_neighbor->vertex1()->x(), first_neighbor->vertex1()->y());
                if (neighbor_data.second) {
                    polyline->width.push_back(neighbor_data.first.width_end);
                    polyline->width.push_back(neighbor_data.first.width_start);
                } else {
                    polyline->width.push_back(neighbor_data.first.width_start);
                    polyline->width.push_back(neighbor_data.first.width_end);
                }
                edge = first_neighbor;
                // 继续链式连接。
                continue;
            }
        } else if (num_neighbors == 0) {
            polyline->endpoints.second = true;
        } else {
            // T形或星形连接点
        }
        // 停止链式连接。
        break;
    }
}

bool MedialAxis::validate_edge(const VD::edge_type* edge)
{
    auto retrieve_segment = [this](const VD::cell_type* cell) -> const Line& { return m_lines[cell->source_index()]; };
    auto retrieve_endpoint = [retrieve_segment](const VD::cell_type* cell) -> const Point& {
        const Line &line = retrieve_segment(cell);
        return cell->source_category() == boost::polygon::SOURCE_CATEGORY_SEGMENT_START_POINT ? line.a : line.b;
    };

    // 防止溢出并检测近乎无限的边
// #ifndef CLIPPERLIB_INT32
//     if (std::abs(edge->vertex0()->x()) > double(CLIPPER_MAX_COORD_UNSCALED) || 
//         std::abs(edge->vertex0()->y()) > double(CLIPPER_MAX_COORD_UNSCALED) || 
//         std::abs(edge->vertex1()->x()) > double(CLIPPER_MAX_COORD_UNSCALED) ||
//         std::abs(edge->vertex1()->y()) > double(CLIPPER_MAX_COORD_UNSCALED))
//         return false;
// #endif // CLIPPERLIB_INT32

    // 构造表示Voronoi图此边的线
    const Line line({ edge->vertex0()->x(), edge->vertex0()->y() },
                    { edge->vertex1()->x(), edge->vertex1()->y() });
    
    // 检索生成正在检查的边的原始线段
    const VD::cell_type* cell_l = edge->cell();
    const VD::cell_type* cell_r = edge->twin()->cell();
    const Line &segment_l = retrieve_segment(cell_l);
    const Line &segment_r = retrieve_segment(cell_r);
    
    /*
    SVG svg("edge.svg");
    svg.draw(m_expolygon);
    svg.draw(line);
    svg.draw(segment_l, "red");
    svg.draw(segment_r, "blue");
    svg.Close();
    */
    
    /*  计算此边两个端点处的截面厚度。
        我们的Voronoi边是围绕其位于左侧的Voronoi单元（segment_l）的CCW序列的一部分。
        此边的孪生边围绕segment_r。因此，segment_r的方向与主边相同，
        而segment_l的方向与孪生边相同。
        我们以前只考虑（一半）到segment_r的距离，这适用于segment_l和segment_r
        几乎镜像相对的情况。然而，在曲线处它们交错排列，仅在非常短的长度上相对
        （我们非常短的边就代表了这种可见性）。
        根据Voronoi定义，w0和w1都可以朝向cell_l或cell_r计算，结果相同。
        当cell_l或cell_r不引用线段而仅引用端点时，我们计算到该端点的距离。 */
    
    coordf_t w0 = cell_r->contains_segment()
        ? segment_r.distance_to(line.a)*2
        : (retrieve_endpoint(cell_r) - line.a).cast<double>().norm()*2;
    
    coordf_t w1 = cell_l->contains_segment()
        ? segment_l.distance_to(line.b)*2
        : (retrieve_endpoint(cell_l) - line.b).cast<double>().norm()*2;
    
    if (cell_l->contains_segment() && cell_r->contains_segment()) {
        // 计算两个边界线段之间的相对角度
        double angle = fabs(segment_r.orientation() - segment_l.orientation());
        if (angle > PI)
            angle = 2. * PI - angle;
        assert(angle >= 0 && angle <= PI);

        // fabs(angle)范围从0（共线，同方向）到PI（共线，相反方向）
        // 我们只对接近第二种情况（相对线段）的线段感兴趣
        // 因此我们允许一些容差。
        // 此过滤器确保我们处理的是狭窄/定向区域（长大于厚）
        // 我们不在非由两个线段生成的边（即由一个线段和另一个线段的端点生成）上运行它，
        // 因为它们的朝向没有意义
        if (PI - angle > PI / 8.) {
            // 角度不够窄
            // 仅将此过滤器应用于不太短的线段，否则它们的角度可能没有意义
            if (w0 < SCALED_EPSILON || w1 < SCALED_EPSILON || line.length() >= m_min_width)
                return false;
        }
    } else {
        if (w0 < SCALED_EPSILON || w1 < SCALED_EPSILON)
            return false;
    }
    
    if ((w0 >= m_min_width || w1 >= m_min_width) &&
        (w0 <= m_max_width || w1 <= m_max_width)) {
        std::pair<EdgeData&, bool> ed = this->edge_data(*edge);
        if (ed.second)
            std::swap(w0, w1);
        ed.first.width_start = w0;
        ed.first.width_end   = w1;
        return true;
    }

    return false;
}

} } // namespace Slicer::Geometry
