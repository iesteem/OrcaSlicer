#ifndef slic3r_EdgeGrid_hpp_
#define slic3r_EdgeGrid_hpp_

#include <stdint.h>
#include <math.h>

#include "Point.hpp"
#include "BoundingBox.hpp"
#include "ExPolygon.hpp"

namespace Slic3r {
namespace EdgeGrid {


class Contour {
public:
	Contour() = default;
	Contour(const Slic3r::Point *begin, const Slic3r::Point *end, bool open) : m_begin(begin), m_end(end), m_open(open) {}
	Contour(const Slic3r::Point *data, size_t size, bool open) : Contour(data, data + size, open) {}
	Contour(const Points &pts, bool open) : Contour(pts.data(), pts.size(), open) {}

    const Slic3r::Point *begin()  const { return m_begin; }
    const Slic3r::Point *end()    const { return m_end; }
    bool                 open()   const { return m_open; }
    bool                 closed() const { return !m_open; }

    const Slic3r::Point &front()  const { return *m_begin; }
    const Slic3r::Point &back()   const { return *(m_end - 1); }

	// 线段 idx 的起点。
	const Slic3r::Point& segment_start(size_t idx) const {
		assert(idx < this->num_segments());
		return m_begin[idx];
	}

	// 线段 idx 的终点。
	const Slic3r::Point& segment_end(size_t idx) const {
		assert(idx < this->num_segments());
		const Slic3r::Point *ptr = m_begin + idx + 1;
		return ptr == m_end ? *m_begin : *ptr;
	}

	// 线段 idx 的前一段的起点。
	const Slic3r::Point& segment_prev(size_t idx) const {
		assert(idx < this->num_segments());
		assert(idx > 0 || ! m_open);
		return idx == 0 ? m_end[-1] : m_begin[idx - 1];
	}

	// 线段 idx 的前一段的索引。
	const size_t 		 segment_idx_prev(size_t idx) const {
		assert(idx < this->num_segments());
		assert(idx > 0 || ! m_open);
		return (idx == 0 ? this->size() : idx) - 1;
	}

	// 线段 idx 的下一段的索引。
	const size_t 		 segment_idx_next(size_t idx) const {
		assert(idx < this->num_segments());
		++ idx;
		return m_begin + idx == m_end ? 0 : idx;
	}

	size_t               num_segments() const { return this->size() - (m_open ? 1 : 0); }

    Line                 get_segment(size_t idx) const
    {
        assert(idx < this->num_segments());
        return Line(this->segment_start(idx), this->segment_end(idx));
    }

    Lines                get_segments() const
    {
        Lines lines;
        lines.reserve(this->num_segments());
        if (this->num_segments() > 2) {
            for (auto it = this->begin(); it != this->end() - 1; ++it) lines.push_back(Line(*it, *(it + 1)));
            if (!m_open) lines.push_back(Line(this->back(), this->front()));
        }
        return lines;
    }

private:
	size_t  			 size() const { return m_end - m_begin; }

	const Slic3r::Point *m_begin { nullptr };
	const Slic3r::Point *m_end   { nullptr };
	bool                 m_open  { false };
};

class Grid
{
public:
	Grid() = default;
	Grid(const BoundingBox &bbox) : m_bbox(bbox) {}

	void set_bbox(const BoundingBox &bbox) { m_bbox = bbox; }

	// 使用开放折线或闭合轮廓填充网格。
	// 如果设置了开放标志，则 polylines_or_polygons 默认被视为开放。
	// 仅当折线的第一个点等于最后一个点时，
	// 该折线被视为闭合，并且在插入到 EdgeGrid 时移除最后一个重复点。
	// 大多数 Grid 函数期望所有轮廓都是闭合的，请注意！
	void create(const std::vector<Points> &polylines_or_polygons, coord_t resolution, bool open);
	void create(const Polygons &polygons, const Polylines &polylines, coord_t resolution);

	// 使用闭合轮廓填充网格。
	void create(const Polygons &polygons, coord_t resolution);
	void create(const std::vector<const Polygon*> &polygons, coord_t resolution);
	void create(const std::vector<Points> &polygons, coord_t resolution) { this->create(polygons, resolution, false); }
	void create(const ExPolygon &expoly, coord_t resolution);
	void create(const ExPolygons &expolygons, coord_t resolution);

