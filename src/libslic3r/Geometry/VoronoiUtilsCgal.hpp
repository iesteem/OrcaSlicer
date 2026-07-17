#ifndef slic3r_VoronoiUtilsCgal_hpp_
#define slic3r_VoronoiUtilsCgal_hpp_

#include <boost/polygon/polygon.hpp>
#include <iterator>

#include "Voronoi.hpp"
#include "../Arachne/utils/PolygonsSegmentIndex.hpp"

namespace Slic3r::Geometry {
class VoronoiDiagram;

class VoronoiUtilsCgal
{
public:
    // 使用CGAL扫描线算法枚举所有线段之间的交点，检查Voronoi图是否为平面图。
    static bool is_voronoi_diagram_planar_intersection(const VoronoiDiagram &voronoi_diagram);

    // 通过验证每个顶点的所有相邻边是否按CCW顺序排列，检查Voronoi图是否为平面图。
    template<typename SegmentIterator>
    static typename boost::polygon::enable_if<
        typename boost::polygon::gtl_if<typename boost::polygon::is_segment_concept<
            typename boost::polygon::geometry_concept<typename std::iterator_traits<SegmentIterator>::value_type>::type>::type>::type,
        bool>::type
    is_voronoi_diagram_planar_angle(const VoronoiDiagram &voronoi_diagram, SegmentIterator segment_begin, SegmentIterator segment_end);
};
} // namespace Slic3r::Geometry

#endif // slic3r_VoronoiUtilsCgal_hpp_
