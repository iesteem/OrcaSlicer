﻿#ifndef SRC_LIBSLIC3R_ALGORITHM_REGION_EXPANSION_HPP_
#define SRC_LIBSLIC3R_ALGORITHM_REGION_EXPANSION_HPP_

#include <cstdint>
#include <libslic3r/Point.hpp>
#include <libslic3r/Polygon.hpp>
#include <libslic3r/ExPolygon.hpp>

namespace Slic3r {
namespace Algorithm {

struct RegionExpansionParameters
{
    // 初始扩展源区域，使其与边界区域有少量交集。
    float                  tiny_expansion;
    // 种子线膨胀多少以生成第一个波区域。
    float                  initial_step;
    // 每一步中膨胀第一个波区域及其后续波区域的量。
    float                  other_step;
    // 初始步骤之后的膨胀步数。
    size_t                 num_other_steps;
    // 种子轮廓在边界上的最大膨胀量。用于裁剪边界以加速波浪传播期间的裁剪。
    float                  max_inflation;

    // 波浪传播的偏移精度。
    double                 arc_tolerance;
    double                 shortest_edge_length;

    static RegionExpansionParameters build(
        // 缩放的扩展值
        float                full_expansion,
        // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
        float                expansion_step,
        // 对于小的 expansion_step，不要超过 max_nr_steps。
        size_t               max_nr_expansion_steps);
};

struct WaveSeed {
    uint32_t src;
    uint32_t boundary;
    Points   path;
};
using WaveSeeds = std::vector<WaveSeed>;

inline bool lower_by_boundary_and_src(const WaveSeed &l, const WaveSeed &r)
{
    return l.boundary < r.boundary || (l.boundary == r.boundary && l.src < r.src);
}

inline bool lower_by_src_and_boundary(const WaveSeed &l, const WaveSeed &r)
{
    return l.src < r.src || (l.src == r.src && l.boundary < r.boundary);
}

// 将源区域稍微向外扩展以与边界相交，用边界裁剪偏移后的源折线。
// 返回带有其来源注释的裁剪路径（路径的来源、边界区域的索引）。
WaveSeeds wave_seeds(
    // 应该接触边界的源区域。
    const ExPolygons      &src,
    // 接触"边界"区域的源区域将被扩展到该"边界"区域中。
    const ExPolygons      &boundary,
    // 初始扩展源区域，使其与边界区域有少量交集。
    float                  tiny_expansion,
    bool                   sorted);

struct RegionExpansion
{
    Polygon     polygon;
    uint32_t    src_id;
    uint32_t    boundary_id;
};

std::vector<RegionExpansion> propagate_waves(const WaveSeeds &seeds, const ExPolygons &boundary, const RegionExpansionParameters &params);
std::vector<RegionExpansion> propagate_waves(const ExPolygons &src, const ExPolygons &boundary, const RegionExpansionParameters &params);

std::vector<RegionExpansion> propagate_waves(const ExPolygons &src, const ExPolygons &boundary,
    // 缩放的扩展值
    float expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t max_nr_steps);

struct RegionExpansionEx
{
    ExPolygon   expolygon;
    uint32_t    src_id;
    uint32_t    boundary_id;
};

std::vector<RegionExpansionEx> propagate_waves_ex(const WaveSeeds &seeds, const ExPolygons &boundary, const RegionExpansionParameters &params);

std::vector<RegionExpansionEx> propagate_waves_ex(const ExPolygons &src, const ExPolygons &boundary,
    // 缩放的扩展值
    float expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t max_nr_steps);

std::vector<Polygons> expand_expolygons(const ExPolygons &src, const ExPolygons &boundary,
    // 缩放的扩展值
    float expansion,
    // 按 expansion_step 大小的波浪进行扩展（expansion_step 是缩放后的值）。
    float expansion_step,
    // 对于小的 expansion_step，不要超过 max_nr_steps。
    size_t max_nr_steps);

// 将源区域与扩展区域合并，返回合并后的扩展多边形。
std::vector<ExPolygon> merge_expansions_into_expolygons(ExPolygons &&src, std::vector<RegionExpansion> &&expanded);

std::vector<ExPolygon> expand_merge_expolygons(ExPolygons &&src, const ExPolygons &boundary, const RegionExpansionParameters &params);

} // Algorithm
} // Slic3r

#endif /* SRC_LIBSLIC3R_ALGORITHM_REGION_EXPANSION_HPP_ */
