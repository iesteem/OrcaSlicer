#include "../ClipperUtils.hpp"
#include "../Layer.hpp"
#include "../Polyline.hpp"

#include "RetractWhenCrossingPerimeters.hpp"

namespace Slic3r {

bool RetractWhenCrossingPerimeters::travel_inside_internal_regions(const Layer &layer, const Polyline &travel)
{
    if (m_layer != &layer) {
        // 更新缓存。
        m_layer       = &layer;
        m_internal_islands.clear();
        m_aabbtree_internal_islands.clear();
        // 收集内部切片的expolygon。
        for (const LayerRegion *layerm : layer.regions())
            for (const Surface &surface : layerm->get_slices().surfaces)
                if (surface.is_internal())
                    m_internal_islands.emplace_back(&surface.expolygon);
        // 计算内部切片的边界框。
        std::vector<AABBTreeIndirect::BoundingBoxWrapper> bboxes;
        bboxes.reserve(m_internal_islands.size());
        for (size_t i = 0; i < m_internal_islands.size(); ++ i)
            bboxes.emplace_back(i, get_extents(*m_internal_islands[i]));
        // 在内部切片的边界框上构建AABB树。
        m_aabbtree_internal_islands.build_modify_input(bboxes);
    }

    BoundingBox           bbox_travel = get_extents(travel);
    AABBTree::BoundingBox bbox_travel_eigen{ bbox_travel.min, bbox_travel.max };
    int result = -1;
    bbox_travel.offset(SCALED_EPSILON);
    AABBTreeIndirect::traverse(m_aabbtree_internal_islands,
        [&bbox_travel_eigen](const AABBTree::Node &node) {
            return bbox_travel_eigen.intersects(node.bbox);
        },
        [&travel, &bbox_travel, &result, &islands = m_internal_islands](const AABBTree::Node &node) {
            assert(node.is_leaf());
            assert(node.is_valid());
            Polygons clipped = ClipperUtils::clip_clipper_polygons_with_subject_bbox(*islands[node.idx], bbox_travel);
            if (diff_pl(travel, clipped).empty()) {
                // 移动路径完全在"内部"岛内。不要回抽。
                result = int(node.idx);
                // 停止遍历。
                return false;
            }
            // 继续遍历。
            return true;
        });
    return result != -1;
}

} // namespace Slic3r
