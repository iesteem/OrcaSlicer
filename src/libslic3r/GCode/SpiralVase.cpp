#include "SpiralVase.hpp"
#include "GCode.hpp"
#include <sstream>
#include <cmath>
#include <limits>

namespace Slic3r {

namespace SpiralVaseHelpers {
/** == 平滑螺旋辅助函数 == */
/** a和b之间的距离 */
float distance(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) { return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2)); }

SpiralVase::SpiralPoint subtract(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b)
{
    return SpiralVase::SpiralPoint(a.x - b.x, a.y - b.y);
}

SpiralVase::SpiralPoint add(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) { return SpiralVase::SpiralPoint(a.x + b.x, a.y + b.y); }

SpiralVase::SpiralPoint scale(SpiralVase::SpiralPoint a, float factor) { return SpiralVase::SpiralPoint(a.x * factor, a.y * factor); }

/** 点积 */
float dot(SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b) { return a.x * b.x + a.y * b.y; }

/** 找到线ab上最接近点c的点 */
SpiralVase::SpiralPoint nearest_point_on_line(SpiralVase::SpiralPoint c, SpiralVase::SpiralPoint a, SpiralVase::SpiralPoint b, float& dist)
{
    SpiralVase::SpiralPoint ab      = subtract(b, a);
    SpiralVase::SpiralPoint ca      = subtract(c, a);
    float                   t       = dot(ca, ab) / dot(ab, ab);
    t                               = t > 1 ? 1 : t;
    t                               = t < 0 ? 0 : t;
    SpiralVase::SpiralPoint closest = SpiralVase::SpiralPoint(add(a, scale(ab, t)));
    dist                            = distance(c, closest);
    return closest;
}

/** 给定由点定义的一组线，如line[n]是从points[n]到points[n+1]的线，
 *  找到最接近p且落在任何线上的点 */
SpiralVase::SpiralPoint nearest_point_on_lines(SpiralVase::SpiralPoint               p,
                                               std::vector<SpiralVase::SpiralPoint>* points,
                                               bool&                                 found,
                                               float&                                dist)
{
    if (points->size() < 2) {
        found = false;
        return SpiralVase::SpiralPoint(0, 0);
    }
    float                   min = std::numeric_limits<float>::max();
    SpiralVase::SpiralPoint closest(0, 0);
    for (unsigned long i = 0; i < points->size() - 1; i++) {
        float                   currentDist = 0;
        SpiralVase::SpiralPoint current     = nearest_point_on_line(p, points->at(i), points->at(i + 1), currentDist);
        if (currentDist < min) {
            min     = currentDist;
            closest = current;
            found   = true;
        }
    }
    dist = min;
    return closest;
}
} // namespace SpiralVase

