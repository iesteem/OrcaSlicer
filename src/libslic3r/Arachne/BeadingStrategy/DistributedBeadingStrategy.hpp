// Copyright (c) 2022 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef DISTRIBUTED_BEADING_STRATEGY_H
#define DISTRIBUTED_BEADING_STRATEGY_H

#include "BeadingStrategy.hpp"
#include "libslic3r/libslic3r.h"

namespace Slic3r::Arachne
{

/*!
 * 此珠子策略选择使线宽偏离最佳线宽最小的壁数，然后在可用厚度中均匀分布线宽。
 */
class DistributedBeadingStrategy : public BeadingStrategy
{
protected:
    float one_over_distribution_radius_squared; // (1 / distribution_radius)^2

public:
    /*!
    * \param distribution_radius 分布特征尺寸与最佳厚度之间差异的半径（以珠子数量计）
    */
    DistributedBeadingStrategy(coord_t optimal_width,
                               coord_t default_transition_length,
                               double  transitioning_angle,
                               double  wall_split_middle_threshold,
                               double  wall_add_middle_threshold,
                               int     distribution_radius);

    ~DistributedBeadingStrategy() override = default;

    Beading compute(coord_t thickness, coord_t bead_count) const override;
    coord_t getOptimalBeadCount(coord_t thickness) const override;
};

} // namespace Slic3r::Arachne
#endif // DISTRIBUTED_BEADING_STRATEGY_H
