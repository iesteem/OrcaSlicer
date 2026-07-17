// Copyright (c) 2020 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef CURAENGINE_WALLTOOLPATHS_H
#define CURAENGINE_WALLTOOLPATHS_H

#include <memory>
#include <ankerl/unordered_dense.h>

#include "BeadingStrategy/BeadingStrategyFactory.hpp"
#include "utils/ExtrusionLine.hpp"
#include "../Polygon.hpp"
#include "../PrintConfig.hpp"

namespace Slic3r::Arachne
{

constexpr bool    fill_outline_gaps                        = true;
inline coord_t    meshfix_maximum_resolution() { return scaled<coord_t>(0.5); }
inline coord_t    meshfix_maximum_deviation() { return scaled<coord_t>(0.025); }
inline coord_t    meshfix_maximum_extrusion_area_deviation() { return scaled<coord_t>(2.); }

class WallToolPathsParams
{
public:
    float   min_bead_width;
    float   min_feature_size;
    float   min_length_factor;
    float   wall_transition_length;
    float   wall_transition_angle;
    float   wall_transition_filter_deviation;
    int     wall_distribution_count;
    bool    is_top_or_bottom_layer;
};

WallToolPathsParams make_paths_params(const int layer_id, const PrintObjectConfig &print_object_config, const PrintConfig &print_config);

class WallToolPaths
{
public:
    /*!
     * 一个类，给定轮廓、标称珠子宽度和最大壁数，创建刀具路径。
     * \param outline 将要生成刀具路径的区域的轮廓
     * \param bead_width_0 生成刀具路径时使用的外壁宽度
     * \param bead_width_x 生成刀具路径时使用的内壁宽度；如果使用 nominal_bead_width 构造函数调用 WallToolPaths，则此值与 bead_width_0 相同
     * \param inset_count 构成壁的最大平行挤出线数量
     * \param wall_0_inset 外壁的缩进量，使其更好地粘附其他壁。
     */
    WallToolPaths(const Polygons& outline, coord_t bead_width_0, coord_t bead_width_x, size_t inset_count, coord_t wall_0_inset, coordf_t layer_height, const WallToolPathsParams &params);

    /*!
     * 生成刀具路径
     * \return 对新创建的刀具路径的引用
     */
    const std::vector<VariableWidthLines> &generate();

    /*!
     * 获取刀具路径，如果在 \p generate() 之前调用此函数，将首先生成刀具路径
     * \return 对刀具路径的引用
     */
    const std::vector<VariableWidthLines> &getToolPaths();

    /*!
     * 计算壁的内轮廓。此轮廓指示壁区域结束和其填充开始的位置。
     * 内部随后可以被填充，例如使用零件的蒙皮/填充，或者在使用额外填充壁的情况下使用图案填充。
     */
    void separateOutInnerContour();

    /*!
     * 获取生成的刀具路径内部区域的内轮廓。
     *
     * 如果壁尚未生成，这将惰性地调用
     * \p generate() 函数以生成可变宽度的壁。
     * 生成的多边形将精确匹配可变宽度壁的内部，
     * 其中壁受 LimitedBeadingStrategy 限制为最大壁数。
     * 如果没有壁，将返回轮廓。
     * \return 生成壁的内轮廓。
     */
    const Polygons& getInnerContour();

    /*!
     * 从刀具路径中移除空路径。
     * \param toolpaths 使用 \p generate() 生成的 VariableWidthPaths
     * \return 如果仍有路径剩余则返回 true。如果所有刀具路径都被移除则返回 false
     */
    static bool removeEmptyToolPaths(std::vector<VariableWidthLines> &toolpaths);

    using ExtrusionLineSet = ankerl::unordered_dense::set<std::pair<const ExtrusionLine *, const ExtrusionLine *>, boost::hash<std::pair<const ExtrusionLine *, const ExtrusionLine *>>>;

    /*!
     * 获取按区域/孔打印壁时的缩进排序约束。
     * 返回的每对由相邻的壁线组成，其中左侧的 inset_idx 比右侧低一个。
     *
     * 奇数壁应始终在其包围的壁多边形之后。
     *
     * \param outer_to_inner 具有较低 inset_idx 的壁多边形是否应优先于具有较高 inset_idx 的。
     */
    static ExtrusionLineSet getRegionOrder(const std::vector<ExtrusionLine *> &input, bool outer_to_inner);

protected:
    /*!
     * 将折线拼接在一起并形成闭合多边形。
     *
     * 同时处理刀具路径和内轮廓。
     */
    static void stitchToolPaths(std::vector<VariableWidthLines> &toolpaths, coord_t bead_width_x);

    /*!
     * 移除长度小于该折线最小线宽一半的折线。
     */
    void removeSmallLines(std::vector<VariableWidthLines> &toolpaths);

    /*!
     * 通过对路径中的每条线使用提供的设置调用简化函数，来简化可变宽度刀具路径。
     * \param settings 用户提供的设置
     * \return
     */
    static void simplifyToolPaths(std::vector<VariableWidthLines>  &toolpaths);

private:
    const Polygons& outline; //<! 作为指定区域的轮廓多边形的引用
    coord_t bead_width_0; //<! 用于 libArachne 生成壁的标称或第一挤出线宽度
    coord_t bead_width_x; //<! 用于 libArachne 生成后续壁的挤出线宽度；如果使用标称宽度构造函数调用 WallToolPaths，则与 bead_width_0 相同
    size_t inset_count; //<! 生成的最大壁数
    coord_t wall_0_inset; //<! 外壁缩进量。仅应在打印实际壁时应用，而不是额外的填充/蒙皮/支撑壁。
    coordf_t layer_height;
    bool print_thin_walls; //<! 是否启用薄特征的加宽珠子元策略
    coord_t min_feature_size; //<! 可通过加宽珠子元策略加宽的最小特征尺寸。比此更薄的特征将不会被打印
    coord_t min_bead_width;  //<! 使用加宽珠子元策略加宽薄模型特征时要使用的最小珠子尺寸
    double small_area_length; //<! 将被过滤掉的小特征的长度，平方为面积
    coord_t wall_transition_filter_deviation; //!< 过滤引起的允许线宽偏差
    bool toolpaths_generated; //<! 刀具路径是否已生成
    std::vector<VariableWidthLines> toolpaths; //<! 生成的刀具路径
    Polygons inner_contour;  //<! 生成刀具路径的内轮廓
    const WallToolPathsParams m_params;
};

} // namespace Slic3r::Arachne

#endif // CURAENGINE_WALLTOOLPATHS_H
