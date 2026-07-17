//Copyright (c) 2020 Ultimaker B.V.
//CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "ExtrusionLine.hpp"
#include "../../VariableWidth.hpp"
#include "libslic3r/Arachne/utils/ExtrusionJunction.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/Line.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/Polyline.hpp"

namespace Slic3r {
class Flow;
}  // namespace Slic3r

namespace Slic3r::Arachne
{

ExtrusionLine::ExtrusionLine(const size_t inset_idx, const bool is_odd) : inset_idx(inset_idx), is_odd(is_odd), is_closed(false) {}

int64_t ExtrusionLine::getLength() const
{
    if (junctions.empty())
        return 0;

    int64_t           len  = 0;
    ExtrusionJunction prev = junctions.front();
    for (const ExtrusionJunction &next : junctions) {
        len += (next.p - prev.p).cast<int64_t>().norm();
        prev = next;
    }
    if (is_closed)
        len += (front().p - back().p).cast<int64_t>().norm();

    return len;
}

void ExtrusionLine::simplify(const int64_t smallest_line_segment_squared, const int64_t allowed_error_distance_squared, const int64_t maximum_extrusion_area_deviation)
{
    const size_t min_path_size = is_closed ? 3 : 2;
    if (junctions.size() <= min_path_size)
        return;

    /* ExtrusionLine 被视为（开放）折线，因此如果 ExtrusionLine 实际上是闭合多边形，其
     * 起点和终点将相等（或几乎相等）。因此，ExtrusionLine 的简化
     * 不应触碰第一个和最后一个点。因此，从索引 1 的点开始简化。
     * */
    std::vector<ExtrusionJunction> new_junctions;
    //// 起始连接点应始终存在于简化路径中
    new_junctions.emplace_back(junctions.front());

    ExtrusionJunction previous = junctions.front();
    /* 对于开放 ExtrusionLine，检查索引 1 处的点时不能考虑最后一个连接点。
     * 对于闭合 ExtrusionLine，第一个和最后一个连接点相同，因此使用倒数第二个连接点。
     * */
    ExtrusionJunction previous_previous = this->is_closed ? junctions[junctions.size() - 2] : junctions.front();

    /* TODO: 在删除、合并或修改连接点时，最好将新连接点的宽度设为其
     * 派生来源连接点的加权平均值。
     */

    /* 删除顶点时，我们检查简化从原始多边形中移除的三角形区域的高度。
     * 然而，当连续删除多个顶点时，先前删除的顶点相对于
     * 快捷路径的高度会发生变化。
     * 为了不重新计算之前删除顶点的新高度值，
     * 我们计算代表三角形的高度，其覆盖的面积与被切掉的面积相同。
     * 我们使用鞋带公式（Shoelace formula）累加被删除线段下的面积。这通过
     * 计算一个"扇形"中的面积来实现，其中扇形的每个叶片从
     * 原点到一个线段。删除顶点时，此扇形的面积
     * 累加。通过减去连接到
     * 快捷线段的叶片面积，我们得到被切除区域的总面积。
     * 从该面积我们使用标准三角形面积公式计算代表三角形的高度：A = .5*b*h
     */
    const ExtrusionJunction& initial = junctions[1];
    int64_t accumulated_area_removed = int64_t(previous.p.x()) * int64_t(initial.p.y()) - int64_t(previous.p.y()) * int64_t(initial.p.x()); // 每条线段的鞋带公式面积的两倍。

    //// 对于闭合多边形，我们处理最后一个点，其与第一个点相同。
    for (size_t point_idx = 1; point_idx < junctions.size() - (this->is_closed ? 0 : 1); point_idx++)
    {
        //// 对于闭合多边形的最后一个点，如果我们修改了第一个点，则使用新多边形的第一个点。
        const bool is_last = point_idx + 1 == junctions.size();
        const ExtrusionJunction& current = is_last ? new_junctions[0] : junctions[point_idx];

        //// 不要将闭合多边形简化到低于 3 个连接点。
        if (this->is_closed && new_junctions.size() + (junctions.size() - point_idx) <= 3) {
            new_junctions.push_back(current);
            continue;
        }

        //// 在溢出情况下溢出，除非下一个顶点将等于上一个。
        const bool spill_over = this->is_closed && point_idx + 2 >= junctions.size() &&
            point_idx + 2 - junctions.size() < new_junctions.size();
        ExtrusionJunction& next = spill_over ? new_junctions[point_idx + 2 - junctions.size()] : junctions[point_idx + 1];

        const int64_t removed_area_next = int64_t(current.p.x()) * int64_t(next.p.y()) - int64_t(current.p.y()) * int64_t(next.p.x()); // 每条线段的鞋带公式面积的两倍。
        const int64_t negative_area_closing = int64_t(next.p.x()) * int64_t(previous.p.y()) - int64_t(next.p.y()) * int64_t(previous.p.x()); // 原点和快捷线段之间的面积
        accumulated_area_removed += removed_area_next;

        const int64_t length2 = (current - previous).cast<int64_t>().squaredNorm();
        if (length2 < scaled<coord_t>(0.025))
        {
            //// 我们始终允许删除小于 5 微米的线段。此情况下的宽度无关紧要。
            continue;
        }

        const int64_t area_removed_so_far = accumulated_area_removed + negative_area_closing; // 闭合快捷区域多边形
        const int64_t base_length_2 = (next - previous).cast<int64_t>().squaredNorm();

        if (base_length_2 == 0) // 两个线段形成一条来回的直线，没有面积。
        {
            continue; // 删除连接点（顶点）。
        }
        //// 我们要检查由上一个、当前和下一个顶点形成的三角形的高度是否小于 allowed_error_distance_squared。
        //// 1/2 L = A           [实际面积是计算出的鞋带值的一半] // 鞋带公式是 .5*(...)，但我们简化计算并去掉 .5
        //// A = 1/2 * b * h     [三角形面积公式]
        //// L = b * h           [应用上述两个并去掉 1/2]
        //h = L / b           [除以 b]
        //// h^2 = (L / b)^2     [平方]
        //// h^2 = L^2 / b^2     [分解除数]
        const auto    height_2 = int64_t(double(area_removed_so_far) * double(area_removed_so_far) / double(base_length_2));
        const int64_t extrusion_area_error = calculateExtrusionAreaDeviationError(previous, current, next);
        if ((height_2 <= scaled<coord_t>(0.001) //几乎完全共线（舍入误差范围内）。
             && Line::distance_to_infinite(current.p, previous.p, next.p) <= scaled<double>(0.001)) // 确保 height_2 不是由于正负面积抵消而变小
            //// 我们不应移除共线段的中点，如果 C-P 段的面积变化超过允许的最大值
             && extrusion_area_error <= maximum_extrusion_area_deviation)
        {
            //// 移除当前连接点（顶点）。
            continue;
        }

        if (length2 < smallest_line_segment_squared
            && height_2 <= allowed_error_distance_squared) // 删除连接点（顶点）不会引入太多误差。
        {
            const int64_t next_length2 = (current - next).cast<int64_t>().squaredNorm();
            if (next_length2 > 4 * smallest_line_segment_squared)
            {
                //// 特殊情况；下一个线很长。如果我们移除它，可能会产生相当明显的伪影。
                //// 我们应改为将此点移动到一个位置，使得两条边都被保留，然后移除我们原本想保留的上一个点。
                //// 通过取这两条线的交点，我们得到一个保留方向（使拐角更尖）的点。
                //// 我们只需确保交点本身不引入伪影。
                //// o < prev_prev
                //                |
                //// o < prev
                //// \  < short 段
                //// 交 > +   o-------------------o < 下一个
                //                    ^ current
                Point intersection_point;
                bool has_intersection = Line(previous_previous.p, previous.p).intersection_infinite(Line(current.p, next.p), &intersection_point);
                const auto dist_greater = [](const Point& p1, const Point& p2, const int64_t threshold) {
                    const auto vec = (p1 - p2).cwiseAbs().cast<uint64_t>().eval();
                    if(vec.x() > threshold || vec.y() > threshold) {
                        //// 如果此条件为 true，则距离肯定大于阈值。
                        //// 我们根本不需要计算平方范数，这避免了潜在的算术溢出。
                        return true;
                    }
                    return vec.squaredNorm() > threshold;
                };
                if (!has_intersection
                    || Line::distance_to_infinite_squared(intersection_point, previous.p, current.p) > double(allowed_error_distance_squared)
                    || dist_greater(intersection_point, previous.p, smallest_line_segment_squared)  // 交点距离"上一个"点太远
                    || dist_greater(intersection_point, current.p, smallest_line_segment_squared))  // 和"当前"点都太远，因此不应替换"当前"点
                {
                    //// 我们找不到更好的位置，但线的长度超过 5 微米。
                    //// 所以我们唯一能做的就是保留它...
                }
                else
                {
                    //// 新点似乎是一个有效的点。
                    const ExtrusionJunction new_to_add = ExtrusionJunction(intersection_point, current.w, current.perimeter_index);
                    //// 如果之前添加了一个点，则移除它。
                    if(!new_junctions.empty())
                    {
                        new_junctions.pop_back();
                        previous = previous_previous;
                    }

                    //// 连接点（顶点）被新点替换。
                    accumulated_area_removed = removed_area_next; // 这样在下一次迭代中，它是原点、[previous] 和 [current] 之间的面积
                    previous_previous = previous;
                    previous = new_to_add; // 注意：只有当我们不删除连接点（顶点）时，"previous"才会更新。
                    new_junctions.push_back(new_to_add);
                    continue;
                }
            }
            else
            {
                continue; // 删除连接点（顶点）。
            }
        }
        //// 连接点（顶点）未被删除。
        accumulated_area_removed = removed_area_next; // 这样在下一次迭代中，它是原点、[previous] 和 [current] 之间的面积
        previous_previous = previous;
        previous = current; // 注意：只有当我们不删除连接点（顶点）时，"previous"才会更新。
        new_junctions.push_back(current);
    }

    if (this->is_closed) {
        /* 对于闭合多边形，第一个和最后一个点应相同。
         * 我们在上面处理了最后一个点，因此将其复制到第一个点。
         */
        new_junctions.front().p = new_junctions.back().p;
    } else {
        //// 结束连接点（顶点）应始终存在于简化路径中
        new_junctions.emplace_back(junctions.back());
    }

    junctions = new_junctions;
}

int64_t ExtrusionLine::calculateExtrusionAreaDeviationError(ExtrusionJunction A, ExtrusionJunction B, ExtrusionJunction C) {
    /*
     * A             B                          C              A                                        C
     * ---------------                                         **************
     * |             |                                         ------------------------------------------
     * |             |--------------------------|  B removed   |            |***************************|
     * |             |                          |  --------->  |            |                           |
     * |             |--------------------------|              |            |***************************|
     * |             |                                         ------------------------------------------
     * ---------------             ^                           **************
     *       ^                B.w + C.w / 2                                       ^
     *  A.w + B.w / 2                                               new_width = 加权平均宽度
     *
     *
     * ******** 表示连续线段中由于对整个挤出线使用加权平均宽度而导致的总挤出面积偏差误差。
     *
     * */
    const int64_t ab_length = (B.p - A.p).cast<int64_t>().norm();
    const int64_t bc_length = (C.p - B.p).cast<int64_t>().norm();
    if (const coord_t width_diff = std::max(std::abs(B.w - A.w), std::abs(C.w - B.w)); width_diff > 1) {
        //// 仅在存在差异时调整宽度，否则舍入误差可能产生错误的加权平均值。
        const int64_t ab_weight              = (A.w + B.w) / 2;
        const int64_t bc_weight              = (B.w + C.w) / 2;
        const int64_t weighted_average_width = (ab_length * ab_weight + bc_length * bc_weight) / (ab_length + bc_length);
        const int64_t ac_length              = (C.p - A.p).cast<int64_t>().norm();
        return std::abs((ab_weight * ab_length + bc_weight * bc_length) - (weighted_average_width * ac_length));
    } else {
        //// 如果宽度差异非常小，则选择较长线段中的宽度
        return ab_length > bc_length ? int64_t(width_diff) * bc_length : int64_t(width_diff) * ab_length;
    }
}

bool ExtrusionLine::is_contour() const
{
    if (!this->is_closed)
        return false;

    Polygon poly;
    poly.points.reserve(this->junctions.size());
    for (const ExtrusionJunction &junction : this->junctions)
        poly.points.emplace_back(junction.p);

    //// Arachne 生成顺时针方向的轮廓和逆时针方向的孔。
    return poly.is_clockwise();
}

double ExtrusionLine::area() const
{
    assert(this->is_closed);
    double a = 0.;
    if (this->junctions.size() >= 3) {
        Vec2d p1 = this->junctions.back().p.cast<double>();
        for (const ExtrusionJunction &junction : this->junctions) {
            Vec2d p2 = junction.p.cast<double>();
            a += cross2(p1, p2);
            p1 = p2;
        }
    }
    return 0.5 * a;
}

} // namespace Slic3r::Arachne

namespace Slic3r {
void extrusion_paths_append(ExtrusionPaths &dst, const ClipperLib_Z::Paths &extrusion_paths, const ExtrusionRole role, const Flow &flow)
{
    for (const ClipperLib_Z::Path &extrusion_path : extrusion_paths) {
        ThickPolyline thick_polyline = Arachne::to_thick_polyline(extrusion_path);
        Slic3r::append(dst, thick_polyline_to_multi_path(thick_polyline, role, flow, scaled<float>(0.05), float(SCALED_EPSILON)).paths);
    }
}

void extrusion_paths_append(ExtrusionPaths &dst, const Arachne::ExtrusionLine &extrusion, const ExtrusionRole role, const Flow &flow)
{
    ThickPolyline thick_polyline = Arachne::to_thick_polyline(extrusion);
    Slic3r::append(dst, thick_polyline_to_multi_path(thick_polyline, role, flow, scaled<float>(0.05), float(SCALED_EPSILON)).paths);
}
} // namespace Slic3r
