#ifndef slic3r_Extruder_hpp_
#define slic3r_Extruder_hpp_

#include "libslic3r.h"
#include "Point.hpp"

namespace Slic3r {

class GCodeConfig;

class Extruder
{
public:
    Extruder(unsigned int id, GCodeConfig *config, bool share_extruder);
    virtual ~Extruder() {}

    void   reset() {
        // BBS
        if (m_share_extruder) {
            m_share_E = 0.;
            m_share_retracted = 0.;
        } else {
            m_E             = 0;
            m_retracted     = 0;
        }
        m_restart_extra = 0;
        m_absolute_E    = 0;
    }

    unsigned int id() const { return m_id; }

    double extrude(double dE);
    double retract(double length, double restart_extra);
    double unretract();
    double E() const { return m_share_extruder ? m_share_E : m_E; }
    void   reset_E() { m_E = 0.; m_share_E = 0.; }
    // e_per_mm 是 extrusion_per_mm = 几何体积 * (耗材流量比 / 横截面积) [不考虑 print_flow_ratio 或桥接流量比等修饰符]
    double e_per_mm(double mm3_per_mm) const { return mm3_per_mm * m_e_per_mm3; }
    // e_per_mm3 是 extrusion_per_mm3 = 耗材流量比 / 横截面积 [不考虑 print_flow_ratio 或桥接流量比等修饰符]
    double e_per_mm3() const { return m_e_per_mm3; }
    // 已使用的耗材体积，单位 mm^3。
    double extruded_volume() const;
    // 已使用的耗材长度，单位 mm。
    double used_filament() const;
    
    // Getters for the PlaceholderParser.
    // Get current extruder position. Only applicable with absolute extruder addressing.
    double position() const { return m_E; }
    // 获取当前回抽值。仅非负值。
    double retracted() const { return m_retracted; }
    // 获取计划后的额外回抽
    double restart_extra() const { return m_restart_extra; }
    // Setters for the PlaceholderParser.
    // Set current extruder position. Only applicable with absolute extruder addressing.
    void   set_position(double e) { m_E = e; }
    // 设置当前回抽值和额外重启耗材量（如果 retracted > 0）。
    void   set_retracted(double retracted, double restart_extra);
    
    double filament_diameter() const;
    double filament_crossection() const { return this->filament_diameter() * this->filament_diameter() * 0.25 * PI; }
    double filament_density() const;
    double filament_cost() const;
    double filament_flow_ratio() const;
    double retract_before_wipe() const;
    double retraction_length() const;
    double retract_lift() const;
    int    retract_speed() const;
    int    deretract_speed() const;
    double retract_restart_extra() const;
    double retract_length_toolchange() const;
    double retract_restart_extra_toolchange() const;
    double travel_slope() const;

    bool   use_firmware_retraction() const;

private:
    // 私有构造函数，用于创建 std::set 中搜索的键。
    Extruder(unsigned int id) : m_id(id) {}

    // 引用 GCodeWriter 拥有的 GCodeWriter 实例。
    GCodeConfig *m_config;
    // 此挤出机的打印范围全局 ID。
    unsigned int m_id;
    // 挤出机轴的当前状态，如果使用相对 e 距离可能会重置。
    double       m_E;
    // 挤出机转速计的当前状态，用于输出 extruded_volume() 和 used_filament() 统计信息。
    double       m_absolute_E;
    // 当前正回抽量。
    double       m_retracted;
    // 当回抽时，此值存储在取消回抽时的额外预挤出量。
    double       m_restart_extra;
    double       m_e_per_mm3;

    // BBS.
    // Create shared E and retraction data for single extruder multi-material machine
    bool          m_share_extruder;
    static double m_share_E;
    static double m_share_retracted;
};

// 默认按挤出机 ID 对 Extruder 对象排序。
inline bool operator==(const Extruder &e1, const Extruder &e2) { return e1.id() == e2.id(); }
inline bool operator!=(const Extruder &e1, const Extruder &e2) { return e1.id() != e2.id(); }
inline bool operator< (const Extruder &e1, const Extruder &e2) { return e1.id() < e2.id(); }
inline bool operator> (const Extruder &e1, const Extruder &e2) { return e1.id() > e2.id(); }

}

#endif
