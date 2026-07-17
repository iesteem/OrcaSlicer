﻿//Copyright (c) 2022 Ultimaker B.V.
//CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#include "OuterWallInsetBeadingStrategy.hpp"

#include <algorithm>
#include <utility>

#include "libslic3r/Arachne/BeadingStrategy/BeadingStrategy.hpp"

namespace Slic3r::Arachne
{
OuterWallInsetBeadingStrategy::OuterWallInsetBeadingStrategy(coord_t outer_wall_offset, BeadingStrategyPtr parent)
    : BeadingStrategy(*parent), parent(std::move(parent)), outer_wall_offset(outer_wall_offset)
{
    name = "OuterWallOfsetBeadingStrategy";
}

coord_t OuterWallInsetBeadingStrategy::getOptimalThickness(coord_t bead_count) const
{
    return parent->getOptimalThickness(bead_count);
}

coord_t OuterWallInsetBeadingStrategy::getTransitionThickness(coord_t lower_bead_count) const
{
    return parent->getTransitionThickness(lower_bead_count);
}

coord_t OuterWallInsetBeadingStrategy::getOptimalBeadCount(coord_t thickness) const
{
    return parent->getOptimalBeadCount(thickness);
}

coord_t OuterWallInsetBeadingStrategy::getTransitioningLength(coord_t lower_bead_count) const
{
    return parent->getTransitioningLength(lower_bead_count);
}

std::string OuterWallInsetBeadingStrategy::toString() const
{
    return std::string("OuterWallOfsetBeadingStrategy+") + parent->toString();
}

BeadingStrategy::Beading OuterWallInsetBeadingStrategy::compute(coord_t thickness, coord_t bead_count) const
{
    Beading ret = parent->compute(thickness, bead_count);

    // 现存壁表示的实际数量和厚度。不计算任何潜在的零宽度"信号"壁。
    bead_count = std::count_if(ret.bead_widths.begin(), ret.bead_widths.end(), [](const coord_t width) { return width > 0; });

    // 如果只有单个壁，则无需应用任何内缩。
    if (bead_count < 2)
    {
        return ret;
    }

    // 实际将外壁向内移动。确保外壁不超过中间线。
    ret.toolpath_locations[0] = std::min(ret.toolpath_locations[0] + outer_wall_offset, thickness / 2);
    return ret;
}

} // namespace Slic3r::Arachne
