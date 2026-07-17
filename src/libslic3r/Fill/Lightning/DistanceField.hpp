//Copyright (c) 2021 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef LIGHTNING_DISTANCE_FIELD_H
#define LIGHTNING_DISTANCE_FIELD_H

#include "../../BoundingBox.hpp"
#include "../../Point.hpp"
#include "../../Polygon.hpp"

//#define LIGHTNING_DISTANCE_FIELD_DEBUG_OUTPUT

namespace Slic3r::FillLightning
{

/*!
 * 2D 场，维护需要为闪电填充支撑的位置。
 *
 * 该场包含一组"单元"，以网格间隔分布。每个单元
 * 维护其距边缘的距离，用于确定它如何被闪电填充支撑。
 */
class DistanceField
{
public:
    /*!
     * 构造一个新的场，用于计算闪电填充。
     * \param radius 填充线预期在上层支撑的影响半径。
     * \param current_outline 此层上的总填充区域。
     * \param current_overhang 此层上需要支撑的悬垂部分。
     */
    DistanceField(const coord_t& radius, const Polygons& current_outline, const BoundingBox& current_outlines_bbox, const Polygons& current_overhang);
    
    /*!
     * 获取下一个需要由新分支支撑的未支撑位置。
     * \param p 下一个要支撑的点的输出变量。
     * \return 成功返回 ``true``，如果没有更多点需要考虑则返回 ``false``。
     */
    bool tryGetNextPoint(Point *out_unsupported_location, size_t *out_unsupported_cell_idx, const size_t start_idx = 0) const
    {
        for (size_t point_idx = start_idx; point_idx < m_unsupported_points.size(); ++point_idx) {
            if (!m_unsupported_points_erased[point_idx]) {
                *out_unsupported_cell_idx = point_idx;
                *out_unsupported_location = m_unsupported_points[point_idx].loc;
                return true;
            }
        }

        return false;
    }

    /*!
     * 使用新添加的分支更新距离场。
     *
     * 分支是从 \p to_node 延伸到 \p added_leaf 的线。此函数更新网格单元，
     * 以便距离场知道其距离当前图案支撑的程度。网格点使用沿线的支撑半径间隔的采样点进行更新。
     * \param to_node 新添加分支的节点端点。
     * \param added_leaf 新添加分支的叶节点位置，与节点形成直线。
     */
    void update(const Point& to_node, const Point& added_leaf);

protected:
    /*!
     * 网格点之间的间距，用于考虑支撑。
     */
    coord_t m_cell_size;

    /*!
     * 树分支上的点支撑的上层区域的半径。
     */
    coord_t m_supporting_radius;
    int64_t m_supporting_radius2;

    /*!
     * 表示需要被支撑的填充的微小离散区域。
     */
    struct UnsupportedCell
    {
        // 此单元中心的位置。
        Point loc;
        // 此单元与 ``current_outline`` 多边形（填充区域边缘）的距离。
        coord_t dist_to_boundary;
    };

    /*!
     * 在某个时刻仍然需要被支撑的单元。
     */
    std::vector<UnsupportedCell> m_unsupported_points;
    std::vector<bool>            m_unsupported_points_erased;

    /*!
     * m_unsupported_points 中所有点的边界框。用于将有符号整数映射到正整数。
     */
    const BoundingBox          m_unsupported_points_bbox;

    /*!
     * 将未支撑的点链接到网格点，以便我们可以快速查找网格中属于某个位置的单元。
     */

    class UnsupportedPointsGrid
    {
    public:
        UnsupportedPointsGrid() = default;
        void initialize(const std::vector<UnsupportedCell> &unsupported_points, const std::function<Point(const Point &)> &map_cell_to_grid)
        {
            if (unsupported_points.empty())
                return;

            BoundingBox unsupported_points_bbox;
            for (const UnsupportedCell &cell : unsupported_points)
                unsupported_points_bbox.merge(cell.loc);

            m_size        = unsupported_points.size();
            m_grid_range  = BoundingBox(map_cell_to_grid(unsupported_points_bbox.min), map_cell_to_grid(unsupported_points_bbox.max));
            m_grid_size   = m_grid_range.size() + Point::Ones();

            m_data.assign(m_grid_size.y() * m_grid_size.x(), std::numeric_limits<size_t>::max());
            m_data_erased.assign(m_grid_size.y() * m_grid_size.x(), true);

            for (size_t cell_idx = 0; cell_idx < unsupported_points.size(); ++cell_idx) {
                const size_t flat_idx   = map_to_flat_array(map_cell_to_grid(unsupported_points[cell_idx].loc));
                assert(m_data[flat_idx] == std::numeric_limits<size_t>::max());
                m_data[flat_idx]        = cell_idx;
                m_data_erased[flat_idx] = false;
            }
        }

        size_t size() const { return m_size; }

        size_t find_cell_idx(const Point &grid_addr)
        {
            if (!m_grid_range.contains(grid_addr))
                return std::numeric_limits<size_t>::max();

            if (const size_t flat_idx = map_to_flat_array(grid_addr); !m_data_erased[flat_idx]) {
                assert(m_data[flat_idx] != std::numeric_limits<size_t>::max());
                return m_data[flat_idx];
            }

            return std::numeric_limits<size_t>::max();
        }

        void mark_erased(const Point &grid_addr)
        {
            assert(m_grid_range.contains(grid_addr));
            if (!m_grid_range.contains(grid_addr))
                return;

            const size_t flat_idx = map_to_flat_array(grid_addr);
            assert(!m_data_erased[flat_idx] && m_data[flat_idx] != std::numeric_limits<size_t>::max());
            assert(m_size != 0);

            m_data_erased[flat_idx] = true;
            --m_size;
        }

    private:
        size_t m_size = 0;

        BoundingBox m_grid_range;
        Point       m_grid_size;

        std::vector<size_t> m_data;
        std::vector<bool>   m_data_erased;

        inline size_t map_to_flat_array(const Point &loc) const
        {
            const Point  offset_loc = loc - m_grid_range.min;
            const size_t flat_idx   = m_grid_size.x() * offset_loc.y() + offset_loc.x();
            assert(offset_loc.x() >= 0 && offset_loc.y() >= 0);
            assert(flat_idx < size_t(m_grid_size.y() * m_grid_size.x()));
            return flat_idx;
        }
    };

    UnsupportedPointsGrid m_unsupported_points_grid;

    /*!
     * 将点映射到网格坐标。
     */
    Point to_grid_point(const Point &point) const {
        return (point - m_unsupported_points_bbox.min) / m_cell_size;
    }

    /*!
     * 将点映射到网格坐标。
     */
    Point from_grid_point(const Point &point) const {
        return point * m_cell_size + m_unsupported_points_bbox.min;
    }

#ifdef LIGHTNING_DISTANCE_FIELD_DEBUG_OUTPUT
    friend void export_distance_field_to_svg(const std::string &path, const Polygons &outline, const Polygons &overhang, const std::list<DistanceField::UnsupportedCell> &unsupported_points, const Points &points);
#endif
};

} // namespace Slic3r::FillLightning

#endif //LIGHTNING_DISTANCE_FIELD_H
