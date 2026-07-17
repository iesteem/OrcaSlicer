#ifndef slic3r_ExtrusionProcessor_hpp_
#define slic3r_ExtrusionProcessor_hpp_

// 此算法从PrusaSlicer复制，原作者为Pavel Mikus(pavel.mikus.mail@seznam.cz)

#include "../AABBTreeLines.hpp"
//#include "../SupportSpotsGenerator.hpp"
#include "../libslic3r.h"
#include "../ExtrusionEntity.hpp"
#include "../Layer.hpp"
#include "../Point.hpp"
#include "../SVG.hpp"
#include "../BoundingBox.hpp"
#include "../Polygon.hpp"
#include "../ClipperUtils.hpp"
#include "../Flow.hpp"
#include "../Config.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Slic3r {

struct ExtendedPoint
{
    Vec2d position;
    float distance;
    float curvature;
};

template<bool SCALED_INPUT, bool ADD_INTERSECTIONS, bool PREV_LAYER_BOUNDARY_OFFSET, bool SIGNED_DISTANCE, typename POINTS, typename L>
std::vector<ExtendedPoint> estimate_points_properties(const POINTS                           &input_points,
                                                      const AABBTreeLines::LinesDistancer<L> &unscaled_prev_layer,
                                                      float                                   flow_width,
                                                      float                                   max_line_length = -1.0f,
                                                      float                                   min_distance = -1.0f)
{
    bool   looped     = input_points.front() == input_points.back();
    std::function<size_t(size_t,size_t)> get_prev_index = [](size_t idx, size_t count) {
        if (idx > 0) {
            return idx - 1;
        } else
            return idx;
    };
    if (looped) {
        get_prev_index = [](size_t idx, size_t count) {
            if (idx == 0)
                idx = count;
            return --idx;
        };
    };
    std::function<size_t(size_t,size_t)> get_next_index = [](size_t idx, size_t size) {
        if (idx + 1 < size) {
            return idx + 1;
        } else
            return idx;
    };
    if (looped) {
        get_next_index = [](size_t idx, size_t count) {
            if (++idx == count)
                idx = 0;
            return idx;
        };
    };

    using P = typename POINTS::value_type;
    // ORCA:
    // 任何新生成点的最小间距阈值
    // 将最小间距设置为流动宽度的25%，确保点之间有足够的间距
    // 以避免微小的停顿，同时打印头的移动粒度足够细以保持
    // 打印质量。
    double min_spacing = flow_width*0.25;

    using AABBScalar = typename AABBTreeLines::LinesDistancer<L>::Scalar;
    if (input_points.empty())
        return {};
    float boundary_offset = PREV_LAYER_BOUNDARY_OFFSET ? 0.5 * flow_width : 0.0f;
    auto  maybe_unscale   = [](const P &p) { return SCALED_INPUT ? unscaled(p) : p.template cast<double>(); };

    std::vector<ExtendedPoint> points;
    points.reserve(input_points.size() * (ADD_INTERSECTIONS ? 1.5 : 1));

    {
        ExtendedPoint start_point{maybe_unscale(input_points.front())};
        auto [distance, nearest_line,
              x] = unscaled_prev_layer.template distance_from_lines_extra<SIGNED_DISTANCE>(start_point.position.cast<AABBScalar>());
        start_point.distance = distance + boundary_offset;
        points.push_back(start_point);
    }
    for (size_t i = 1; i < input_points.size(); i++) {
        ExtendedPoint next_point{maybe_unscale(input_points[i])};
        auto [distance, nearest_line,
              x] = unscaled_prev_layer.template distance_from_lines_extra<SIGNED_DISTANCE>(next_point.position.cast<AABBScalar>());
        next_point.distance = distance + boundary_offset;

        // 交点处理
        if (ADD_INTERSECTIONS &&
            ((points.back().distance > boundary_offset + EPSILON) != (next_point.distance > boundary_offset + EPSILON))) {
            const ExtendedPoint &prev_point    = points.back();
            auto                 intersections = unscaled_prev_layer.template intersections_with_line<true>(
                L{prev_point.position.cast<AABBScalar>(), next_point.position.cast<AABBScalar>()});
            for (const auto &intersection : intersections) {
                ExtendedPoint p{};
                p.position = intersection.first.template cast<double>();
                p.distance = boundary_offset;
                // ORCA: 过滤掉在交点处引入的点，如果它们与前一个或下一个点的距离没有意义
                if ((p.position - prev_point.position).norm() > min_spacing &&
                    (next_point.position - p.position).norm() > min_spacing) {
                    points.push_back(p);
                }
            }
        }
        points.push_back(next_point);
    }

    // 分段处理
    if (PREV_LAYER_BOUNDARY_OFFSET && ADD_INTERSECTIONS) {
        std::vector<ExtendedPoint> new_points;
        new_points.reserve(points.size() * 2);
        new_points.push_back(points.front());
        for (int point_idx = 0; point_idx < int(points.size()) - 1; ++point_idx) {
            const ExtendedPoint &curr = points[point_idx];
            const ExtendedPoint &next = points[point_idx + 1];

            if ((curr.distance > -boundary_offset && curr.distance < boundary_offset + 2.0f) ||
                (next.distance > -boundary_offset && next.distance < boundary_offset + 2.0f)) {
                double line_len = (next.position - curr.position).norm();

                // ORCA: 仅当路径有将触发减速的悬垂且路径也相当大（即长度2mm或以上）时，
                // 通过添加额外的点将路径分割成较小的线段。
                // 如果起点/终点没有悬垂，不要分割它。
                // 如果悬垂分段控制被禁用（min_distance=-1），忽略此检查。
                if ((min_distance > 0 && ((std::abs(curr.distance) > min_distance) || (std::abs(next.distance) > min_distance)) && line_len >= 2.f) ||
                    (min_distance <= 0 && line_len > 4.0f)) {
                    double a0 = std::clamp((curr.distance + 3 * boundary_offset) / line_len, 0.0, 1.0);
                    double a1 = std::clamp(1.0f - (next.distance + 3 * boundary_offset) / line_len, 0.0, 1.0);
                    double t0 = std::min(a0, a1);
                    double t1 = std::max(a0, a1);

                    if (t0 < 1.0) {
                        Vec2d p0     = curr.position + t0 * (next.position - curr.position);
                        auto [p0_dist, p0_near_l,
                              p0_x] = unscaled_prev_layer.template distance_from_lines_extra<SIGNED_DISTANCE>(p0.cast<AABBScalar>());
                        ExtendedPoint new_p{};
                        new_p.position = p0;
                        new_p.distance = float(p0_dist + boundary_offset);
                        // ORCA: 仅当新点的悬垂距离将用于生成速度变化时才创建路径中的新点，
                        // 或者如果此选项被禁用（min_distance<=0）
                        if( (std::abs(p0_dist) > min_distance) || (min_distance<=0)){
                            // ORCA: 同时过滤掉在路径起点引入的点，如果它们与起点的距离
                            // 没有意义
                            if ((p0 - curr.position).norm() > min_spacing && (next.position - p0).norm() > min_spacing) {
                                new_points.push_back(new_p);
                            }
                        }
                    }
                    if (t1 > 0.0) {
                        Vec2d p1     = curr.position + t1 * (next.position - curr.position);
                        auto [p1_dist, p1_near_l,
                              p1_x] = unscaled_prev_layer.template distance_from_lines_extra<SIGNED_DISTANCE>(p1.cast<AABBScalar>());
                        ExtendedPoint new_p{};
                        new_p.position = p1;
                        new_p.distance = float(p1_dist + boundary_offset);
                        // ORCA: 仅当新点的悬垂距离将用于生成速度变化时才创建路径中的新点，
                        // 或者如果此选项被禁用（min_distance<=0）
                        if( (std::abs(p1_dist) > min_distance) || (min_distance<=0)){
                            // ORCA: 过滤掉在路径末尾引入的点，如果它们与末尾点的距离
                            // 没有意义
                            if ((p1 - curr.position).norm() > min_spacing && (next.position - p1).norm() > min_spacing) {
                                new_points.push_back(new_p);
                            }
                        }
                    }
                }
            }
            new_points.push_back(next);
        }
        points = std::move(new_points);
    }

    // 最大线长处理
    if (max_line_length > 0) {
        std::vector<ExtendedPoint> new_points;
        new_points.reserve(points.size() * 2);
        {
            for (size_t i = 0; i + 1 < points.size(); i++) {
                const ExtendedPoint &curr = points[i];
                const ExtendedPoint &next = points[i + 1];
                new_points.push_back(curr);
                double len             = (next.position - curr.position).squaredNorm();
                double t               = sqrt((max_line_length * max_line_length) / len);
                size_t new_point_count = 1.0 / t;
                for (size_t j = 1; j < new_point_count + 1; j++) {
                    Vec2d pos  = curr.position * (1.0 - j * t) + next.position * (j * t);
                    auto [p_dist, p_near_l,
                          p_x] = unscaled_prev_layer.template distance_from_lines_extra<SIGNED_DISTANCE>(pos.cast<AABBScalar>());
                    ExtendedPoint new_p{};
                    new_p.position = pos;
                    new_p.distance = float(p_dist + boundary_offset);

                    // ORCA: 过滤掉引入的点，如果它们与前一个或下一个点的距离没有意义
                    if ((pos - curr.position).norm() > min_spacing && (next.position - pos).norm() > min_spacing) {
                        new_points.push_back(new_p);
                    }
                }
            }
            new_points.push_back(points.back());
        }
        points = std::move(new_points);
    }

    // 曲率计算
    float accumulated_distance = 0;
    std::vector<float> distances_for_curvature(points.size());
    for (size_t point_idx = 0; point_idx < points.size(); ++point_idx) {
        const ExtendedPoint &a = points[point_idx];
        const ExtendedPoint &b = points[get_prev_index(point_idx, points.size())];

        distances_for_curvature[point_idx] = (b.position - a.position).norm();
        accumulated_distance += distances_for_curvature[point_idx];
    }

    if (accumulated_distance > EPSILON)
        for (float window_size : {3.0f, 9.0f, 16.0f}) {
            for (int point_idx = 0; point_idx < int(points.size()); ++point_idx) {
                ExtendedPoint &current = points[point_idx];

                Vec2d back_position = current.position;
                {
                    size_t back_point_index = point_idx;
                    float  dist_backwards   = 0;
                    while (dist_backwards < window_size * 0.5 && back_point_index != get_prev_index(back_point_index, points.size())) {
                        float line_dist = distances_for_curvature[get_prev_index(back_point_index, points.size())];
                        if (dist_backwards + line_dist > window_size * 0.5) {
                            back_position = points[back_point_index].position +
                                            (window_size * 0.5 - dist_backwards) *
                                                (points[get_prev_index(back_point_index, points.size())].position -
                                                 points[back_point_index].position)
                                                    .normalized();
                            dist_backwards += window_size * 0.5 - dist_backwards + EPSILON;
                        } else {
                            dist_backwards += line_dist;
                            back_point_index = get_prev_index(back_point_index, points.size());
                        }
                    }
                }

                Vec2d front_position = current.position;
                {
                    size_t front_point_index = point_idx;
                    float  dist_forwards     = 0;
                    while (dist_forwards < window_size * 0.5 && front_point_index != get_next_index(front_point_index, points.size())) {
                        float line_dist = distances_for_curvature[front_point_index];
                        if (dist_forwards + line_dist > window_size * 0.5) {
                            front_position = points[front_point_index].position +
                                             (window_size * 0.5 - dist_forwards) *
                                                 (points[get_next_index(front_point_index, points.size())].position -
                                                  points[front_point_index].position)
                                                     .normalized();
                            dist_forwards += window_size * 0.5 - dist_forwards + EPSILON;
                        } else {
                            dist_forwards += line_dist;
                            front_point_index = get_next_index(front_point_index, points.size());
                        }
                    }
                }

                float new_curvature = angle(current.position - back_position, front_position - current.position) / window_size;
                if (abs(current.curvature) < abs(new_curvature)) {
                    current.curvature = new_curvature;
                }
            }
        }

    return points;
}