	const std::vector<Contour>& contours() const { return m_contours; }

#if 0
	// Test, whether the edges inside the grid intersect with the polygons provided.
	bool intersect(const MultiPoint &polyline, bool closed);
	bool intersect(const Polygon &polygon) { return intersect(static_cast<const MultiPoint&>(polygon), true); }
	bool intersect(const Polygons &polygons) { for (size_t i = 0; i < polygons.size(); ++ i) if (intersect(polygons[i])) return true; return false; }
	bool intersect(const ExPolygon &expoly) { if (intersect(expoly.contour)) return true; for (size_t i = 0; i < expoly.holes.size(); ++ i) if (intersect(expoly.holes[i])) return true; return false; }
	bool intersect(const ExPolygons &expolygons) { for (size_t i = 0; i < expolygons.size(); ++ i) if (intersect(expolygons[i])) return true; return false; }

	// Test, whether a point is inside a contour.
	bool inside(const Point &pt);
#endif

	// 从边缘网格填充粗略的 m_signed_distance_field。
	// 粗略的 SDF 由 signed_distance() 用于搜索半径之外的距离。
	// 仅对闭合轮廓调用此函数！
	void calculate_sdf();

	// 返回基于 m_signed_distance_field 网格的有符号距离估计值。
	float signed_distance_bilinear(const Point &pt) const;

	// 计算从点到搜索半径内轮廓的有符号距离。
	// 仅对闭合轮廓调用此函数！
	struct ClosestPointResult {
		size_t contour_idx  	= size_t(-1);
		size_t start_point_idx  = size_t(-1);
		// 到最近点的有符号距离。
		double distance 		= std::numeric_limits<double>::max();
		// 从 start_point_idx 开始的边上最近点的参数 <0, 1)
		double t 				= 0.;

		bool valid() const { return contour_idx != size_t(-1); }
	};
	ClosestPointResult closest_point_signed_distance(const Point &pt, coord_t search_radius) const;

	// 仅对闭合轮廓调用此函数！
	bool signed_distance_edges(const Point &pt, coord_t search_radius, coordf_t &result_min_dist, bool *pon_segment = nullptr) const;

	// 计算从点到搜索半径内轮廓的有符号距离。如果在搜索半径内未找到边缘，
	// 则从 m_signed_distance_field 返回插值（如果存在）。
	// 仅对闭合轮廓调用此函数！
	bool signed_distance(const Point &pt, coord_t search_radius, coordf_t &result_min_dist) const;

	const BoundingBox& 	bbox() const { return m_bbox; }
	const coord_t 		resolution() const { return m_resolution; }
	const size_t		rows() const { return m_rows; }
	const size_t		cols() const { return m_cols; }

	// 用于支撑：包围光栅化边缘的轮廓。
	Polygons 			contours_simplified(coord_t offset, bool fill_holes) const;

	typedef std::pair<const Contour*, size_t> ContourPoint;
	typedef std::pair<const Contour*, size_t> ContourEdge;
	std::vector<std::pair<ContourEdge, ContourEdge>> intersecting_edges() const;
	bool 											 has_intersecting_edges() const;

