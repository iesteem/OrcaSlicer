#ifndef slic3r_RetractWhenCrossingPerimeters_hpp_
#define slic3r_RetractWhenCrossingPerimeters_hpp_

#include <vector>

#include "../AABBTreeIndirect.hpp"

namespace Slic3r {

// 前向声明。
class ExPolygon;
class Layer;
class Polyline;

class RetractWhenCrossingPerimeters
{
public:
    bool    travel_inside_internal_regions(const Layer &layer, const Polyline &travel);

private:
    // 上次访问的对象层，为其创建了内部岛缓存。
    const Layer                        *m_layer;
    // 仅内部岛，引用由m_layer->regions()->surfaces()拥有的数据。
    std::vector<const ExPolygon*>       m_internal_islands;
    // 内部岛的搜索结构。
    using AABBTree = AABBTreeIndirect::Tree<2, coord_t>;
    AABBTree                            m_aabbtree_internal_islands;
};

} // namespace Slic3r

#endif // slic3r_RetractWhenCrossingPerimeters_hpp_
