#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include "FillHoneycomb.hpp"

namespace Slic3r {

void FillHoneycomb::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    // 缓存六边形计算
    CacheID cache_id(params.density, this->spacing);
    Cache::iterator it_m = this->cache.find(cache_id);
    if (it_m == this->cache.end()) {
        it_m = this->cache.insert(it_m, std::pair<CacheID, CacheData>(cache_id, CacheData()));
        CacheData &m        = it_m->second;
        coord_t min_spacing = coord_t(scale_(this->spacing)) * params.multiline;
        m.distance          = coord_t(min_spacing / params.density);
        m.hex_side          = coord_t(m.distance / (sqrt(3)/2));
        m.hex_width         = m.distance * 2; // $m->{hex_width} == $m->{hex_side} * sqrt(3);
        coord_t hex_height  = m.hex_side * 2;
        m.pattern_height    = hex_height + m.hex_side;
        m.y_short           = coord_t(m.distance * sqrt(3)/3);
        m.x_offset          = min_spacing / 2;
        m.y_offset          = coord_t(m.x_offset * sqrt(3)/3);
        m.hex_center        = Point(m.hex_width/2, m.hex_side);
    }
    CacheData &m = it_m->second;

    Polylines all_polylines;
    {
        // 将实际边界框调整到六边形图案的最近倍数
        // 并使其在层之间对齐

        BoundingBox bounding_box = expolygon.contour.bounding_box();
        {
            // 根据填充方向旋转边界框
            Polygon bb_polygon = bounding_box.polygon();
            bb_polygon.rotate(direction.first, m.hex_center);
            bounding_box = bb_polygon.bounding_box();

            // 扩展边界框，使我们的图案与其他层对齐
            // $bounding_box->[X1] 和 [Y1] 表示新边界框偏移与旧边界框偏移之间的位移
            // 填充未与物体边界框对齐，而是与世界坐标系对齐。据推测这已经足够好了。
            bounding_box.merge(align_to_grid(bounding_box.min, Point(m.hex_width, m.pattern_height)));
        }

        coord_t x = bounding_box.min(0);
        while (x <= bounding_box.max(0)) {
            Polyline p;
            coord_t ax[2] = { x + m.x_offset, x + m.distance - m.x_offset };
            for (size_t i = 0; i < 2; ++ i) {
                std::reverse(p.points.begin(), p.points.end()); // 将前半部分颠倒
                for (coord_t y = bounding_box.min(1); y <= bounding_box.max(1); y += m.y_short + m.hex_side + m.y_short + m.hex_side) {
                    p.points.push_back(Point(ax[1], y + m.y_offset));
                    p.points.push_back(Point(ax[0], y + m.y_short - m.y_offset));
                    p.points.push_back(Point(ax[0], y + m.y_short + m.hex_side + m.y_offset));
                    p.points.push_back(Point(ax[1], y + m.y_short + m.hex_side + m.y_short - m.y_offset));
                    p.points.push_back(Point(ax[1], y + m.y_short + m.hex_side + m.y_short + m.hex_side + m.y_offset));
                }
                ax[0] = ax[0] + m.distance;
                ax[1] = ax[1] + m.distance;
                std::swap(ax[0], ax[1]); // 绘制对称图案
                x += m.distance;
            }
            p.rotate(-direction.first, m.hex_center);
            p.simplify(5 * spacing); // 简化为 5 倍线宽
            all_polylines.push_back(p);
        }
    }
    // 如果需要，应用多线偏移
    multiline_fill(all_polylines, params, 1.1 * spacing);

    all_polylines = intersection_pl(std::move(all_polylines), expolygon);
    chain_or_connect_infill(std::move(all_polylines), expolygon, polylines_out, this->spacing, params);
}

} // namespace Slic3r
