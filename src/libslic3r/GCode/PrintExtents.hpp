// 测量计划挤出的范围。
// 用于碰撞报告。

#ifndef slic3r_PrintExtents_hpp_
#define slic3r_PrintExtents_hpp_

#include "libslic3r.h"

namespace Slic3r {

class Print;
class PrintObject;
class BoundingBoxf;

// 返回裙边和 skirt 投影的边界框。
BoundingBoxf get_print_extrusions_extents(const Print &print);

// 返回z <= max_print_z处对象挤出投影的边界框。
BoundingBoxf get_print_object_extrusions_extents(const PrintObject &print_object, const coordf_t max_print_z);

// 返回z <= max_print_z层擦拭塔投影的边界框。
// 投影不包含初始区域。
BoundingBoxf get_wipe_tower_extrusions_extents(const Print &print, const coordf_t max_print_z);

// 返回擦拭塔初始挤出的边界框。
BoundingBoxf get_wipe_tower_priming_extrusions_extents(const Print &print);

};

#endif /* slic3r_PrintExtents_hpp_ */
