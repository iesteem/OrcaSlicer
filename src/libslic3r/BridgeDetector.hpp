#ifndef slic3r_BridgeDetector_hpp_
#define slic3r_BridgeDetector_hpp_

#include "ClipperUtils.hpp"
#include "Line.hpp"
#include "Point.hpp"
#include "Polygon.hpp"
#include "Polyline.hpp"
#include "PrincipalComponents2D.hpp"
#include "libslic3r.h"
#include "ExPolygon.hpp"
#include <string>

namespace Slic3r {

// The bridge detector optimizes a direction of bridges over a region or a set of regions.
// A bridge direction is considered optimal, if the length of the lines strang over the region is maximal.
// This is optimal if the bridge is supported in a single direction only, but
// it may not likely be optimal, if the bridge region is supported from all sides. Then an optimal 
// solution would find a direction with shortest bridges.
// The bridge orientation is measured CCW from the X axis.
class BridgeDetector {
public:
    // 未扩展的孔洞。
    const ExPolygons            &expolygons;
    // 如果调用者按值提供输入多边形，则进行复制。
    ExPolygons                   expolygons_owned;
    // 下层切片，所有区域。
    const ExPolygons   			&lower_slices;
    // 填充的缩放挤出宽度。
    coord_t                      spacing;
    // 暴力搜索最佳桥接角度的角度分辨率。
    double                       resolution;
    // 最终最优角度。
    double                       angle;
    
    BridgeDetector(ExPolygon _expolygon, const ExPolygons &_lower_slices, coord_t _extrusion_width);
    BridgeDetector(const ExPolygons &_expolygons, const ExPolygons &_lower_slices, coord_t _extrusion_width);
    // 如果 bridge_direction_override != 0，则使用该角度代替自动检测。
    bool detect_angle(double bridge_direction_override = 0.);
    Polygons coverage(double angle = -1, bool precise = true) const;
    void unsupported_edges(double angle, Polylines* unsupported) const;
    Polylines unsupported_edges(double angle = -1) const;
    
private:
    // 抑制警告 "assignment operator could not be generated"
    BridgeDetector& operator=(const BridgeDetector &);

    void initialize();

    struct BridgeDirection {
        BridgeDirection(double a = -1.) : angle(a), coverage(0.), max_length(0.), archored_percent(0.){}
        // 最佳方向是导致最多线条被桥接的方向（即覆盖率最大）
        bool operator<(const BridgeDirection &other) const {
            // Initial sort by coverage only - comparator must obey strict weak ordering
            return this->coverage > other.coverage;//this->archored_percent > other.archored_percent;
        };
        double angle;
        double coverage;
        double max_length;
        double archored_percent;
    };

    // 获取可能的桥接方向候选。
    std::vector<double> bridge_direction_candidates() const;

    // 表示支撑边缘的开放线。
    Polylines _edges;
    // 表示支撑区域的封闭多边形。
    ExPolygons _anchor_regions;
};


//return ideal bridge direction and unsupported bridge endpoints distance.
inline std::tuple<Vec2d, double> detect_bridging_direction(const Lines &floating_edges, const Polygons &overhang_area)
{
    if (floating_edges.empty()) {
        // 认为此区域从所有侧面都被锚定，选择可能产生最短桥接的桥接方向
        auto [pc1, pc2] = compute_principal_components(overhang_area);
        if (pc2 == Vec2f::Zero()) { // overhang may be smaller than resolution. In this case, any direction is ok
            return {Vec2d{1.0,0.0}, 0.0};
        } else {
            return {pc2.normalized().cast<double>(), 0.0};
        }
    }

    // 悬垂未完全被锚点包围，在这种情况下，找到能够最小化空中桥接端/180度转弯数量的方向
    std::unordered_map<double, Vec2d> directions{};
    for (const Line &l : floating_edges) {
        Vec2d normal = l.normal().cast<double>().normalized();
        double quantized_angle = std::ceil(std::atan2(normal.y(),normal.x()) * 1000.0);
        directions.emplace(quantized_angle, normal);
    }
    std::vector<std::pair<Vec2d, double>> direction_costs{};
    // 这实际上是垂直桥接方向的成本 - 我们找到最小成本然后返回垂直方向
    for (const auto& d : directions) {
        direction_costs.emplace_back(d.second, 0.0);
    }

    for (const Line &l : floating_edges) {
        Vec2d line = (l.b - l.a).cast<double>();
        for (auto &dir_cost : direction_costs) {
            // the dot product already contains the length of the line. dir_cost.first is normalized.
            dir_cost.second += std::abs(line.dot(dir_cost.first));
        }
    }

    Vec2d  result_dir = Vec2d::Ones();
    double min_cost   = std::numeric_limits<double>::max();
    for (const auto &cost : direction_costs) {
        if (cost.second < min_cost) {
            // now flip the orientation back and return the direction of the bridge extrusions
            result_dir = Vec2d{cost.first.y(), -cost.first.x()};
            min_cost   = cost.second;
        }
    }

    return {result_dir, min_cost};
};

//return ideal bridge direction and unsupported bridge endpoints distance.
inline std::tuple<Vec2d, double> detect_bridging_direction(const Polygons &to_cover, const Polygons &anchors_area)
{
    Polygons  overhang_area      = diff(to_cover, anchors_area);
    Lines     floating_edges     = to_lines(diff_pl(to_polylines(overhang_area), expand(anchors_area, float(SCALED_EPSILON))));
    return detect_bridging_direction(floating_edges, overhang_area);
}

}

#endif
