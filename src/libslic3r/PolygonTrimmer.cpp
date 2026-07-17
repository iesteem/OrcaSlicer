#include "PolygonTrimmer.hpp"
#include "EdgeGrid.hpp"
#include "Geometry.hpp"

namespace Slic3r {

TrimmedLoop trim_loop(const Polygon &loop, const EdgeGrid::Grid &grid)
{
	assert(! loop.empty());
	assert(loop.size() >= 2);

	TrimmedLoop out;

	if (loop.size() >= 2) {

		struct Visitor {
			Visitor(const EdgeGrid::Grid &grid, const Slic3r::Point *pt_prev, const Slic3r::Point *pt_this) : grid(grid), pt_prev(pt_prev), pt_this(pt_this) {}

			bool operator()(coord_t iy, coord_t ix) {
				// 调用时传入与线相交的网格单元的行和列。
				auto cell_data_range = grid.cell_data_range(iy, ix);
				for (auto it_contour_and_segment = cell_data_range.first; it_contour_and_segment != cell_data_range.second; ++ it_contour_and_segment) {
					// 线段的端点及其向量。
					auto segment = grid.segment(*it_contour_and_segment);
					if (Geometry::segments_intersect(segment.first, segment.second, *pt_prev, *pt_this)) {
						// 两个线段相交。将它们添加到输出。
					}
				}
				// 继续沿边缘遍历网格。
				return true;
			}

			const EdgeGrid::Grid &grid;
			const Slic3r::Point  *pt_this;
			const Slic3r::Point  *pt_prev;
		} visitor(grid, &loop.points.back(), nullptr);

		for (const Point &pt_this : loop.points) {
			visitor.pt_this = &pt_this;
			grid.visit_cells_intersecting_line(*visitor.pt_prev, pt_this, visitor);
			visitor.pt_prev = &pt_this;
		}
	}

	return out;
}

std::vector<TrimmedLoop> trim_loops(const Polygons &loops, const EdgeGrid::Grid &grid)
{
	std::vector<TrimmedLoop> out;
	out.reserve(loops.size());
	for (const Polygon &loop : loops)
		out.emplace_back(trim_loop(loop, grid));
	return out;
}

}