struct ProcessedPoint
{
    Point p;
    float speed = 1.0f;
    float overlap = 1.0f;
};

class ExtrusionQualityEstimator
{
    std::unordered_map<const PrintObject *, AABBTreeLines::LinesDistancer<Linef>> prev_layer_boundaries;
    std::unordered_map<const PrintObject *, AABBTreeLines::LinesDistancer<Linef>> next_layer_boundaries;
    std::unordered_map<const PrintObject *, AABBTreeLines::LinesDistancer<CurledLine>> prev_curled_extrusions;
    std::unordered_map<const PrintObject *, AABBTreeLines::LinesDistancer<CurledLine>> next_curled_extrusions;
    const PrintObject                                                            *current_object;

public:
    void set_current_object(const PrintObject *object) { current_object = object; }

    void prepare_for_new_layer(const PrintObject * obj, const Layer *layer)
    {
        if (layer == nullptr) return;
        const PrintObject *object = obj;
        prev_layer_boundaries[object] = next_layer_boundaries[object];
        next_layer_boundaries[object] = AABBTreeLines::LinesDistancer<Linef>{to_unscaled_linesf(layer->lslices)};
        prev_curled_extrusions[object] = next_curled_extrusions[object];
        next_curled_extrusions[object] = AABBTreeLines::LinesDistancer<CurledLine>{layer->curled_lines};
    }

