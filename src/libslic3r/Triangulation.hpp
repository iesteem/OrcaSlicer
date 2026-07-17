#ifndef libslic3r_Triangulation_hpp_
#define libslic3r_Triangulation_hpp_

#include <vector>
#include <set>
#include <libslic3r/Point.hpp>
#include <libslic3r/Polygon.hpp>
#include <libslic3r/ExPolygon.hpp>

namespace Slic3r {

class Triangulation
{
public:
    Triangulation() = delete;

    // define oriented connection of 2 vertices(defined by its index)
    using HalfEdge  = std::pair<uint32_t, uint32_t>;
    using HalfEdges = std::vector<HalfEdge>;
    using Indices   = std::vector<Vec3i32>;

    /// <summary>
    /// 通过三角剖分连接点以创建由三角形填充的表面
    /// 输入点必须唯一
    /// 使点唯一的灵感来自 Emboss::dilate_to_unique_points
    /// </summary>
    /// <param name="points">要连接的点</param>
    /// <param name="edges">边的约束，对是从点(first)到
    /// 点(second)，按字典序排序</param>
    /// <returns>三角形</returns>
    static Indices triangulate(const Points &points,
                               const HalfEdges &half_edges);
    static Indices triangulate(const Polygon &polygon);
    static Indices triangulate(const Polygons &polygons);
    static Indices triangulate(const ExPolygon &expolygon);
    static Indices triangulate(const ExPolygons &expolygons);

    // Map for convert original index to set without duplication
    //              from_index<to_index>
    using Changes = std::vector<uint32_t>;

    /// <summary>
    /// 创建从原始索引到新索引的转换映射，
    /// 针对重复点进行处理
    /// </summary>
    /// <param name="points">输入点集</param>
    /// <param name="duplicits">从点中收集的重复点</param>
    /// <returns>点索引的转换映射</returns>
    static Changes create_changes(const Points &points, const Points &duplicits);

    /// <summary>
    /// Triangulation for expolygons, speed up when points are already collected
    /// NOTE: Not working properly for ExPolygons with multiple point on same coordinate
    /// You should check it by "collect_changes"
    /// </summary>
    /// <param name="expolygons">Input shape to triangulation - define edges</param>
    /// <param name="points">Points from expolygons</param>
    /// <returns>Triangle indices</returns>
    static Indices triangulate(const ExPolygons &expolygons, const Points& points);

    /// <summary>
    /// Triangulation for expolygons containing multiple points with same coordinate
    /// </summary>
    /// <param name="expolygons">Input shape to triangulation - define edge</param>
    /// <param name="points">Points from expolygons</param>
    /// <param name="changes">Changes swap for indicies into points</param>
    /// <returns>Triangle indices</returns>
    static Indices triangulate(const ExPolygons &expolygons, const Points& points, const Changes& changes);
};

} // namespace Slic3r
#endif // libslic3r_Triangulation_hpp_