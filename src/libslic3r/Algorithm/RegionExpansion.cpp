﻿#include "RegionExpansion.hpp"

#include <libslic3r/AABBTreeIndirect.hpp>
#include <libslic3r/ClipperZUtils.hpp>
#include <libslic3r/ClipperUtils.hpp>
#include <libslic3r/Utils.hpp>

#include <numeric>

namespace Slic3r {
namespace Algorithm {

// 根据 ClipperLib 偏移代码计算半径离散化，参见 void ClipperOffset::DoOffset(double delta)
inline double clipper_round_offset_error(double offset, double arc_tolerance)
{
    static constexpr const double def_arc_tolerance = 0.25;
    const double y =
        arc_tolerance <= 0 ?
            def_arc_tolerance :
            arc_tolerance > offset * def_arc_tolerance ?
                offset * def_arc_tolerance :
                arc_tolerance;
    double steps = std::min(M_PI / std::acos(1. - y / offset), offset * M_PI);
    return offset * (1. - cos(M_PI / steps));
}

RegionExpansionParameters RegionExpansionParameters::build(
    // 缩放的扩展值
    float                full_expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float                expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t               max_nr_expansion_steps)
{
    assert(full_expansion > 0);
    assert(expansion_step > 0);
    assert(max_nr_expansion_steps > 0);

    RegionExpansionParameters out;
    // 初始扩展源区域，使其与边界区域有少量交集。
    // 扩展不应过小，但也要足够小，以便后续扩展能补偿 tiny_expansion 并将波浪带回边界，
    // 而不会在边界接触处产生不美观的尖点。
    out.tiny_expansion = std::min(0.25f * full_expansion, scaled<float>(0.05f));
    size_t nsteps = size_t(ceil((full_expansion - out.tiny_expansion) / expansion_step));
    if (max_nr_expansion_steps > 0)
        nsteps = std::min(nsteps, max_nr_expansion_steps);
    assert(nsteps > 0);
    out.initial_step = (full_expansion - out.tiny_expansion) / nsteps;
    if (nsteps > 1 && 0.25 * out.initial_step < out.tiny_expansion) {
        // 通过减少步数来减小步长。
        nsteps       = std::max<size_t>(1, (floor((full_expansion - out.tiny_expansion) / (4. * out.tiny_expansion))));
        out.initial_step = (full_expansion - out.tiny_expansion) / nsteps;
    }
    if (0.25 * out.initial_step < out.tiny_expansion || nsteps == 1) {
        out.tiny_expansion = 0.2f * full_expansion;
        out.initial_step   = 0.8f * full_expansion;
    }
    out.other_step           = out.initial_step;
    out.num_other_steps      = nsteps - 1;

    // 波浪传播的偏移精度。
    out.arc_tolerance        = scaled<double>(0.1);
    out.shortest_edge_length = out.initial_step * ClipperOffsetShortestEdgeFactor;

    // 种子轮廓在边界上的最大膨胀量。用于裁剪边界以加速波浪传播期间的裁剪。
    // 需要与偏移器精度保持同步。
    // Clipper 正圆角偏移应当偏移不足而非过度。
    // 但仍添加了一点额外偏移。
    out.max_inflation = (out.tiny_expansion + nsteps * out.initial_step) * 1.1;
//// (用于 uncertainty 的 clipper_round_offset_error(out.tiny_expansion, co.ArcTolerance) + nsteps * clipper_round_offset_error(out.initial_step, co.ArcTolerance) * 1.5; // Account

    return out;
}

// 类似于 expolygons_to_zpaths()，但每个轮廓在转换为 zpath 之前先进行扩展。
// 扩展后的轮廓随后被打开（第一个点在末尾重复）。
static ClipperLib_Z::Paths expolygons_to_zpaths_expanded_opened(
    const ExPolygons &src, const float expansion, coord_t &base_idx)
{
    ClipperLib_Z::Paths out;
    out.reserve(2 * std::accumulate(src.begin(), src.end(), size_t(0),
        [](const size_t acc, const ExPolygon &expoly) { return acc + expoly.num_contours(); }));
    ClipperLib::ClipperOffset offsetter;
    offsetter.ShortestEdgeLength = expansion * ClipperOffsetShortestEdgeFactor;
    ClipperLib::Paths expansion_cache;
    for (const ExPolygon &expoly : src) {
        for (size_t icontour = 0; icontour < expoly.num_contours(); ++ icontour) {
            // Execute 会重新定向轮廓，使最外层轮廓具有正面积。因此输出轮廓将是 CCW 方向，
            // 即使输入路径是 CW 方向。
            // 偏移是在轮廓重新定向后应用的，因此偏移值的符号被反转。
            offsetter.Clear();
            offsetter.AddPath(expoly.contour_or_hole(icontour).points, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
            expansion_cache.clear();
            offsetter.Execute(expansion_cache, icontour == 0 ? expansion : -expansion);
            append(out, ClipperZUtils::to_zpaths<true>(expansion_cache, base_idx));
        }
        ++ base_idx;
    }
    return out;
}

// 路径是通过将闭合多边形分割为开放路径，然后裁剪它们而创建的。
// 因此，裁剪后的多边形的某些部分可能会在源多边形的端点处被分割。
// 这些端点按字典序排序在 "splits" 中。
// 重新连接这些被分割的部分。
static inline void merge_splits(ClipperLib_Z::Paths &paths, std::vector<std::pair<ClipperLib_Z::IntPoint, int>> &splits)
{
    for (auto it_path = paths.begin(); it_path != paths.end(); ) {
        ClipperLib_Z::Path &path = *it_path;
        assert(path.size() >= 2);
        bool merged = false;
        if (path.size() >= 2) {
            const ClipperLib_Z::IntPoint &front = path.front();
            const ClipperLib_Z::IntPoint &back  = path.back();
            // 裁剪前的路径本应穿越裁剪边界或完全在其外部。
            // 因此裁剪后的轮廓应变为开放状态，只有一个例外：锚点扩展成一个闭合孔洞。
            if (front.x() != back.x() || front.y() != back.y()) {
                // 在 "splits" 中查找端点，可能连接轮廓。
                // "splits" 映射到连接到同一端点的其他片段。
                auto find_end = [&splits](const ClipperLib_Z::IntPoint &pt) -> std::pair<ClipperLib_Z::IntPoint, int>* {
                    auto it = std::lower_bound(splits.begin(), splits.end(), pt,
                        [](const auto &l, const auto &r){ return ClipperZUtils::zpoint_lower(l.first, r); });
                    return it != splits.end() && it->first == pt ? &(*it) : nullptr;
                };
                auto *end = find_end(front);
                bool  end_front = true;
                if (! end) {
                    end_front = false;
                    end = find_end(back);
                }
                if (end) {
                    // 此线段在裁剪前终止于源闭合轮廓的一个分割点处。
                    if (end->second == -1) {
                        // 找到了开放端，尚未匹配。
                        end->second = int(it_path - paths.begin());
                    } else {
                        // 找到了开放端并与 end->second 匹配
                        ClipperLib_Z::Path &other_path = paths[end->second];
                        polylines_merge(other_path, other_path.front() == end->first, std::move(path), end_front);
                        if (std::next(it_path) == paths.end()) {
                            paths.pop_back();
                            break;
                        }
                        path = std::move(paths.back());
                        paths.pop_back();
                        merged = true;
                    }
                }
            }
        }
        if (! merged)
            ++ it_path;
    }
}

using AABBTreeBBoxes = AABBTreeIndirect::Tree<2, coord_t>;

static AABBTreeBBoxes build_aabb_tree_over_expolygons(const ExPolygons &expolygons) 
{
    // 计算内部切片的边界框。
    std::vector<AABBTreeIndirect::BoundingBoxWrapper> bboxes;
    bboxes.reserve(expolygons.size());
    for (size_t i = 0; i < expolygons.size(); ++ i)
        bboxes.emplace_back(i, get_extents(expolygons[i].contour));
    // 在边界扩展多边形的边界框上构建 AABB 树。
    AABBTreeBBoxes out;
    out.build_modify_input(bboxes);
    return out;
}

static int sample_in_expolygons(
    // 边界扩展多边形上的 AABB 树
    const AABBTreeBBoxes &aabb_tree,
    const ExPolygons     &expolygons,
    const Point          &sample)
{
    int out = -1;
    AABBTreeIndirect::traverse(aabb_tree,
        [&sample](const AABBTreeBBoxes::Node &node) {
            return node.bbox.contains(sample);
        },
        [&expolygons, &sample, &out](const AABBTreeBBoxes::Node &node) {
            assert(node.is_leaf());
            assert(node.is_valid());
            if (expolygons[node.idx].contains(sample)) {
                out = int(node.idx);
                // 停止遍历。
                return false;
            }
            // 继续遍历。
            return true;
        });
    return out;
}

std::vector<WaveSeed> wave_seeds(
    // 应当接触边界的源区域。
    const ExPolygons      &src,
    // 接触"边界"区域的源区域将被扩展到该"边界"区域中。
    const ExPolygons      &boundary,
    // 初始扩展源区域，使其与边界区域有少量交集。
    float                  tiny_expansion,
    // 按边界 ID 和源 ID 排序输出。
    bool                   sorted)
{
    assert(tiny_expansion > 0);

    if (src.empty() || boundary.empty())
        return {};

    using Intersection  = ClipperZUtils::ClipperZIntersectionVisitor::Intersection;
    using Intersections = ClipperZUtils::ClipperZIntersectionVisitor::Intersections;

    ClipperLib_Z::Paths segments;
    Intersections       intersections;

    coord_t             idx_boundary_begin = 1;
    coord_t             idx_boundary_end   = idx_boundary_begin;
    coord_t             idx_src_end;

    {
        ClipperLib_Z::Clipper zclipper;
        ClipperZUtils::ClipperZIntersectionVisitor visitor(intersections);
        zclipper.ZFillFunction(visitor.clipper_callback());
        // 作为闭合轮廓
        zclipper.AddPaths(ClipperZUtils::expolygons_to_zpaths(boundary, idx_boundary_end), ClipperLib_Z::ptClip, true);
        // 作为开放轮廓
        std::vector<std::pair<ClipperLib_Z::IntPoint, int>> zsrc_splits;
        {
            idx_src_end = idx_boundary_end;
            ClipperLib_Z::Paths zsrc = expolygons_to_zpaths_expanded_opened(src, tiny_expansion, idx_src_end);
            zclipper.AddPaths(zsrc, ClipperLib_Z::ptSubject, false);
            zsrc_splits.reserve(zsrc.size());
            for (const ClipperLib_Z::Path &path : zsrc) {
                assert(path.size() >= 2);
                assert(path.front() == path.back());
                zsrc_splits.emplace_back(path.front(), -1);
            }
            std::sort(zsrc_splits.begin(), zsrc_splits.end(), [](const auto &l, const auto &r){ return ClipperZUtils::zpoint_lower(l.first, r.first); });
        }
        ClipperLib_Z::PolyTree polytree;
        zclipper.Execute(ClipperLib_Z::ctIntersection, polytree, ClipperLib_Z::pftNonZero, ClipperLib_Z::pftNonZero);
        ClipperLib_Z::PolyTreeToPaths(std::move(polytree), segments);
        merge_splits(segments, zsrc_splits);
    }

    // 在边界框上构建 AABBTree。
    // 仅在必要时构建，即当任意种子轮廓是闭合的，因此没有与边界的交点，
    // 并且闭合轮廓的所有 Z 坐标都指向源轮廓时。
    AABBTreeBBoxes aabb_tree;

    // 将路径分类到各自的岛屿中。
    // 每个源区域与边界的配对将独立处理（波浪扩展）。
    // 单个源区域的多个片段可能与同一边界相交。
    WaveSeeds out;
    out.reserve(segments.size());
    int iseed = 0;
    for (const ClipperLib_Z::Path &path : segments) {
        assert(path.size() >= 2);
        ClipperLib_Z::IntPoint front = path.front();
        ClipperLib_Z::IntPoint back  = path.back();
        //// Both ends of a seed 段 是 supposed to 为 内部 a 单个 边界 expolygon.
        //// Thus as long as the seed 轮廓 是 不 closed, it 应 为 open at a 边界 点.
        assert((front == back && front.z() >= idx_boundary_end && front.z() < idx_src_end) || 
            //// (前.z() < 0 && 后.z() < 0));
            //// Hope 该 at least one 结束 of an open 折线 是 clipped by the 边界, thus an 相交 点 是 created.
            (front.z() < 0 || back.z() < 0));

        if (front != back && front.z() >= 0 && back.z() >= 0) {
            //// 非常 rare 情况 当 both endpoints intersect 边界 ExPolygons in existing 点.
            //// So the ZFillFunction callback hasn't been called.
            continue;
        } else
        if (front == back && (front.z() < idx_boundary_end)) {
            //// 此 应 为 a 非常 rare 异常.
            //// See https://github.com/prusa3d/PrusaSlicer/issues/12469.
            //// Segement 是 open, 尚 its 第一个 点 seems to 为 part of 边界 多边形.
            //// Take the 第一个 点 with src 多边形 索引.
            for (const ClipperLib_Z::IntPoint &point : path) {
                if (point.z() >= idx_boundary_end) {
                    front = point;
                    back = point;
                }
            }
        }

        const Intersection *intersection = nullptr;
        auto intersection_point_valid = [idx_boundary_end, idx_src_end](const Intersection &is) {
            return is.first >= 1 && is.first < idx_boundary_end &&
                   is.second >= idx_boundary_end && is.second < idx_src_end;
        };
        if (front.z() < 0) {
            const Intersection &is = intersections[- front.z() - 1];
            assert(intersection_point_valid(is));
            if (intersection_point_valid(is))
                intersection = &is;
        }
        if (! intersection && back.z() < 0) {
            const Intersection &is = intersections[- back.z() - 1];
            assert(intersection_point_valid(is));
            if (intersection_point_valid(is))
                intersection = &is;
        }
        if (intersection) {
            //// The 路径 intersects the 边界 轮廓 at least at one side.
            out.push_back({ uint32_t(intersection->second - idx_boundary_end), uint32_t(intersection->first - 1), ClipperZUtils::from_zpath(path) });
        } else {
            // 这应该是一个闭合轮廓。
            assert(front == back && front.z() >= idx_boundary_end && front.z() < idx_src_end);
            // 查找此闭合路径的一个样本的源边界扩展多边形。
            if (aabb_tree.empty())
                aabb_tree = build_aabb_tree_over_expolygons(boundary);
            int boundary_id = sample_in_expolygons(aabb_tree, boundary, Point(front.x(), front.y()));
            // 找到包含采样点的边界。
            assert(boundary_id >= 0);
            if (boundary_id >= 0)
                out.push_back({ uint32_t(front.z() - idx_boundary_end), uint32_t(boundary_id), ClipperZUtils::from_zpath(path) });
        }
        ++ iseed;
    }

    if (sorted)
        // 按相交边界和源轮廓对种子进行排序。
        std::sort(out.begin(), out.end(), lower_by_boundary_and_src);
    return out;
}

static ClipperLib::Paths wavefront_initial(ClipperLib::ClipperOffset &co, const ClipperLib::Paths &polylines, float offset)
{
    ClipperLib::Paths out;
    out.reserve(polylines.size());
    ClipperLib::Paths out_this;
    for (const ClipperLib::Path &path : polylines) {
        assert(path.size() >= 2);
        co.Clear();
        co.AddPath(path, jtRound, path.front() == path.back() ? ClipperLib::etClosedLine : ClipperLib::etOpenRound);
        co.Execute(out_this, offset);
        append(out, std::move(out_this));
    }
    return out;
}

// 输入多边形可能包含多个扩展多边形，甚至嵌套的扩展多边形。
// 因此，膨胀后某些多边形可能重叠，但重叠将在后续的裁剪操作中解决，因此此处不进行处理。
static ClipperLib::Paths wavefront_step(ClipperLib::ClipperOffset &co, const ClipperLib::Paths &polygons, float offset)
{
    ClipperLib::Paths out;
    out.reserve(polygons.size());
    ClipperLib::Paths out_this;
    for (const ClipperLib::Path &polygon : polygons) {
        co.Clear();
        // Execute 会重新定向轮廓，使最外层轮廓具有正面积。因此输出轮廓将是 CCW 方向，
        // 即使输入路径是 CW 方向。
        // 偏移是在轮廓重新定向后应用的，因此偏移值的符号被反转。
        co.AddPath(polygon, jtRound, ClipperLib::etClosedPolygon);
        bool ccw = ClipperLib::Orientation(polygon);
        co.Execute(out_this, ccw ? offset : - offset);
        if (! ccw) {
            // 反转结果轮廓的方向。
            for (ClipperLib::Path &path : out_this)
                std::reverse(path.begin(), path.end());
        }
        append(out, std::move(out_this));
    }
    return out;
}

static ClipperLib::Paths wavefront_clip(const ClipperLib::Paths &wavefront, const Polygons &clipping)
{
    ClipperLib::Clipper clipper;
    clipper.AddPaths(wavefront, ClipperLib::ptSubject, true);
    clipper.AddPaths(ClipperUtils::PolygonsProvider(clipping),  ClipperLib::ptClip, true);
    ClipperLib::Paths out;
    clipper.Execute(ClipperLib::ctIntersection, out, ClipperLib::pftPositive, ClipperLib::pftPositive);
    return out;
}

static Polygons propagate_wave_from_boundary(
    ClipperLib::ClipperOffset   &co,
    // 波浪种子：非常接近边界的开放折线。
    const ClipperLib::Paths     &seed,
    // 波形将在此边界内传播。
    const ExPolygon             &boundary,
    // 种子线膨胀多少以生成第一个波区域。
    const float                  initial_step,
    // 每一步中膨胀第一个波区域及其后续波区域的量。
    const float                  other_step,
    // 初始步骤之后的膨胀步数。
    const size_t                 num_other_steps,
    // 种子轮廓在边界上的最大膨胀量。用于裁剪边界以加速波浪传播期间的裁剪。
    const float                  max_inflation)
{
    assert(! seed.empty() && seed.front().size() >= 2);
    Polygons clipping = ClipperUtils::clip_clipper_polygons_with_subject_bbox(boundary, get_extents<true>(seed).inflated(max_inflation));
    ClipperLib::Paths polygons = wavefront_clip(wavefront_initial(co, seed, initial_step), clipping);
    // 现在偏移剩余的
    for (size_t ioffset = 0; ioffset < num_other_steps; ++ ioffset)
        polygons = wavefront_clip(wavefront_step(co, polygons, other_step), clipping);
    return to_polygons(polygons);
}

// 结果区域按边界 ID 和源 ID 排序。
std::vector<RegionExpansion> propagate_waves(const WaveSeeds &seeds, const ExPolygons &boundary, const RegionExpansionParameters &params)
{
    std::vector<RegionExpansion> out;
    ClipperLib::Paths            paths;
    ClipperLib::ClipperOffset co;
    co.ArcTolerance       = params.arc_tolerance;
    co.ShortestEdgeLength = params.shortest_edge_length;
    for (auto it_seed = seeds.begin(); it_seed != seeds.end();) {
        auto it = it_seed;
        paths.clear();
        for (; it != seeds.end() && it->boundary == it_seed->boundary && it->src == it_seed->src; ++ it)
            paths.emplace_back(it->path);
        // 传播波前，同时用裁剪后的边界对其进行裁剪。
        // 收集扩展后的多边形，将其与源多边形合并。
        RegionExpansion re;
        for (Polygon &polygon : propagate_wave_from_boundary(co, paths, boundary[it_seed->boundary], params.initial_step, params.other_step, params.num_other_steps, params.max_inflation))
            out.push_back({ std::move(polygon), it_seed->src, it_seed->boundary });
        it_seed = it;
    }

    return out;
}

std::vector<RegionExpansion> propagate_waves(const ExPolygons &src, const ExPolygons &boundary, const RegionExpansionParameters &params)
{
    return propagate_waves(wave_seeds(src, boundary, params.tiny_expansion, true), boundary, params);
}

std::vector<RegionExpansion> propagate_waves(const ExPolygons &src, const ExPolygons &boundary,
    //// Scaled expansion 值
    float expansion, 
    //// Expand by waves of expansion_step 大小 (expansion_step 是 scaled).
    float expansion_step,
    //// 用于 small 的 Don't take more than max_nr_steps expansion_step.
    size_t max_nr_steps)
{
    return propagate_waves(src, boundary, RegionExpansionParameters::build(expansion, expansion_step, max_nr_steps));
}

// 返回每个源 ExPolygon 扩展到边界中的区域。
std::vector<RegionExpansionEx> propagate_waves_ex(const WaveSeeds &seeds, const ExPolygons &boundary, const RegionExpansionParameters &params)
{
    std::vector<RegionExpansion> expanded = propagate_waves(seeds, boundary, params);
    assert(std::is_sorted(seeds.begin(), seeds.end(), [](const auto &l, const auto &r){ return l.boundary < r.boundary || (l.boundary == r.boundary && l.src < r.src); }));
    Polygons acc;
    std::vector<RegionExpansionEx> out;
    for (auto it = expanded.begin(); it != expanded.end(); ) {
        auto it2 = it;
        acc.clear();
        for (; it2 != expanded.end() && it2->boundary_id == it->boundary_id && it2->src_id == it->src_id; ++ it2)
            acc.emplace_back(std::move(it2->polygon));
        size_t size = it2 - it;
        if (size == 1)
            out.push_back({ ExPolygon{std::move(acc.front())}, it->src_id, it->boundary_id });
        else {
            ExPolygons expolys = union_ex(acc);
            reserve_more_power_of_2(out, expolys.size());
            for (ExPolygon &ex : expolys)
                out.push_back({ std::move(ex), it->src_id, it->boundary_id });
        }
        it = it2;
    }
    return out;
}

// 返回每个源 ExPolygon 扩展到边界中的区域。
std::vector<RegionExpansionEx> propagate_waves_ex(
    // 应当接触边界的源区域。
    // 接触"边界"区域的源区域将被扩展到该"边界"区域中。
    const ExPolygons    &src,
    const ExPolygons    &boundary,
    // 缩放的扩展值
    float                full_expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float                expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t               max_nr_expansion_steps)
{
    auto params = RegionExpansionParameters::build(full_expansion, expansion_step, max_nr_expansion_steps);
    return propagate_waves_ex(wave_seeds(src, boundary, params.tiny_expansion, true), boundary, params);
}

std::vector<Polygons> expand_expolygons(const ExPolygons &src, const ExPolygons &boundary,
    // 缩放的扩展值
    float expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t max_nr_steps)
{
    std::vector<Polygons> out(src.size(), Polygons{});
    for (RegionExpansion &r : propagate_waves(src, boundary, expansion, expansion_step, max_nr_steps))
        out[r.src_id].emplace_back(std::move(r.polygon));
    return out;
}

std::vector<ExPolygon> merge_expansions_into_expolygons(ExPolygons &&src, std::vector<RegionExpansion> &&expanded)
{
    // 扩展区域将合并到源区域中，因此将按源 ID 重新排序。
    std::sort(expanded.begin(), expanded.end(), [](const auto &l, const auto &r) { return l.src_id < r.src_id; });
    uint32_t   last = 0;
    Polygons   acc;
    ExPolygons out;
    out.reserve(src.size());
    for (auto it = expanded.begin(); it != expanded.end();) {
        for (; last < it->src_id; ++ last)
            out.emplace_back(std::move(src[last]));
        acc.clear();
        assert(it->src_id == last);
        for (; it != expanded.end() && it->src_id == last; ++ it)
            acc.emplace_back(std::move(it->polygon));
        //FIXME 偏移和合并可以更高效，例如不需要复制源 expolygon
        ExPolygon &src_ex = src[last ++];
        assert(! src_ex.contour.empty());
#if 0
        {
            static int iRun = 0;
            BoundingBox bbox = get_extents(acc);
            bbox.merge(get_extents(src_ex));
            SVG svg(debug_out_path("expand_merge_expolygons-failed-union=%d.svg", iRun ++).c_str(), bbox);
            svg.draw(acc);
            svg.draw_outline(acc, "black", scale_(0.05));
            svg.draw(src_ex, "red");
            svg.Close();
        }
#endif
        Point sample = src_ex.contour.front();
        append(acc, to_polygons(std::move(src_ex)));
        ExPolygons merged = union_safety_offset_ex(acc);
        // 通过波浪扩展一个 expolygon 不应改变源 expolygon 的连通性：
        // 应生成单个 expolygon，可能带有更多孔洞。
        if (merged.size() > 1) {
            //// assert(合并.大小() == 1);
            // 初始波浪出现问题。很可能桥梁完全无效，
            // 或者边界区域非常接近某些桥梁边缘但实际上并未接触。
            // 只选择一个包含源 expolygon 的一个采样点的合并 expolygon。
            auto aabb_tree = build_aabb_tree_over_expolygons(merged);
            int id = sample_in_expolygons(aabb_tree, merged, sample);
            assert(id != -1);
            if (id != -1)
                out.emplace_back(std::move(merged[id]));
        } else if (merged.size() == 1)
            out.emplace_back(std::move(merged.front()));
    }
    for (; last < uint32_t(src.size()); ++ last)
        out.emplace_back(std::move(src[last]));
    return out;
}

std::vector<ExPolygon> expand_merge_expolygons(ExPolygons &&src, const ExPolygons &boundary, const RegionExpansionParameters &params)
{
    // 扩展区域按边界 ID 和源 ID 排序
    std::vector<RegionExpansion> expanded = propagate_waves(src, boundary, params);
    return merge_expansions_into_expolygons(std::move(src), std::move(expanded));
}

} // Algorithm
} // Slic3r
