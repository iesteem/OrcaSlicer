// Copyright (c) 2022 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef BEADING_STRATEGY_H
#define BEADING_STRATEGY_H

#include <math.h>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

#include "libslic3r/libslic3r.h"

namespace Slic3r::Arachne
{

template<typename T> constexpr T pi_div(const T div) { return static_cast<T>(M_PI) / div; }

/*!
 * 主要是虚基类模板。
 *
 * 用多个珠子（beads）覆盖给定（恒定）水平模型厚度的策略。
 *
 * 珠子的宽度可能不同。
 *
 * TODO: 扩展为包含打印顺序？
 */
class BeadingStrategy
{
public:
    /*!
     * 给定水平模型厚度的珠子排列。
     */
    struct Beading
    {
        coord_t total_thickness;
        std::vector<coord_t> bead_widths; //! 从外向内每个珠子的线宽
        std::vector<coord_t> toolpath_locations; //! 每个珠子的刀具路径位置到轮廓的距离
        coord_t left_over; //! 未被任何珠子覆盖的距离；间隙区域。
    };

    BeadingStrategy(coord_t optimal_width, double wall_split_middle_threshold, double wall_add_middle_threshold, coord_t default_transition_length, float transitioning_angle = pi_div(3));

    BeadingStrategy(const BeadingStrategy &other);

    virtual ~BeadingStrategy() = default;

    /*!
     * 获取用于覆盖给定厚度的珠子宽度。
     *
     * 要求：给定恒定的 \p bead_count，每个珠子宽度的输出必须随 \p thickness 逐渐变化。
     *
     * \note \p bead_count 可能与 \ref BeadingStrategy::optimal_bead_count 不同
     */
    virtual Beading compute(coord_t thickness, coord_t bead_count) const = 0;

    /*!
     * 给定 \param bead_count 的理想厚度
     */
    virtual coord_t getOptimalThickness(coord_t bead_count) const;

    /*!
     * 模型厚度，在此厚度下 \ref BeadingStrategy::optimal_bead_count 从 \p lower_bead_count 过渡到 \p lower_bead_count + 1
     */
    virtual coord_t getTransitionThickness(coord_t lower_bead_count) const;

    /*!
     * 对于给定的模型厚度，我们理想情况下应使用的珠子数量
     */
    virtual coord_t getOptimalBeadCount(coord_t thickness) const = 0;

    /*!
     * 沿着骨架标记/显著区域的过渡区域长度。
     *
     * 过渡用于平滑整数珠子数量中的跳变；跳变转变为由其长度定义斜度的斜坡。
     */
    virtual coord_t getTransitioningLength(coord_t lower_bead_count) const;

    /*!
     * 过渡长度中放置于过渡低端与未平滑珠子数量跳变点之间的比例。
     *
     * 过渡用于平滑整数珠子数量中的跳变；跳变转变为可相对于跳变位置定位的斜坡。
     */
    virtual float getTransitionAnchorPos(coord_t lower_bead_count) const;

    /*!
     * 获取珠子数量区域中 \ref BeadingStrategy::compute 在宽度上出现弯曲的位置。
     * 从较低厚度到较高厚度排序。
     *
     * 这用于在骨架中插入额外的支撑骨线，使得长梯形中产生的珠子不在两端之间线性变化。
     */
    virtual std::vector<coord_t> getNonlinearThicknesses(coord_t lower_bead_count) const;

    virtual std::string toString() const;

    double  getSplitMiddleThreshold() const;
    double  getTransitioningAngle() const;

protected:
    std::string name;

    coord_t optimal_width; //! 最佳珠子宽度，在"理想"情况下的标称壁宽。

    double  wall_split_middle_threshold; //! 中间壁应分裂为两个的阈值，以最佳壁宽的比率表示。

    double  wall_add_middle_threshold; //! 在偶数个壁之间应添加新中间壁的阈值，以最佳壁宽的比率表示。

    coord_t default_transition_length; //! 在珠子数量之间平滑过渡的区域长度

    /*!
     * 轮廓线段之间的最大角度，小于此角度时将添加过渡。
     * 等于 180 - 论文中的"限制角平分线角度"
     */
    double  transitioning_angle;
};

using BeadingStrategyPtr = std::unique_ptr<BeadingStrategy>;

} // namespace Slic3r::Arachne
#endif // BEADING_STRATEGY_H
