#ifndef slic3r_AvoidCrossingPerimeters_hpp_
#define slic3r_AvoidCrossingPerimeters_hpp_

#include "../libslic3r.h"
#include "../ExPolygon.hpp"
#include "../EdgeGrid.hpp"

namespace Slic3r {

// 前向声明。
class GCode;
class Layer;
class Point;

class AvoidCrossingPerimeters
{
public:
    // 绕对象外面行走 vs. 在单个对象内部行走。
    void        use_external_mp(bool use = true) { m_use_external_mp = use; };
    bool        used_external_mp() { return m_use_external_mp; }
    void        use_external_mp_once()  { m_use_external_mp_once = true; }
    bool        used_external_mp_once() { return m_use_external_mp_once; }
    void        disable_once()          { m_disabled_once = true; }
    bool        disabled_once() const   { return m_disabled_once; }
    void        reset_once_modifiers()  { m_use_external_mp_once = false; m_disabled_once = false; }

    void        init_layer(const Layer &layer);

    Polyline    travel_to(const GCode& gcodegen, const Point& point)
    {
        bool could_be_wipe_disabled;
        return this->travel_to(gcodegen, point, &could_be_wipe_disabled);
    }

    Polyline    travel_to(const GCode& gcodegen, const Point& point, bool* could_be_wipe_disabled);

    struct Boundary {
        // 用于检测移动中周长穿越的边界集合
        Polygons                        boundaries;
        // 边界的边界框
        BoundingBoxf                    bbox;
        // 边界中所有点的预计算距离
        std::vector<std::vector<float>> boundaries_params;
        // 用于检测线段与边界中任何多边形之间的交点
        EdgeGrid::Grid                  grid;

        void clear()
        {
            boundaries.clear();
            boundaries_params.clear();
        }
    };

private:
    bool           m_use_external_mp { false };
    // 仅用于下一个移动移动
    bool           m_use_external_mp_once { false };
    // 此标志仅针对下一个移动移动禁用reduce_crossing_wall
    // 我们默认在打印的第一个移动移动中启用它
    bool           m_disabled_once { true };

    // 按一半外部周长宽度偏移的L层切片。用于检测线段或多段线是否在任何多边形内部。
    ExPolygons               m_lslices_offset;
    std::vector<BoundingBox> m_lslices_offset_bboxes;
    // 用于检测线段或多段线是否在任何多边形内部。
    EdgeGrid::Grid m_grid_lslice;
    // 存储对象内移动所需的所有数据
    Boundary m_internal;
    // 存储对象外移动所需的所有数据
    Boundary m_external;
};

} // namespace Slic3r

#endif // slic3r_AvoidCrossingPerimeters_hpp_
