// Copyright (c) 2016 Scott Lenser
//// Copyright (c) 2020 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef UTILS_SPARSE_POINT_GRID_H
#define UTILS_SPARSE_POINT_GRID_H

#include <cassert>
#include <vector>

#include "SparseGrid.hpp"

namespace Slic3r::Arachne {

/*! \brief 可以高效定位空间附近元素的稀疏网格。
 *
 * \tparam ElemT 要存储的元素类型。
 * \tparam Locator 从 ElemT 获取位置的仿函数。Locator
 *    必须具有：Point operator()(const ElemT &elem) const
 *    返回与 val 关联的位置。
 */
template<class ElemT, class Locator> class SparsePointGrid : public SparseGrid<ElemT>
{
public:
    using Elem = ElemT;

    /*! \brief 使用指定的单元格大小构造稀疏网格。
     *
     * \param[in] cell_size 网格中一个单元格（正方形）的大小。
     *    典型值约为预期查询半径的 0.5-2 倍。
     * \param[in] elem_reserve 预预留空间的元素数量。
     * \param[in] max_load_factor 重新哈希前的最大平均负载因子。
     */
    SparsePointGrid(coord_t cell_size, size_t elem_reserve = 0U, float max_load_factor = 1.0f);

    /*! \brief 将 elem 插入稀疏网格。
     *
     * \param[in] elem 要插入的元素。
     */
    void insert(const Elem &elem);

protected:
    using GridPoint = typename SparseGrid<ElemT>::GridPoint;

    /*! \brief 用于从元素获取位置的访问器。 */
    Locator m_locator;
};

template<class ElemT, class Locator>
SparsePointGrid<ElemT, Locator>::SparsePointGrid(coord_t cell_size, size_t elem_reserve, float max_load_factor) : SparseGrid<ElemT>(cell_size, elem_reserve, max_load_factor) {}

template<class ElemT, class Locator>
void SparsePointGrid<ElemT, Locator>::insert(const Elem &elem)
{
    Point     loc      = m_locator(elem);
    GridPoint grid_loc = SparseGrid<ElemT>::toGridPoint(loc.template cast<int64_t>());

    SparseGrid<ElemT>::m_grid.emplace(grid_loc, elem);
}

} // namespace Slic3r::Arachne

#endif // UTILS_SPARSE_POINT_GRID_H
