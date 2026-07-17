#ifndef SLA_CONCAVEHULL_HPP
#define SLA_CONCAVEHULL_HPP

#include <libslic3r/ExPolygon.hpp>

namespace Slic3r {
namespace sla {

inline Polygons get_contours(const ExPolygons &poly)
{
    Polygons ret; ret.reserve(poly.size());
    for (const ExPolygon &p : poly) ret.emplace_back(p.contour);

    return ret;
}

using ThrowOnCancel = std::function<void()>;

/// 一种伪凹包，通过显式桥接连接分离的形状来构造。
/// 桥接从每个形状的质心生成到"场景"的中心，该中心是根据形状质心
/// 计算得到的质心（形成一个星形...）
class ConcaveHull {
    Polygons m_polys;

    static Point centroid(const Points& pp);

    static inline Point centroid(const Polygon &poly) { return poly.centroid(); }

    Points calculate_centroids() const;

    void merge_polygons();

    void add_connector_rectangles(const Points &centroids,
                                  coord_t       max_dist,
                                  ThrowOnCancel thr);
public:

    ConcaveHull(const ExPolygons& polys, double merge_dist, ThrowOnCancel thr)
        : ConcaveHull{to_polygons(polys), merge_dist, thr} {}

    ConcaveHull(const Polygons& polys, double mergedist, ThrowOnCancel thr);

    const Polygons & polygons() const { return m_polys; }

    ExPolygons to_expolygons() const;
};

ExPolygons offset_waffle_style_ex(const ConcaveHull &ccvhull, coord_t delta);
Polygons   offset_waffle_style(const ConcaveHull &polys, coord_t delta);

}}     // namespace Slic3r::sla
#endif // CONCAVEHULL_HPP
