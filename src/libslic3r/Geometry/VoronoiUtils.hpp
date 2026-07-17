#ifndef slic3r_VoronoiUtils_hpp_
#define slic3r_VoronoiUtils_hpp_

#include <boost/polygon/polygon.hpp>
#include <iterator>
#include <limits>

#include "libslic3r/Geometry/Voronoi.hpp"
#include "libslic3r/Arachne/utils/PolygonsSegmentIndex.hpp"
#include "libslic3r/Arachne/utils/PolygonsPointIndex.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/libslic3r.h"

using VD = Slic3r::Geometry::VoronoiDiagram;

namespace Slic3r::Geometry {

// 表示围绕线段的梯形Voronoi单元。
template<typename PT> struct SegmentCellRange
{
    const PT             source_segment_start_point; // The start point of the source segment of this cell.
    const PT             source_segment_end_point;   // The end point of the source segment of this cell.
    const VD::edge_type *edge_begin = nullptr;       // The edge of the Voronoi diagram where the loop around the cell starts.
    const VD::edge_type *edge_end   = nullptr;       // The edge of the Voronoi diagram where the loop around the cell ends.

    SegmentCellRange() = delete;
    explicit SegmentCellRange(const PT &source_segment_start_point, const PT &source_segment_end_point)
        : source_segment_start_point(source_segment_start_point), source_segment_end_point(source_segment_end_point)
    {}

    bool is_valid() const { return edge_begin && edge_end && edge_begin != edge_end; }
};

// 表示围绕点的梯形Voronoi单元。
template<typename PT> struct PointCellRange
{
    const PT             source_point;  // The source point of this cell.
    const VD::edge_type *edge_begin = nullptr; // The edge of the Voronoi diagram where the loop around the cell starts.
    const VD::edge_type *edge_end   = nullptr; // The edge of the Voronoi diagram where the loop around the cell ends.

    PointCellRange() = delete;
    explicit PointCellRange(const PT &source_point) : source_point(source_point) {}

    bool is_valid() const { return edge_begin && edge_end && edge_begin != edge_end; }
};

class VoronoiUtils
{
public:
    static Vec2i64 to_point(const VD::vertex_type *vertex);

    static Vec2i64 to_point(const VD::vertex_type &vertex);

    static bool is_finite(const VD::vertex_type &vertex);

    static VD::vertex_type make_rotated_vertex(VD::vertex_type &vertex, double angle);

    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        typename std::iterator_traits<SegmentIterator>::reference>::type
    get_source_segment(const VD::cell_type &cell, SegmentIterator segment_begin, SegmentIterator segment_end);

    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        typename boost::polygon::segment_point_type<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type
    get_source_point(const VoronoiDiagram::cell_type &cell, SegmentIterator segment_begin, SegmentIterator segment_end);

    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        Arachne::PolygonsPointIndex>::type
    get_source_point_index(const VD::cell_type &cell, SegmentIterator segment_begin, SegmentIterator segment_end);

    /**
     * 基于（近似）步长离散化抛物线。
     *
     * 改编自CuraEngine VoronoiUtils::discretizeParabola，作者为Tim Kuipers @BagelOrb 和 @Ghostkeeper。
     *
     * @param approximate_step_size 沿source_segment平行方向测量，而非沿抛物线方向。
     */
    template<typename Segment>
    static typename boost::polygon::enable_if<typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
        typename boost::polygon::geometry_concept<Segment>::type>::type>::type,
        Points>::type
    discretize_parabola(const Point &source_point, const Segment &source_segment, const Point &start, const Point &end, coord_t approximate_step_size, float transitioning_angle);

    /**
     * 计算围绕骨架图中属于中轴线段的单元的线段范围。
     *
     * 这仅应用于属于骨架图中心线段的单元，例如梯形单元，而非三角形单元。
     *
     * 结果线段仅为第一个和最后一个线段。它们链接到相邻线段，
     * 因此您可以迭代线段直到到达最后一个线段。
     *
     * 改编自CuraEngine VoronoiUtils::computeSegmentCellRange，作者为Tim Kuipers @BagelOrb、
     * Jaime van Kessel @nallath、Remco Burema @rburema 和 @Ghostkeeper。
     *
     * @param cell 要计算线段范围的单元。
     * @param segment_begin 输入多边形所有边的起始迭代器。
     * @param segment_end 输入多边形所有边的结束迭代器。
     * @return 围绕单元的线段范围。
     */
    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        Geometry::SegmentCellRange<
            typename boost::polygon::segment_point_type<typename std::iterator_traits<SegmentIterator>::value_type>::type>>::type
    compute_segment_cell_range(const VD::cell_type &cell, SegmentIterator segment_begin, SegmentIterator segment_end);

    /**
     * 计算围绕骨架图中属于中轴线上点的单元的线段范围。
     *
     * 这仅应用于属于骨架图拐角的单元，例如三角形单元，而非梯形单元。
     *
     * 结果线段仅为第一个和最后一个线段。它们链接到相邻线段，
     * 因此您可以迭代线段直到到达最后一个线段。
     *
     * 改编自CuraEngine VoronoiUtils::computePointCellRange，作者为Tim Kuipers @BagelOrb、
     * Jaime van Kessel @nallath、Remco Burema @rburema 和 @Ghostkeeper。
     *
     * @param cell 要计算线段范围的单元。
     * @param segment_begin 输入多边形所有边的起始迭代器。
     * @param segment_end 输入多边形所有边的结束迭代器。
     * @return 围绕单元的线段范围。
     */
    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        Geometry::PointCellRange<
            typename boost::polygon::segment_point_type<typename std::iterator_traits<SegmentIterator>::value_type>::type>>::type
    compute_point_cell_range(const VD::cell_type &cell, SegmentIterator segment_begin, SegmentIterator segment_end);

    template<typename T> static bool is_in_range(double value)
    {
        return double(std::numeric_limits<T>::lowest()) <= value && value <= double(std::numeric_limits<T>::max());
    }

    template<typename T> static bool is_in_range(const VD::vertex_type &vertex)
    {
        return VoronoiUtils::is_finite(vertex) && is_in_range<T>(vertex.x()) && is_in_range<T>(vertex.y());
    }

    template<typename T> static bool is_in_range(const VD::edge_type &edge)
    {
        if (edge.vertex0() == nullptr || edge.vertex1() == nullptr)
            return false;

        return is_in_range<T>(*edge.vertex0()) && is_in_range<T>(*edge.vertex1());
    }
};

} // namespace Slic3r::Geometry

#endif // slic3r_VoronoiUtils_hpp_