    std::vector<ProcessedPoint> estimate_extrusion_quality(const ExtrusionPath                &path,
                                                           const ConfigOptionPercents         &overlaps,
                                                           const ConfigOptionFloatsOrPercents &speeds,
                                                           float                               ext_perimeter_speed,
                                                           float                               original_speed,
                                                           bool                                slowdown_for_curled_edges)
    {
        size_t                               speed_sections_count = std::min(overlaps.values.size(), speeds.values.size());
        std::vector<std::pair<float, float>> speed_sections;



        for (size_t i = 0; i < speed_sections_count; i++) {
            float distance = path.width * (1.0 - (overlaps.get_at(i) / 100.0));
            float speed    = speeds.get_at(i).percent ? (ext_perimeter_speed * speeds.get_at(i).value / 100.0) : speeds.get_at(i).value;
            speed_sections.push_back({distance, speed});
        }
        std::sort(speed_sections.begin(), speed_sections.end(),
                  [](const std::pair<float, float> &a, const std::pair<float, float> &b) {
                    if (a.first == b.first) {
                        return a.second > b.second;
                    }
                    return a.first < b.first; });

        std::pair<float, float> last_section{INFINITY, 0};
        for (auto &section : speed_sections) {
            if (section.first == last_section.first) {
                section.second = last_section.second;
            } else {
                last_section = section;
            }
        }

        // Orca: 找到速度调整开始的最小悬垂距离
        float smallest_distance_with_lower_speed = std::numeric_limits<float>::infinity(); // 初始化为较大值
        bool found = false;
        for (const auto& section : speed_sections) {
            if (section.second <= original_speed) {
                if (section.first < smallest_distance_with_lower_speed) {
                    smallest_distance_with_lower_speed = section.first;
                    found = true;
                }
            }
        }

        // 如果未找到有意义的（即需要减速的）悬垂距离，则不应分割线条
        if (!found)
            smallest_distance_with_lower_speed=-1.f;

        // Orca: 将触发减速的最小悬垂距离传递给点属性估计器
        std::vector<ExtendedPoint> extended_points = estimate_points_properties<true, true, true, true>
                                                                (path.polyline.points,
                                                                 prev_layer_boundaries[current_object],
                                                                 path.width,
                                                                 -1,
                                                                 smallest_distance_with_lower_speed);
        const auto width_inv = 1.0f / path.width;
        std::vector<ProcessedPoint> processed_points;
        processed_points.reserve(extended_points.size());
        for (size_t i = 0; i < extended_points.size(); i++) {
            const ExtendedPoint &curr = extended_points[i];
            const ExtendedPoint &next = extended_points[i + 1 < extended_points.size() ? i + 1 : i];

            float artificial_distance_to_curled_lines = 0.0;
            if(slowdown_for_curled_edges) {
                // 以下代码人为增加距离，以对卷曲线条上的挤出提供减速
                const double dist_limit = 10.0 * path.width;
                {
                Vec2d middle = 0.5 * (curr.position + next.position);
                auto line_indices = prev_curled_extrusions[current_object].all_lines_in_radius(Point::new_scale(middle), scale_(dist_limit));
                    if (!line_indices.empty()) {
                        double len   = (next.position - curr.position).norm();
                        // 对于长线，额外的减速存在问题。如果中间附近意外出现小的卷曲线条，
                        // 整个段会不必要地变慢。对于这些长线，我们进行额外检查以确定是否值得减速。
                        // 请注意这仍然是相当粗略的近似，例如我们仍然只检查中点附近的线条
                        // TODO 也许在运行此算法之前将线分割成更小的段？但可能要求很高，而且G-code会很大
                        if (len > 2) {
                            Vec2d dir   = Vec2d(next.position - curr.position) / len;
                            Vec2d right = Vec2d(-dir.y(), dir.x());

                            Polygon box_of_influence = {
                                scaled(Vec2d(curr.position + right * dist_limit)),
                                scaled(Vec2d(next.position + right * dist_limit)),
                                scaled(Vec2d(next.position - right * dist_limit)),
                                scaled(Vec2d(curr.position - right * dist_limit)),
                            };

                            double projected_lengths_sum = 0;
                            for (size_t idx : line_indices) {
                                const CurledLine &line   = prev_curled_extrusions[current_object].get_line(idx);
                                Lines             inside = intersection_ln({{line.a, line.b}}, {box_of_influence});
                                if (inside.empty())
                                    continue;
                                double projected_length = abs(dir.dot(unscaled(Vec2d((inside.back().b - inside.back().a).cast<double>()))));
                                projected_lengths_sum += projected_length;
                            }
                            if (projected_lengths_sum < 0.4 * len) {
                                line_indices.clear();
                            }
                        }

                        for (size_t idx : line_indices) {
                            const CurledLine &line                 = prev_curled_extrusions[current_object].get_line(idx);
                            float             distance_from_curled = unscaled(line_alg::distance_to(line, Point::new_scale(middle)));
                            float             dist                 = path.width * (1.0 - (distance_from_curled / dist_limit)) *
                                     (1.0 - (distance_from_curled / dist_limit)) *
                                     (line.curled_height / (path.height * 10.0f)); // 来自SupportSpotGenerator的max_curled_height_factor
                            artificial_distance_to_curled_lines = std::max(artificial_distance_to_curled_lines, dist);
                        }
                    }
                }
            }

            auto calculate_speed = [&speed_sections, &original_speed](float distance) {
                float final_speed;
                if (distance <= speed_sections.front().first) {
                    final_speed = original_speed;
                } else if (distance >= speed_sections.back().first) {
                    final_speed = speed_sections.back().second;
                } else {
                    size_t section_idx = 0;
                    while (distance > speed_sections[section_idx + 1].first) {
                        section_idx++;
                    }
                    float t = (distance - speed_sections[section_idx].first) /
                              (speed_sections[section_idx + 1].first - speed_sections[section_idx].first);
                    t           = std::clamp(t, 0.0f, 1.0f);
                    final_speed = (1.0f - t) * speed_sections[section_idx].second + t * speed_sections[section_idx + 1].second;
                }
                return round(final_speed);
            };

            float extrusion_speed = std::min(calculate_speed(curr.distance), calculate_speed(next.distance));
            // ORCA: 将结果速度限制为基于悬垂值计算的速度和当前速度中的最小值
            // 修复由于（例如）体积流量限制导致的结果悬垂速度高于当前速度的错误
            extrusion_speed = std::min(extrusion_speed, original_speed);

            if(slowdown_for_curled_edges) {
                float curled_speed = calculate_speed(artificial_distance_to_curled_lines);
                extrusion_speed       = std::min(curled_speed, extrusion_speed); // 根据计算出的悬垂速度或人为卷曲速度中的最小值调整挤出速度
            }

            float overlap = std::min(1 - (curr.distance+artificial_distance_to_curled_lines) * width_inv, 1 - (next.distance+artificial_distance_to_curled_lines) * width_inv);

            processed_points.push_back({ scaled(curr.position), extrusion_speed, overlap });
        }
        return processed_points;
    }
};

} // namespace Slic3r

#endif // slic3r_ExtrusionProcessor_hpp_