std::string SpiralVase::process_layer(const std::string &gcode, bool last_layer)
{
    /*  此后处理器依赖于几个假设：
        - 所有层都通过它处理，包括那些不应转换的层，
          以便用XY位置更新读取器。
        - 对此方法的每次调用都包括一个完整的层，开头有一个Z移动。
        - 每层由合适的几何形状组成（即单个完整环）。
        - 在调用此方法之前，环没有被裁剪。 */

    // 如果我们要修改G-code，只需将其馈送到读取器
    // 以更新位置。
    if (! m_enabled) {
        m_reader.parse_buffer(gcode);
        return gcode;
    }

    // 通过求和所有挤出移动获取此层的总XY长度。
    float total_layer_length = 0;
    float layer_height = 0;
    float z = 0.f;

    {
        //FIXME 性能警告：这会复制读取器的GCodeConfig。
        GCodeReader r = m_reader;  // 克隆
        bool set_z = false;
        r.parse_buffer(gcode, [&total_layer_length, &layer_height, &z, &set_z]
            (GCodeReader &reader, const GCodeReader::GCodeLine &line) {
            if (line.cmd_is("G1")) {
                if (line.extruding(reader)) {
                    total_layer_length += line.dist_XY(reader);
                } else if (line.has(Z)) {
                    layer_height += line.dist_Z(reader);
                    if (!set_z) {
                        z = line.new_Z(reader);
                        set_z = true;
                    }
                }
            }
        });
    }

    // 从初始Z中移除层高。
    z -= layer_height;

    std::vector<SpiralVase::SpiralPoint>* current_layer = new std::vector<SpiralVase::SpiralPoint>();
    std::vector<SpiralVase::SpiralPoint>* previous_layer = m_previous_layer;

    bool smooth_spiral = m_smooth_spiral;
    std::string new_gcode;
    std::string transition_gcode;
    float max_xy_dist_for_smoothing = m_max_xy_smoothing;
    //FIXME 过渡层的渐变更可靠地处理相对挤出机距离。
    // 对于绝对挤出机距离，它将被关闭。
    // 渐变更改绝对挤出机距离需要在第一个过渡层之后处理每个挤出值。
    bool  transition_in = m_transition_layer && m_config.use_relative_e_distances.value;
    bool  transition_out = last_layer && m_config.use_relative_e_distances.value;

    float starting_flowrate  = float(m_config.spiral_starting_flow_ratio.value);
    float finishing_flowrate = float(m_config.spiral_finishing_flow_ratio.value);

    float len = 0.f;
    SpiralVase::SpiralPoint last_point = previous_layer != NULL && previous_layer->size() >0? previous_layer->at(previous_layer->size()-1): SpiralVase::SpiralPoint(0,0);
    m_reader.parse_buffer(gcode, [&new_gcode, &z, total_layer_length, layer_height, transition_in, &len, &current_layer, &previous_layer, &transition_gcode, transition_out, smooth_spiral, &max_xy_dist_for_smoothing, &last_point, starting_flowrate, finishing_flowrate]
        (GCodeReader &reader, GCodeReader::GCodeLine line) {
        if (line.cmd_is("G1")) {
            // Orca: 过滤掉层更改时的回抽
            if (line.retracting(reader) || (line.extruding(reader) && line.dist_XY(reader) < EPSILON)) return;
            if (line.has_z() && !(line.has_x() || line.has_y())) {
                // 如果这是层的初始Z移动，将其替换为
                // 到上一层的最后Z的（冗余）移动。
                line.set(Z, z);
                new_gcode += line.raw() + '\n';
                return;
            } else {
                float dist_XY = line.dist_XY(reader);
                if (line.has_x() || line.has_y()) { // 有时行有X/Y但移动到最后一个位置
                    if (dist_XY > 0 && line.extruding(reader)) { // 排除擦拭和回抽
                        len += dist_XY;
                        float factor = len / total_layer_length;
                        if (transition_in){
                            // 过渡层，从spiral_vase_starting_flow_rate到100%内插挤出量。
                            float starting_e_factor = starting_flowrate + (factor * (1.f - starting_flowrate));
                            line.set(E, line.e() * starting_e_factor, 5 /*decimal_digits*/);
                        } else if (transition_out) {
                            // 我们想要最后一层逐渐减少挤出，但不改变z高度！
                            // 所以先克隆该行，然后修改其Z并复制到逐渐减少E的新层
                            // 我们在最后添加这个新层
                            // 与transition_in一样，量从100%逐渐减少到spiral_vase_finishing_flow_rate
                            GCodeReader::GCodeLine transitionLine(line);
                            float finishing_e_factor = finishing_flowrate + ((1.f -factor) * (1.f - finishing_flowrate));
                            transitionLine.set(E, line.e() * finishing_e_factor, 5 /*decimal_digits*/);
                            transition_gcode += transitionLine.raw() + '\n';
                        }
                        // 这行是Spiral Vase模式的核心，平滑地增加Z
                        line.set(Z, z + factor * layer_height);
                        if (smooth_spiral) {
                            // 现在还需要尝试插值X和Y
                            SpiralVase::SpiralPoint p(line.x(), line.y()); // 获取当前x/y坐标
                            current_layer->push_back(p);       // 存储该点供以后的层使用
                            if (previous_layer != NULL) {
                                bool        found    = false;
                                float       dist     = 0;
                                SpiralVase::SpiralPoint nearestp = SpiralVaseHelpers::nearest_point_on_lines(p, previous_layer, found, dist);
                                if (found && dist < max_xy_dist_for_smoothing) {
                                    // 在此层的点和上一层的点之间进行插值
                                    SpiralVase::SpiralPoint target = SpiralVaseHelpers::add(SpiralVaseHelpers::scale(nearestp, 1 - factor), SpiralVaseHelpers::scale(p, factor));

                                    // 移除微小移动
                                    // 我们需要计算这条新线的距离！
                                    float modified_dist_XY = SpiralVaseHelpers::distance(last_point, target);
                                    if (modified_dist_XY < 0.001)
                                        line.clear();
                                    else {
                                        line.set(X, target.x);
                                        line.set(Y, target.y);
                                        // 根据长度变化缩放挤出量
                                        line.set(E, line.e() * modified_dist_XY / dist_XY, 5 /*decimal_digits*/);
                                        last_point = target;
                                    }
                                } else {
                                    last_point = p;
                                }
                            }
                        }
                        new_gcode += line.raw() + '\n';
                    }
                    return;
                    /*  跳过移动移动：移动到第一个周长点的移动
                        会在环在XY中不对齐时导致可见接缝；通过跳过它，
                        我们在XY平面中混合第一个环移动（虽然这种混合的平滑
                        程度取决于第一个段的长度；也许我们应该
                        强制一些最小长度？）。
                        当启用smooth_spiral时，无论如何我们都会最终到达下一层
                        应该开始的位置，所以我们不需要移动移动 */
                }
            }
        }
        new_gcode += line.raw() + '\n';
        if(transition_out) {
            transition_gcode += line.raw() + '\n';
        }
    });

    delete m_previous_layer;
    m_previous_layer = current_layer;

    return new_gcode + transition_gcode;
}

}
