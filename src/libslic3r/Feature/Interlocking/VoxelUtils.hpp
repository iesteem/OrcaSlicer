// Copyright (c) 2022 Ultimaker B.V.
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef UTILS_VOXEL_UTILS_H
#define UTILS_VOXEL_UTILS_H

#include <functional>

#include "libslic3r/Polygon.hpp"
#include "libslic3r/ExPolygon.hpp"

namespace Slic3r
{

using GridPoint3 = Vec3crd;

/*!
 * 用于存储相对于参考单元的膨胀（dilation）偏移位置的类。
 */
struct DilationKernel
{
    /*!
     * 立方体核检查参考体素周围一个立方体中的所有体素。
     *  _____
     * |\ ___\
     * | |    |
     *  \|____|
     *
     * 菱形核使用曼哈顿距离在参考体素周围创建菱形形状。
     *  /|\
     * /_|_\
     * \ | /
     *  \|/
     *
     * 棱柱核在 XY 方向为菱形，但在 Z 方向围绕参考体素垂直延伸。
     *   / \
     *  /   \
     * |\   /|
     * | \ / |
     * |  |  |
     *  \ | /
     *   \|/
     */
    enum class Type
    {
        CUBE,
        DIAMOND,
        PRISM
    };
    GridPoint3 kernel_size_; //!< 核的大小（以体素单元格数量计）
    Type type_;
    std::vector<GridPoint3> relative_cells_; //!< 相对于某个要膨胀的参考单元格的所有偏移位置

    DilationKernel(GridPoint3 kernel_size, Type type);
};

/*!
 * 用于遍历 3D 体素网格的实用类。
 *
 * 包含体素与直线、多边形、区域等的相交计算。
 */
class VoxelUtils
{
public:
    using grid_coord_t = coord_t;

    Vec3crd cell_size_;

    VoxelUtils(Vec3crd cell_size)
        : cell_size_(cell_size)
    {
    }

    /*!
     * 处理线段穿过的体素。
     *
     * \param start 线的起点
     * \param end 线的终点
     * \param process_cell_func 对每个线穿过的单元格执行的函数
     * \return 执行是否因 \p cell_processing_function 指示而提前停止
     */
    bool walkLine(Vec3crd start, Vec3crd end, const std::function<bool(GridPoint3)>& process_cell_func) const;

    /*!
     * 处理多边形线段穿过的体素。
     *
     * \warning 体素可能被多次处理！
     *
     * \param polys 要遍历的多边形
     * \param z 多边形所在的高度
     * \param process_cell_func 对每个体素单元格执行的函数
     * \return 执行是否因 \p cell_processing_function 指示而提前停止
     */
    bool walkPolygons(const ExPolygon& polys, coord_t z, const std::function<bool(GridPoint3)>& process_cell_func) const;

    /*!
     * 处理多边形线段附近的体素。
     * 对于多边形穿过的每个体素，我们根据核处理每个偏移体素。
     *
     * \warning 体素可能被多次处理！
     *
     * \param polys 要遍历的多边形
     * \param z 多边形所在的高度
     * \param process_cell_func 对每个体素单元格执行的函数
     * \return 执行是否因 \p cell_processing_function 指示而提前停止
     */
    bool walkDilatedPolygons(const ExPolygon& polys, coord_t z, const DilationKernel& kernel, const std::function<bool(GridPoint3)>& process_cell_func) const;
    bool walkDilatedPolygons(const ExPolygons& polys, coord_t z, const DilationKernel& kernel, const std::function<bool(GridPoint3)>& process_cell_func) const
    {
        for (const auto & poly : polys) {
            if (!walkDilatedPolygons(poly, z, kernel, process_cell_func)) {
                return false;
            }
        }

        return true;
    }

private:
    /*!
     * \warning 假定 \p polys 已在 xy 方向上平移 cell_size 的一半
     */
    bool _walkAreas(const ExPolygon& polys, coord_t z, const std::function<bool(GridPoint3)>& process_cell_func) const;

public:
    /*!
     * 处理多边形对象区域内的所有体素。
     *
     * \warning 区域边缘的体素不处理。薄区域可能不处理任何体素。
     *
     * \param polys 要填充的区域
     * \param z 多边形所在的高度
     * \param process_cell_func 对每个体素单元格执行的函数
     * \return 执行是否因 \p cell_processing_function 指示而提前停止
     */
    bool walkAreas(const ExPolygon& polys, coord_t z, const std::function<bool(GridPoint3)>& process_cell_func) const;

    /*!
     * 处理多边形对象区域内的所有体素。
     * 对于多边形内的每个体素，我们根据核处理每个偏移体素。
     *
     * \warning 区域边缘的体素不处理。薄区域可能不处理任何体素。
     *
     * \param polys 要填充的区域
     * \param z 多边形所在的高度
     * \param process_cell_func 对每个体素单元格执行的函数
     * \return 执行是否因 \p cell_processing_function 指示而提前停止
     */
    bool walkDilatedAreas(const ExPolygon& polys, coord_t z, const DilationKernel& kernel, const std::function<bool(GridPoint3)>& process_cell_func) const;
    bool walkDilatedAreas(const ExPolygons& polys, coord_t z, const DilationKernel& kernel, const std::function<bool(GridPoint3)>& process_cell_func) const
    {
        for (const auto & poly : polys) {
            if (!walkDilatedAreas(poly, z, kernel, process_cell_func)) {
                return false;
            }
        }

        return true;
    }

    /*!
     * 使用核进行膨胀。
     *
     * 扩展 \p process_cell_func，以便对于每个单元格也处理附近的单元格。
     *
     * 将此函数应用于 process_cell_func，以创建一个新的 process_cell_func，该函数也将效果应用到附近的体素。
     *
     * \param kernel 相对于 \p process_cell_func 输入的偏移位置
     * \param process_cell_func 对每个体素单元格执行的函数
     */
    std::function<bool(GridPoint3)> dilate(const DilationKernel& kernel, const std::function<bool(GridPoint3)>& process_cell_func) const;

    GridPoint3 toGridPoint(const Vec3crd& point) const
    {
        return GridPoint3(toGridCoord(point.x(), 0), toGridCoord(point.y(), 1), toGridCoord(point.z(), 2));
    }

    grid_coord_t toGridCoord(const coord_t& coord, const size_t dim) const
    {
        assert(dim < 3);
        return coord / cell_size_[dim] - (coord < 0);
    }

    Vec3crd toLowerCorner(const GridPoint3& location) const
    {
        return Vec3crd(toLowerCoord(location.x(), 0), toLowerCoord(location.y(), 1), toLowerCoord(location.z(), 2));
    }

    coord_t toLowerCoord(const grid_coord_t& grid_coord, const size_t dim) const
    {
        assert(dim < 3);
        return grid_coord * cell_size_[dim];
    }

    /*!
     * 返回等于坐标 \p p 处体素单元格横截面的矩形多边形
     */
    Polygon toPolygon(const GridPoint3 p) const
    {
        Polygon ret;
        Vec3crd c = toLowerCorner(p);
        ret.append({c.x(), c.y()});
        ret.append({c.x() + cell_size_.x(), c.y()});
        ret.append({c.x() + cell_size_.x(), c.y() + cell_size_.y()});
        ret.append({c.x(), c.y() + cell_size_.y()});
        return ret;
    }
};

} // namespace Slic3r

#endif // UTILS_VOXEL_UTILS_H
