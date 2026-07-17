#ifndef slic3r_CoolingBuffer_hpp_
#define slic3r_CoolingBuffer_hpp_

#include "../libslic3r.h"
#include <map>
#include <string>
#include <cfloat>

namespace Slic3r {

class GCode;
class Layer;
struct PerExtruderAdjustments;

// 一个独立的G-code过滤器，用于控制打印的冷却。
// G-code按层处理。收集一层后，编辑风扇开启/停止命令
// 并修改打印以延长最小层时间。
//
// 虽然听起来简单，但实际实现要复杂得多。
// 即，对于多挤出机打印，每种材料可能需要不同的冷却逻辑。
// 例如，某些材料可能不喜欢打印太慢，而某些材料
// 我们可能需要大幅减速。
//
class CoolingBuffer {
public:
    CoolingBuffer(GCode &gcodegen);
    void        reset(const Vec3d &position);
    void        set_current_extruder(unsigned int extruder_id) { m_current_extruder = extruder_id; }
    std::string process_layer(std::string &&gcode, size_t layer_id, bool flush);

private:
    CoolingBuffer& operator=(const CoolingBuffer&) = delete;
    std::vector<PerExtruderAdjustments> parse_layer_gcode(const std::string &gcode, std::vector<float> &current_pos) const;
    float       calculate_layer_slowdown(std::vector<PerExtruderAdjustments> &per_extruder_adjustments);
    // 在存储在per_extruder_adjustments中的G-code行上应用减速，必要时启用风扇。
    // 返回调整后的G-code。
    std::string apply_layer_cooldown(const std::string &gcode, size_t layer_id, float layer_time, std::vector<PerExtruderAdjustments> &per_extruder_adjustments);

    // 为对象层前面的支撑层缓存的G-code片段。
    std::string                 m_gcode;
    // 内部数据。
    // BBS: X,Y,Z,E,F,I,J
    std::vector<char>           m_axis;
    std::vector<float>          m_current_pos;
    // 当前已知风扇速度，如果未知则为-1。
    int                         m_fan_speed;
    int                         m_additional_fan_speed;
    // 从GCodeWriter缓存。
    // 打印挤出机ID，从零开始。
    std::vector<unsigned int>   m_extruder_ids;
    // m_extruder_ids的最大值加1。
    unsigned int                m_num_extruders { 0 };
    const std::string           m_toolchange_prefix;
    // 引用GCode::m_config，即FullPrintConfig。虽然FullPrintConfig的PrintObjectConfig切片被修改，
    // FullPrintConfig的PrintConfig切片是常量，因此不需要线程同步。
    const PrintConfig          &m_config;
    unsigned int                m_current_extruder;
    //BBS: 当前风扇速度
    int                         m_current_fan_speed;
};

}

#endif