	template<typename VISITOR> void visit_cells_intersecting_line(Slic3r::Point p1, Slic3r::Point p2, VISITOR &visitor) const
	{
		// 线段的端点。
		assert(m_bbox.contains(p1));
		assert(m_bbox.contains(p2));
		p1 -= m_bbox.min;
		p2 -= m_bbox.min;
        assert(p1.x() >= 0 && size_t(p1.x()) < m_cols * m_resolution);
        assert(p1.y() >= 0 && size_t(p1.y()) < m_rows * m_resolution);
        assert(p2.x() >= 0 && size_t(p2.x()) < m_cols * m_resolution);
        assert(p2.y() >= 0 && size_t(p2.y()) < m_rows * m_resolution);
		// 获取端点的单元格。
		coord_t ix = p1(0) / m_resolution;
		coord_t iy = p1(1) / m_resolution;
		coord_t ixb = p2(0) / m_resolution;
		coord_t iyb = p2(1) / m_resolution;
		assert(ix >= 0 && size_t(ix) < m_cols);
		assert(iy >= 0 && size_t(iy) < m_rows);
		assert(ixb >= 0 && size_t(ixb) < m_cols);
		assert(iyb >= 0 && size_t(iyb) < m_rows);
		// 考虑端点。
		if (! visitor(iy, ix) || (ix == ixb && iy == iyb))
			// 两端落入同一单元格。
			return;
		// 光栅化线的中心部分。
		coord_t dx = std::abs(p2(0) - p1(0));
		coord_t dy = std::abs(p2(1) - p1(1));
		if (p1(0) < p2(0)) {
			int64_t ex = int64_t((ix + 1)*m_resolution - p1(0)) * int64_t(dy);
			if (p1(1) < p2(1)) {
				// x 正方向，y 正方向
				int64_t ey = int64_t((iy + 1)*m_resolution - p1(1)) * int64_t(dx);
				do {
					assert(ix <= ixb && iy <= iyb);
					if (ex < ey) {
						ey -= ex;
						ex = int64_t(dy) * m_resolution;
						ix += 1;
						assert(ix <= ixb);
					}
					else if (ex == ey) {
						ex = int64_t(dy) * m_resolution;
						ey = int64_t(dx) * m_resolution;
						ix += 1;
						iy += 1;
						assert(ix <= ixb);
						assert(iy <= iyb);
					}
					else {
						assert(ex > ey);
						ex -= ey;
						ey = int64_t(dx) * m_resolution;
						iy += 1;
						assert(iy <= iyb);
					}
					if (! visitor(iy, ix))
						return;
				} while (ix != ixb || iy != iyb);
			}
			else {
				// x 正方向，y 负方向
				int64_t ey = int64_t(p1(1) - iy*m_resolution) * int64_t(dx);
				do {
					assert(ix <= ixb && iy >= iyb);
					if (ex <= ey) {
						ey -= ex;
						ex = int64_t(dy) * m_resolution;
						ix += 1;
						assert(ix <= ixb);
					}
					else {
						ex -= ey;
						ey = int64_t(dx) * m_resolution;
						iy -= 1;
						assert(iy >= iyb);
					}
					if (! visitor(iy, ix))
						return;
				} while (ix != ixb || iy != iyb);
			}
		}
		else {
			int64_t ex = int64_t(p1(0) - ix*m_resolution) * int64_t(dy);
			if (p1(1) < p2(1)) {
				// x 负方向，y 正方向
				int64_t ey = int64_t((iy + 1)*m_resolution - p1(1)) * int64_t(dx);
				do {
					assert(ix >= ixb && iy <= iyb);
					if (ex < ey) {
						ey -= ex;
						ex = int64_t(dy) * m_resolution;
						ix -= 1;
						assert(ix >= ixb);
					}
					else {
						assert(ex >= ey);
						ex -= ey;
						ey = int64_t(dx) * m_resolution;
						iy += 1;
						assert(iy <= iyb);
					}
					if (! visitor(iy, ix))
						return;
				} while (ix != ixb || iy != iyb);
			}
			else {
				// x 负方向，y 负方向
				int64_t ey = int64_t(p1(1) - iy*m_resolution) * int64_t(dx);
				do {
					assert(ix >= ixb && iy >= iyb);
					if (ex < ey) {
						ey -= ex;
						ex = int64_t(dy) * m_resolution;
						ix -= 1;
						assert(ix >= ixb);
					}
					else if (ex == ey) {
						// 网格单元格的下边缘属于该单元格。
						// 处理一般情况下射线可能穿过单元格左下角的情况，
						// 或退化情况下（水平或垂直线）的左边缘或下边缘。
						if (dx > 0) {
							ex = int64_t(dy) * m_resolution;
							ix -= 1;
							assert(ix >= ixb);
						}
						if (dy > 0) {
							ey = int64_t(dx) * m_resolution;
							iy -= 1;
							assert(iy >= iyb);
						}
					}
					else {
						assert(ex > ey);
						ex -= ey;
						ey = int64_t(dx) * m_resolution;
						iy -= 1;
						assert(iy >= iyb);
					}
					if (! visitor(iy, ix))
						return;
				} while (ix != ixb || iy != iyb);
			}
		}
	}

