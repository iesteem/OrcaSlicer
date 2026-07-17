#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"
#include <cmath>
#include "FillBase.hpp"
#include "FillCrossHatch.hpp"

namespace Slic3r {

// CrossHatch 填充：提升 3D 打印速度并降低噪音
// CrossHatch，如其名所示，每几层将线条方向交替 90 度以改善附着力。
// 它在方向转换之间引入过渡层以实现更好的线条内聚力，从而解决了线条填充的弱点。
// 过渡技术受到 David Eccles 的启发，改进了 3D 蜂巢填充，但我们做出了更灵活的实现。
// 该方法显著提高了打印速度，满足现代高速 3D 打印机的需求，并降低了大多数层的噪音。
// By Bambu Lab

// 图形致谢：David Eccles (gringer)。
// 但我们对点做了不同的定义。
/*    o---o
 *   /     \
 *  /       \
 *           \       /
 *            \     /
 *             o---o
 *   p1   p2  p3   p4
 */

static Pointfs generate_one_cycle(double progress, coordf_t period)
{
    Pointfs out;
    double  offset = progress * 1. / 8. * period;
    out.reserve(4);
    out.push_back(Vec2d(0.25 * period - offset, offset));
    out.push_back(Vec2d(0.25 * period + offset, offset));
    out.push_back(Vec2d(0.75 * period - offset, -offset));
    out.push_back(Vec2d(0.75 * period + offset, -offset));
    return out;
}

static Polylines generate_transform_pattern(double inprogress, int direction, coordf_t ingrid_size, coordf_t inwidth, coordf_t inheight)
{
    coordf_t  width     = inwidth;
    coordf_t  height    = inheight;
    coordf_t  grid_size = ingrid_size * 2; // we due with odd and even saparately.
    double    progress  = inprogress;
    Polylines out_polylines;

    // generate template patterns;
    Pointfs one_cycle_points = generate_one_cycle(progress, grid_size);

    Polyline one_cycle;
    one_cycle.points.reserve(one_cycle_points.size());
    for (size_t i = 0; i < one_cycle_points.size(); i++) one_cycle.points.push_back(Point(one_cycle_points[i]));

    // 如果是垂直方向则交换
    if (direction < 0) {
        width  = height;
        height = inwidth;
    }

    // 复制多段线；
    Polylines odd_polylines;
    Polyline  odd_poly;
    int       num_of_cycle = width / grid_size + 2;
    odd_poly.points.reserve(num_of_cycle * one_cycle.size());

    // 复制到奇数行
    Point translate = Point(0, 0);
    for (size_t i = 0; i < num_of_cycle; i++) {
        Polyline odd_points;
        odd_points = Polyline(one_cycle);
        odd_points.translate(Point(i * grid_size, 0.0));
        odd_poly.points.insert(odd_poly.points.end(), odd_points.begin(), odd_points.end());
    }

    // 填充高度
    int num_of_lines = height / grid_size + 2;
    odd_polylines.reserve(num_of_lines * odd_poly.size());
    for (size_t i = 0; i < num_of_lines; i++) {
        Polyline poly = odd_poly;
        poly.translate(Point(0.0, grid_size * i));
        odd_polylines.push_back(poly);
    }
    // 保存到输出
    out_polylines.insert(out_polylines.end(), odd_polylines.begin(), odd_polylines.end());

    // 复制到偶数行
    Polylines even_polylines;
    even_polylines.reserve(odd_polylines.size());
    for (size_t i = 0; i < odd_polylines.size(); i++) {
        Polyline even = odd_poly;
        even.translate(Point(-0.5 * grid_size, (i + 0.5) * grid_size));
        even_polylines.push_back(even);
    }

    // 保存用于输出
    out_polylines.insert(out_polylines.end(), even_polylines.begin(), even_polylines.end());

    // 如果需要则改为垂直方向
    if (direction < 0) {
        // 交换 xy，看是否需要更好的性能方法
        for (Polyline &poly : out_polylines) {
            for (Point &p : poly) { std::swap(p.x(), p.y()); }
        }
    }

    return out_polylines;
}

static Polylines generate_repeat_pattern(int direction, coordf_t grid_size, coordf_t inwidth, coordf_t inheight)
{
    coordf_t  width  = inwidth;
    coordf_t  height = inheight;
    Polylines out_polylines;

    // 如果是垂直方向则交换
    if (direction < 0) {
        width  = height;
        height = inwidth;
    }

    int num_of_lines = height / grid_size + 1;
    out_polylines.reserve(num_of_lines);

    for (int i = 0; i < num_of_lines; i++) {
        Polyline poly;
        poly.points.reserve(2);
        poly.append(Point(coordf_t(0), coordf_t(grid_size * i)));
        poly.append(Point(width, coordf_t(grid_size * i)));
        out_polylines.push_back(poly);
    }

    // 如果需要则改为垂直方向
    if (direction < 0) {
        // 交换 xy
        for (Polyline &poly : out_polylines) {
            for (Point &p : poly) { std::swap(p.x(), p.y()); }
        }
    }

    return out_polylines;
}

// 生成与边界框重叠的实际图案
// repeat_ratio 定义重复图案高度与网格之间的比率
static Polylines generate_infill_layers(coordf_t z_height, double repeat_ratio, coordf_t grid_size, coordf_t width, coordf_t height)
{
    Polylines result;
    coordf_t  trans_layer_size  = grid_size * 0.4;          // 上部。
    coordf_t  repeat_layer_size = grid_size * repeat_ratio; // 下部。
    z_height                    += repeat_layer_size / 2 + trans_layer_size;   // 偏移以改善前几层的强度并降低翘曲风险。
    coordf_t  period            = trans_layer_size + repeat_layer_size;
    coordf_t  remains           = z_height - std::floor(z_height / period) * period;
    coordf_t  trans_z           = remains - repeat_layer_size; // 先放置重复层。
    coordf_t  repeat_z          = remains;

    int phase     = fmod(z_height, period * 2) - (period - 1); // 添加 epsilon
    int direction = phase <= 0 ? -1 : 1;

    // 这是一个重复层
    if (trans_z < 0) {
        result = generate_repeat_pattern(direction, grid_size, width, height);
    }
    // 这是一个过渡层
    else {
        double progress = fmod(trans_z, trans_layer_size) / trans_layer_size;

        // 将进度分割为前进和后退，方向相反。
        if (progress < 0.5)
            result = generate_transform_pattern((progress + 0.1) * 2, direction, grid_size, width, height); // 增加重叠。
        else
            result = generate_transform_pattern((1.1 - progress) * 2, -1 * direction, grid_size, width, height);
    }

    return result;
}

void FillCrossHatch ::_fill_surface_single(
    const FillParams &params, unsigned int thickness_layers, const std::pair<float, Point> &direction, ExPolygon expolygon, Polylines &polylines_out)
{
    // 旋转角度
    auto infill_angle = float(this->angle);
    if (std::abs(infill_angle) >= EPSILON) expolygon.rotate(-infill_angle);

    // 获取旋转后的边界框
    BoundingBox bb = expolygon.contour.bounding_box();

    // 线间距修正
    double density_adjusted = params.density / params.multiline;
    coord_t line_spacing = coord_t(scale_(this->spacing) / density_adjusted);

    // 降低密度
    if (params.density < 0.999) line_spacing *= 1.08;

    bb.merge(align_to_grid(bb.min, Point(line_spacing * 4, line_spacing * 4)));

    // 生成图案
    //Orca: 优化交叉阴影填充图案，以在低填充密度时提高强度。
    double repeat_ratio = 1.0;
    if (params.density < 0.3)
        repeat_ratio = std::clamp(1.0 - std::exp(-5 * params.density), 0.2, 1.0);

    Polylines polylines = generate_infill_layers(scale_(this->z), repeat_ratio, line_spacing, bb.size()(0), bb.size()(1));

    // 将图案平移到实际空间
    for (Polyline &pl : polylines) { pl.translate(bb.min); }

    // 如果需要，应用多线偏移
    multiline_fill(polylines, params, spacing);

    polylines = intersection_pl(polylines, to_polygons(expolygon));

    // --- 移除陀螺形填充的微小残留
    if (!polylines.empty()) {
        // 移除非常小的片段，但注意不要移除连接薄壁的填充线！
        // 填充周长线应间隔大约一个填充线宽。
        const double minlength = scale_(0.8 * this->spacing);
        polylines.erase(std::remove_if(polylines.begin(), polylines.end(), [minlength](const Polyline &pl)
            { return pl.length() < minlength; }), polylines.end());
    }

    if (!polylines.empty()) {
        int infill_start_idx = polylines_out.size(); // 仅旋转属于我们的部分。
        // 连接线
        chain_or_connect_infill(std::move(polylines), expolygon, polylines_out, this->spacing, params);

        // 旋转回来
        if (std::abs(infill_angle) >= EPSILON) {
            for (auto it = polylines_out.begin() + infill_start_idx; it != polylines_out.end(); ++it) it->rotate(infill_angle);
        }
    }
}

} // namespace Slic3r