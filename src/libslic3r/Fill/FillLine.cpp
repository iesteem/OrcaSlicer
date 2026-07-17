#include "../ClipperUtils.hpp"
#include "../ExPolygon.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include "FillLine.hpp"

namespace Slic3r {

void FillLine::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    // 旋转多边形，以便在此处使用垂直线
    expolygon.rotate(- direction.first);

    this->_min_spacing = scale_(this->spacing);
    assert(params.density > 0.0001f && params.density <= 1.f);
    this->_line_spacing = coord_t(coordf_t(this->_min_spacing) / params.density);
    this->_diagonal_distance = this->_line_spacing * 2;
    this->_line_oscillation = this->_line_spacing - this->_min_spacing; // 仅用于线条填充
    BoundingBox bounding_box = expolygon.contour.bounding_box();

    // 根据请求的密度定义流量间距
    if (params.density > 0.9999f && !params.dont_adjust) {
        this->_line_spacing = this->_adjust_solid_spacing(bounding_box.size()(0), this->_line_spacing);
        this->spacing = unscale<double>(this->_line_spacing);
    } else {
        // 扩展边界框，使我们的图案与其他层对齐
        // 将参考点转换到旋转坐标系。
        bounding_box.merge(align_to_grid(
            bounding_box.min,
            Point(this->_line_spacing, this->_line_spacing),
            direction.second.rotated(- direction.first)));
    }

    // 生成基本图案
    coord_t x_max = bounding_box.max(0) + SCALED_EPSILON;
    Lines lines;
    for (coord_t x = bounding_box.min(0); x <= x_max; x += this->_line_spacing)
        lines.push_back(this->_line(lines.size(), x, bounding_box.min(1), bounding_box.max(1)));

    // 针对稍大的 expolygon 裁剪路径，以便即使 expolygon 有垂直边也能保留第一条和最后一条路径
    // 防止边缘线被裁剪的最小偏移是 SCALED_EPSILON；
    // 但是我们使用更大的偏移来支持侧面略微倾斜且不完全直的 expolygon
    //FIXME Vojtech: Update the intersecton function to work directly with lines.
    Polylines polylines_src;
    polylines_src.reserve(lines.size());
    for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++ it) {
        polylines_src.push_back(Polyline());
        Points &pts = polylines_src.back().points;
        pts.reserve(2);
        pts.push_back(it->a);
        pts.push_back(it->b);
    }
    Polylines polylines = intersection_pl(polylines_src, offset(expolygon, scale_(0.02)));

    // FIXME Vojtech: 这仅对水平线执行，不对垂直线执行！
    const float INFILL_OVERLAP_OVER_SPACING = 0.3f;
    // 从 expolygon 外部扩展填充路径多少？
    coord_t extra = coord_t(floor(this->_min_spacing * INFILL_OVERLAP_OVER_SPACING + 0.5f));
    for (Polylines::iterator it_polyline = polylines.begin(); it_polyline != polylines.end(); ++ it_polyline) {
        Point *first_point = &it_polyline->points.front();
        Point *last_point  = &it_polyline->points.back();
        if (first_point->y() > last_point->y())
            std::swap(first_point, last_point);
        first_point->y() -= extra;
        last_point->y() += extra;
    }

    size_t n_polylines_out_old = polylines_out.size();

    // 连接线
    if (! polylines.empty()) { // prevent calling leftmost_point() on empty collections
        // 将 expolygon 偏移 max(min_spacing/2, extra)
        ExPolygon expolygon_off;
        {
            ExPolygons expolygons_off = offset_ex(expolygon, this->_min_spacing/2);
            if (! expolygons_off.empty()) {
                // 扩展多边形时，孤岛数量只会减少。因此对于输入的一个孤岛，offset_ex 应恰好生成一个扩展后的孤岛。
                assert(expolygons_off.size() == 1);
                std::swap(expolygon_off, expolygons_off.front());
            }
        }
        bool first = true;
        for (Polyline &polyline : chain_polylines(std::move(polylines))) {
            if (! first) {
                // 尝试连接线。
                Points &pts_end = polylines_out.back().points;
                const Point &first_point = polyline.points.front();
                const Point &last_point = pts_end.back();
                // X, Y 方向的距离。
                const Vector distance = last_point - first_point;
                // TODO: 我们还应检查两个点是否都在填充边界上，以避免连接内部区域边界上的路径
                if (this->_can_connect(std::abs(distance(0)), std::abs(distance(1))) &&
                    expolygon_off.contains(Line(last_point, first_point))) {
                    // 追加多段线。
                    pts_end.insert(pts_end.end(), polyline.points.begin(), polyline.points.end());
                    continue;
                }
            }
            // 线无法连接。
            polylines_out.emplace_back(std::move(polyline));
            first = false;
        }
    }

    // 路径必须旋转回来
    for (Polylines::iterator it = polylines_out.begin() + n_polylines_out_old; it != polylines_out.end(); ++ it) {
        // 无需平移，绝对位置无关紧要。
        // it->translate(- direction.second(0), - direction.second(1));
        it->rotate(direction.first);
    }
}

} // namespace Slic3r
