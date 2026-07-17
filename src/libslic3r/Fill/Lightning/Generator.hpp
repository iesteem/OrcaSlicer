//Copyright (c) 2021 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef LIGHTNING_GENERATOR_H
#define LIGHTNING_GENERATOR_H

#include "Layer.hpp"

#include <functional>
#include <memory>
#include <vector>

namespace Slic3r 
{
class PrintObject;

namespace FillLightning
{

/*!
 * 生成闪电填充图案。
 *
 * 闪电填充图案旨在使用最少的材料
 * 支撑打印件的顶部表皮，同时以合理一致的流动线进行打印。
 * 它完全牺牲强度以换取
 * 顶部表面质量并减少打印时间/材料用量。
 *
 * 闪电填充因其创建的图案类似于一条
 * 分叉路径而得名，该路径有一条主路径和许多侧边小线。这些路径
 * 从模型侧面刚好在需要从内部支撑顶部表面的下方生长出来，
 * 从而需要最少的材料。
 *
 * 此图案基于 Tricard、Claux 和 Lefebvre 的论文
 * "Ribbed Support Vaults for 3D Printing of Hollowed Objects"：
 * https://www.researchgate.net/publication/333808588_Ribbed_Support_Vaults_for_3D_Printing_of_Hollowed_Objects
 */
class Generator  // "Just like Nicola used to make!"
{
public:
    /*!
     * 创建一个生成器，用填充填充特定网格。
     *
     * 此生成器将预计算内容，以准备为该网格中的填充区域生成
     * 闪电填充。此时填充区域必须
     * 已经计算好。
     */
    explicit Generator(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

    /*!
     * 获取为网格特定层生成的路径树。
     *
     * 该树表示打印填充必须追踪的路径。
     * \param layer_id 要获取路径树的层号。在网格的层范围内（不是全局层号）。
     * \return 表示打印路径的树结构，用于创建闪电填充图案。
     */
    const Layer& getTreesForLayer(const size_t& layer_id) const;

    std::vector<Polygons>& Overhangs() { return m_overhang_per_layer; }

    float infilll_extrusion_width() const { return m_infill_extrusion_width; }

    Generator(PrintObject* m_object, std::vector<Polygons>& contours, std::vector<Polygons>& overhangs, const std::function<void()> &throw_on_cancel_callback, float density = 0.15);

protected:
    /*!
     * 计算填充区域上方需要由填充支撑的悬垂部分。
     *
     * 通常，悬垂仅对模型外部生成，并且仅当生成支撑时。
     * 对于此图案，我们还需要为模型内部生成悬垂区域。
     */
    void generateInitialInternalOverhangs(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

    /*!
     * 计算所有层的树结构。
     */
    void generateTrees(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);
    void generateTreesforSupport(std::vector<Polygons>& contours, const std::function<void()> &throw_on_cancel_callback);

    float m_infill_extrusion_width;

    /*!
     * 每个填充片段可以支撑上层表皮的距离。
     */
    coord_t m_supporting_radius;

    /*!
     * 壁可以支撑其上方壁的距离。如果壁完全支撑其上方壁，
     * 则不需要填充来支撑。
     *
     * 这类似于为支撑计算的悬垂距离。它由 lightning_infill_overhang_angle 设置决定。
     */
    coord_t m_wall_supporting_radius;

    /*!
     * 每个填充片段可以支撑上层其他填充的距离。
     *
     * 这可能与 \ref supporting_radius 不同，因为填充是
     * 一端悬空打印的。该端点会下垂更多，因此
     * 填充线可能比表皮线需要更多支撑。
     */
    coord_t m_prune_length;

    /*!
     * 为拉直线条，线条可以移动的距离。
     *
     * 拉直线条可减少材料和时间用量，并减少打印图案所需的加速度。
     * 但是，如果线条部分悬挂在前一层线条旁边，则会使填充变弱。
     */
    coord_t m_straightening_max_distance;

    /*!
     * 每一层需要由图案支撑的悬垂部分。
     *
     * 由 \ref generateInitialInternalOverhangs 生成。
     */
    std::vector<Polygons> m_overhang_per_layer;

    /*!
     * 每一层生成的闪电路径。
     *
     * 由 \ref generateTrees 生成。
     */
    std::vector<Layer> m_lightning_layers;

    std::vector<BoundingBox> bboxs;
};

} // namespace FillLightning
} // namespace Slic3r

#endif // LIGHTNING_GENERATOR_H