	template<typename VISITOR> void visit_cells_intersecting_box(BoundingBox bbox, VISITOR &visitor) const
	{
		// 线段的端点。
		bbox.min -= m_bbox.min;
		bbox.max -= m_bbox.min + Point(1, 1);
		// 获取端点的单元格。
		bbox.min /= m_resolution;
		bbox.max /= m_resolution;
		// 用单元格进行裁剪。
		bbox.min.x() = std::max<coord_t>(bbox.min.x(), 0);
		bbox.min.y() = std::max<coord_t>(bbox.min.y(), 0);
		bbox.max.x() = std::min<coord_t>(bbox.max.x(), (coord_t)m_cols - 1);
		bbox.max.y() = std::min<coord_t>(bbox.max.y(), (coord_t)m_rows - 1);
		for (coord_t iy = bbox.min.y(); iy <= bbox.max.y(); ++ iy)
			for (coord_t ix = bbox.min.x(); ix <= bbox.max.x(); ++ ix)
				if (! visitor(iy, ix))
					return;
	}

    std::pair<std::vector<std::pair<size_t, size_t>>::const_iterator, std::vector<std::pair<size_t, size_t>>::const_iterator> cell_data_range(coord_t row, coord_t col) const
	{
        assert(row >= 0 && size_t(row) < m_rows);
        assert(col >= 0 && size_t(col) < m_cols);
		const EdgeGrid::Grid::Cell &cell = m_cells[row * m_cols + col];
		return std::make_pair(m_cell_data.begin() + cell.begin, m_cell_data.begin() + cell.end);
	}

	std::pair<const Slic3r::Point&, const Slic3r::Point&> segment(const std::pair<size_t, size_t> &contour_and_segment_idx) const
	{
		const Contour &contour = m_contours[contour_and_segment_idx.first];
		size_t iseg = contour_and_segment_idx.second;
		return std::pair<const Slic3r::Point&, const Slic3r::Point&>(contour.segment_start(iseg), contour.segment_end(iseg));
	}

	Line line(const std::pair<size_t, size_t> &contour_and_segment_idx) const
	{
		const Contour &contour = m_contours[contour_and_segment_idx.first];
		size_t iseg = contour_and_segment_idx.second;
		return Line(contour.segment_start(iseg), contour.segment_end(iseg));
	}

protected:
	struct Cell {
		Cell() : begin(0), end(0) {}
		size_t begin;
		size_t end;
	};

	void create_from_m_contours(coord_t resolution);
#if 0
	bool line_cell_intersect(const Point &p1, const Point &p2, const Cell &cell);
#endif
	bool cell_inside_or_crossing(int r, int c) const
	{
		if (r < 0 || (size_t)r >= m_rows ||
			c < 0 || (size_t)c >= m_cols)
			// 单元格在域外。希望轮廓已正确定向，因此
			// 存在一个逆时针（CCW）最外轮廓，使得域外的单元格位于外部。
			return false;
		const Cell &cell = m_cells[r * m_cols + c];
		return 
			(cell.begin < cell.end) || 
			(! m_signed_distance_field.empty() && m_signed_distance_field[r * (m_cols + 1) + c] <= 0.f);
	}

	// 轮廓周围的边界框。
	BoundingBox 								m_bbox;
	// 网格尺寸。
	coord_t										m_resolution;
	size_t										m_rows = 0;
	size_t										m_cols = 0;

	// 引用源轮廓。
	// 此格式允许处理任何 Slic3r 定点轮廓格式
	//（Polygon, ExPolygon, ExPolygons 等）。
	std::vector<Contour>						m_contours;

	// 引用 m_contours 的轮廓和线段。
	std::vector<std::pair<size_t, size_t> >		m_cell_data;

	// 完整网格的单元格。
	std::vector<Cell> 							m_cells;

	// 从边缘网格导出的距离场，由 Danielsson 倒角度量填充种子。
	// 可能为空。
	std::vector<float>							m_signed_distance_field;
};

// 调试工具。保存有符号距离场。
extern void save_png(const Grid &grid, const BoundingBox &bbox, coord_t resolution, const char *path, size_t scale = 1);

} // namespace EdgeGrid

// 从多边形集合中查找所有相交边对。
extern std::vector<std::pair<EdgeGrid::Grid::ContourEdge, EdgeGrid::Grid::ContourEdge>> intersecting_edges(const Polygons &polygons);

// 从多边形集合中查找所有相交边对，并在 SVG 中高亮显示。
extern void export_intersections_to_svg(const std::string &filename, const Polygons &polygons);

} // namespace Slic3r

#endif /* slic3r_EdgeGrid_hpp_ */
