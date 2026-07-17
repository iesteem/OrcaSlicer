//Copyright (c) 2016 Scott Lenser
//// Copyright (c) 2018 Ultimaker B.V.
//CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef UTILS_SPARSE_GRID_H
#define UTILS_SPARSE_GRID_H

#include <cassert>
#include <vector>
#include <functional>

#include "../../Point.hpp"
#include "SquareGrid.hpp"

namespace Slic3r::Arachne {

/*! \brief 可以高效定位空间附近元素的稀疏网格。
 *
 * \note 这是一个抽象模板类，没有任何插入元素的函数。
 * \see SparsePointGrid
 *
 * \tparam ElemT 要存储的元素类型。
 */
template<class ElemT> class SparseGrid : public SquareGrid
{
public:
    using Elem = ElemT;

    using GridPoint    = SquareGrid::GridPoint;
    using grid_coord_t = SquareGrid::grid_coord_t;
    using GridMap       = std::unordered_multimap<GridPoint, Elem, PointHash>;

    using iterator       = typename GridMap::iterator;
    using const_iterator = typename GridMap::const_iterator;

    /*! \brief 使用指定的单元格大小构造稀疏网格。
     *
     * \param[in] cell_size 网格中一个单元格（正方形）的大小。
     *    典型值约为预期查询半径的 0.5-2 倍。
     * \param[in] elem_reserve 预预留空间的元素数量。
     * \param[in] max_load_factor 重新哈希前的最大平均负载因子。
     */
    SparseGrid(coord_t cell_size, size_t elem_reserve=0U, float max_load_factor=1.0f);

    iterator begin() { return m_grid.begin(); }
    iterator end() { return m_grid.end(); }
    const_iterator begin() const { return m_grid.begin(); }
    const_iterator end() const { return m_grid.end(); }

    /*! \brief 返回 query_pt 半径内的所有数据。
     *
     * 查找所有位置在 \p query_pt 的 \p radius 范围内的元素。可能
     * 返回超出 radius 的额外元素。
     *
     * 平均运行时间为 a*(1 + 2 * radius / cell_size)**2 +
     * b*cnt，其中 a 和 b 是比例常数，cnt 是返回项的数量。
     * 搜索平均返回面积为 (2*radius + cell_size)**2 的区域内的项。
     * 项距 query_point 的最大距离为 radius + cell_size。
     *
     * \param[in] query_pt 搜索中心点。
     * \param[in] radius 搜索半径。
     * \return 找到的元素向量
     */
    std::vector<Elem> getNearby(const Point &query_pt, coord_t radius) const;

    /*! \brief 处理可能包含目标点的单元格中的元素。
     *
     * 处理可能包含在 \p radius 范围内、距 \p query_pt 有元素的单元格中的元素。
     * 处理所有在 query_pt 的 radius 范围内的元素。可能
     * 处理距 query_pt 最远为 radius + cell_size 的元素。
     *
     * \param[in] query_pt 搜索中心点。
     * \param[in] radius 搜索半径。
     * \param[in] process_func 处理每个元素。对每个单元格中的元素
     *    调用 process_func(elem)。如果函数返回 false，则停止处理。
     * \return 是否需要在处理此函数后继续处理
     */
    bool processNearby(const Point &query_pt, coord_t radius, const std::function<bool(const ElemT &)> &process_func) const;

protected:
    /*! \brief 处理 \p grid_pt 指示的单元格中的元素。
     *
     * \param[in] grid_pt 单元格的网格坐标。
     * \param[in] process_func 处理每个元素。对每个单元格中的元素
     *    调用 process_func(elem)。如果函数返回 false，则停止处理。
     * \return 是否需要继续处理下一个单元格。
     */
    bool processFromCell(const GridPoint &grid_pt, const std::function<bool(const Elem &)> &process_func) const;

    /*! \brief 从网格位置（GridPoint）到元素（Elem）的映射。 */
    GridMap m_grid;
};

template<class ElemT> SparseGrid<ElemT>::SparseGrid(coord_t cell_size, size_t elem_reserve, float max_load_factor) : SquareGrid(cell_size)
{
    //// 必须在 reserve 调用之前设置。
    m_grid.max_load_factor(max_load_factor);
    if (elem_reserve != 0U)
        m_grid.reserve(elem_reserve);
}

template<class ElemT> bool SparseGrid<ElemT>::processFromCell(const GridPoint &grid_pt, const std::function<bool(const Elem &)> &process_func) const
{
    auto grid_range = m_grid.equal_range(grid_pt);
    for (auto iter = grid_range.first; iter != grid_range.second; ++iter)
        if (!process_func(iter->second))
            return false;
    return true;
}

template<class ElemT>
bool SparseGrid<ElemT>::processNearby(const Point &query_pt, coord_t radius, const std::function<bool(const Elem &)> &process_func) const
{
    return SquareGrid::processNearby(query_pt, radius, [&process_func, this](const GridPoint &grid_pt) { return processFromCell(grid_pt, process_func); });
}

template<class ElemT> std::vector<typename SparseGrid<ElemT>::Elem> SparseGrid<ElemT>::getNearby(const Point &query_pt, coord_t radius) const
{
    std::vector<Elem>                       ret;
    const std::function<bool(const Elem &)> process_func = [&ret](const Elem &elem) {
        ret.push_back(elem);
        return true;
    };
    processNearby(query_pt, radius, process_func);
    return ret;
}

} // namespace Slic3r::Arachne

#endif // UTILS_SPARSE_GRID_H
