// Copyright (c) 2022 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef INTERLOCKING_GENERATOR_HPP
#define INTERLOCKING_GENERATOR_HPP

#include "libslic3r/Print.hpp"
#include "VoxelUtils.hpp"

namespace Slic3r {

/*!
 * 用于在两个不同挤出机的相邻模型之间生成互锁结构的类。
 *
 * 该结构由两种材料交替的水平梁组成。
 * 在 z 方向上，这些梁的方向每 90* 交替一次。
 *
 * 两种材料 # 和 O 的示例：
 * 偶数梁：     奇数梁：
 * ######           ##OO##OO
 * OOOOOO           ##OO##OO
 * ######           ##OO##OO
 * OOOOOO           ##OO##OO
 *
 * 结构单个单元的一种材料如下所示：
 *                    .-*-.
 *                .-*       *-.
 *               |*-.           *-.
 *               |    *-.           *-.
 *            .-* *-.     *-.           *-.
 *        .-*         *-.     *-.       .-*|
 *    .-*           .-*   *-.     *-.-*    |
 *   |*-.       .-*     .-*   *-.   |   .-*
 *   |    *-.-*     .-*           *-|-*
 *    *-.   |   .-*
 *        *-|-*
 *
 * 我们设置一个体素网格 (2*beam_w, 2*beam_w, 2*beam_h)，标记所有包含两个网格的体素。
 * 然后我们移除所有也包含空气的体素，以便互锁图案从外部不可见。
 * 然后我们为每个体素生成并合并多边形，并将这些区域应用于网格的轮廓。
 */
class InterlockingGenerator
{
public:
    /*!
     * 在每两个相邻网格之间生成互锁结构。
     */
    static void generate_interlocking_structure(PrintObject* print_object);

private:
    /*!
     * 在两个网格之间生成互锁结构。
     */
    void generateInterlockingStructure() const;

    /*!
     * 私有类，用于存储计算两个网格之间互锁结构时使用的一些变量。
     * \param region_a_index 第一个区域
     * \param region_b_index 第二个区域
     * \param rotation 旋转互锁图案的角度
     * \param cell_size 体素单元格的大小 (coord_t, coord_t, layer_count)
     * \param beam_layer_count 梁高度的层数
     * \param interface_dilation 界面的加厚核
     * \param air_dilation 应用于空气的膨胀核，使得靠近模型外部的单元格不会被生成
     * \param air_filtering 是否完全移除所有在外部可见（即接触空气）的互锁单元格。
     * 如果无空气过滤，则那些单元格将在梁中间被切断。
     */
    InterlockingGenerator(PrintObject&          print_object,
                          const size_t          region_a_index,
                          const size_t          region_b_index,
                          const coord_t         beam_width,
                          const coord_t         boundary_avoidance,
                          const float           rotation,
                          const Vec3crd&        cell_size,
                          const coord_t         beam_layer_count,
                          const DilationKernel& interface_dilation,
                          const DilationKernel& air_dilation,
                          const bool            air_filtering)
        : print_object(print_object)
        , region_a_index(region_a_index)
        , region_b_index(region_b_index)
        , beam_width(beam_width)
        , boundary_avoidance(boundary_avoidance)
        , vu(cell_size)
        , rotation(rotation)
        , cell_size(cell_size)
        , beam_layer_count(beam_layer_count)
        , interface_dilation(interface_dilation)
        , air_dilation(air_dilation)
        , air_filtering(air_filtering)
    {}

    /*! 给定两个多边形，返回与空气接壤的部分，并向上增长 'perpendicular' 到 'detect' 距离。
     *
     * \param a 第一个多边形。
     * \param b 第二个多边形。
     * \param detect 扩展距离。（不等同于偏移，而是一系列小偏移和差异）。
     * \return 表示 a 和 b 的"边界"但已"垂直"扩展的多边形对。
     */
    std::pair<ExPolygons, ExPolygons> growBorderAreasPerpendicular(const ExPolygons& a, const ExPolygons& b, const coord_t& detect) const;

    /*! 对薄材料条带的特殊处理。
     *
     * 在需要时将网格相互扩展，即当薄材料条带需要附着时。
     * \param has_all_meshes 仅当附近存在需要粘附的微结构时才进行此特殊处理。
     */
    void handleThinAreas(const std::unordered_set<GridPoint3>& has_all_meshes) const;

    /*!
     * 计算与两个模型外壳重叠的体素。
     * 这包括壁，也包括顶部/底部蒙皮。
     *
     * \param kernel 用于给返回的体素外壳更多厚度的膨胀核
     * \return 网格 a 的外壳体素和网格 b 的外壳体素
     */
    std::vector<std::unordered_set<GridPoint3>> getShellVoxels(const DilationKernel& kernel) const;

    /*!
     * 计算某些层的外壳重叠的体素。
     * 这包括壁，也包括顶部/底部蒙皮。
     *
     * \param layers 计算外壳体素的层轮廓
     * \param kernel 用于给返回的体素外壳更多厚度的膨胀核
     * \param[out] cells 属于外壳的输出单元格
     */
    void addBoundaryCells(const std::vector<ExPolygons>& layers, const DilationKernel& kernel, std::unordered_set<GridPoint3>& cells) const;

    /*!
     * 计算两个模型占据的区域。
     *
     * 执行形态学闭合，以便我们不会将两个模型之间的小间隙注册为分离。
     * \return layer_regions 计算出的层区域
     */
    std::vector<ExPolygons> computeUnionedVolumeRegions() const;

    /*!
     * 生成单个单元格的梁的多边形。
     * \return cell_area_per_mesh_per_layer 每个梁的输出多边形
     */
    std::vector<std::vector<ExPolygons>> generateMicrostructure() const;

    /*!
     * 使用计算出的互锁结构更改网格的轮廓。
     *
     * \param cells 要应用互锁结构的单元格
     * \param layer_regions 两个网格的总体积（小间隙已闭合）
     */
    void applyMicrostructureToOutlines(const std::unordered_set<GridPoint3>& cells, const std::vector<ExPolygons>& layer_regions) const;

    static const coord_t ignored_gap_ = 100u; //!< 模型之间的间距，被视为相邻以便在其间生成互锁结构

    PrintObject&  print_object;
    const size_t  region_a_index;
    const size_t  region_b_index;
    const coord_t beam_width;
    const coord_t boundary_avoidance;

    const VoxelUtils vu;

    const float          rotation;
    const Vec3crd        cell_size;
    const coord_t        beam_layer_count;
    const DilationKernel interface_dilation;
    const DilationKernel air_dilation;
    //// 是否完全移除所有在外部可见的互锁单元格。如果无空气过滤，
    // 则那些单元格将在梁中间被切断。
    const bool air_filtering;
};

} // namespace Slic3r

#endif // INTERLOCKING_GENERATOR_HPP
