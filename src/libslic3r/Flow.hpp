#ifndef slic3r_Flow_hpp_
#define slic3r_Flow_hpp_

#include "libslic3r.h"
#include "Config.hpp"
#include "Exception.hpp"
#include "ExtrusionEntity.hpp"

namespace Slic3r {

class PrintObject;

// 桥接线材的额外间距，单位为 mm。
#define BRIDGE_EXTRA_SPACING 0.05

enum FlowRole {
    frExternalPerimeter,
    frPerimeter,
    frInfill,
    frSolidInfill,
    frTopSolidInfill,
    frSupportMaterial,
    frSupportMaterialInterface,
    frSupportTransition,  // BBS
};

class FlowError : public Slic3r::InvalidArgument
{
public:
	FlowError(const std::string& what_arg) : Slic3r::InvalidArgument(what_arg) {}
	FlowError(const char* what_arg) : Slic3r::InvalidArgument(what_arg) {}
};

class FlowErrorNegativeSpacing : public FlowError
{
public:
    FlowErrorNegativeSpacing();
};

class FlowErrorNegativeFlow : public FlowError
{
public:
    FlowErrorNegativeFlow();
};

class FlowErrorMissingVariable : public FlowError
{
public:
    FlowErrorMissingVariable(const std::string& what_arg) : FlowError(what_arg) {}
};

class Flow
{
public:
    Flow() = default;
    Flow(float width, float height, float nozzle_diameter) :
        Flow(width, height, rounded_rectangle_extrusion_spacing(width, height), nozzle_diameter, false) {}

    // 非桥接流量：两端带半圆的挤出最大宽度。
    // 桥接流量：桥接线材直径。
    float   width()           const { return m_width; }
    coord_t scaled_width()    const { return coord_t(scale_(m_width)); }
    // 非桥接流量：层高。
    // 桥接流量：桥接线材直径 = 层高。
    float   height()          const { return m_height; }
    // 挤出中心线之间的间距。
    float   spacing()         const { return m_spacing; }
    void    set_spacing(float spacing) { m_spacing = spacing; }
    coord_t scaled_spacing()  const { return coord_t(scale_(m_spacing)); }
    // 喷嘴直径。
    float   nozzle_diameter() const { return m_nozzle_diameter; }
    // 是否为桥接？
    bool    bridge()          const { return m_bridge; }
    // 挤出的横截面积。
    double  mm3_per_mm()      const;

    // 象脚补偿间距，用于检测无法应用象脚补偿的狭窄部分。
    // 仅在 frExternalPerimeter 上使用。
    // 启用一些周长挤压（参见 INSET_OVERLAP_TOLERANCE）。
    // 这里象脚补偿允许 0.2 倍外部周长间距的重叠。
    coord_t scaled_elephant_foot_spacing() const { return coord_t(0.5f * float(this->scaled_width() + 0.6f * this->scaled_spacing())); }

    bool operator==(const Flow &rhs) const { return m_width == rhs.m_width && m_height == rhs.m_height && m_nozzle_diameter == rhs.m_nozzle_diameter && m_bridge == rhs.m_bridge; }

    bool operator!=(const Flow &rhs) const{
        return m_width != rhs.m_width || m_height != rhs.m_height || m_nozzle_diameter != rhs.m_nozzle_diameter || m_bridge != rhs.m_bridge;
    }

    bool operator <(const Flow &rhs) const {
        return this->mm3_per_mm() < rhs.mm3_per_mm();
    }
    Flow        with_width (float width)  const { 
        assert(! m_bridge); 
        return Flow(width, m_height, rounded_rectangle_extrusion_spacing(width, m_height), m_nozzle_diameter, m_bridge);
    }
    Flow        with_height(float height) const { 
        assert(! m_bridge); 
        return Flow(m_width, height, rounded_rectangle_extrusion_spacing(m_width, height), m_nozzle_diameter, m_bridge);
    }
    // 调整挤出流量以适应新的挤出线间距，保持挤出之间的旧间距。
    Flow        with_spacing(float spacing) const;
    // 调整圆形挤出模型的宽度/高度以达到规定的横截面积，同时保持挤出间距。
    Flow        with_cross_section(float area) const;
    Flow        with_flow_ratio(double ratio) const { return this->with_cross_section(this->mm3_per_mm() * ratio); }

    static Flow bridging_flow(float dmr, float nozzle_diameter) { return Flow { dmr, dmr, bridge_extrusion_spacing(dmr), nozzle_diameter, true }; }

    static Flow new_from_config_width(FlowRole role, const ConfigOptionFloatOrPercent &width, float nozzle_diameter, float height);

    // 圆形挤出模型的挤出间距。
    static float rounded_rectangle_extrusion_spacing(float width, float height);
    // 圆形挤出模型的挤出宽度。
    static float rounded_rectangle_extrusion_width_from_spacing(float spacing, float height);
    // 圆形线材挤出的间距。
    static float bridge_extrusion_spacing(float dmr);

    // 基于喷嘴直径的合理挤出宽度默认值。
    // 默认值源自手动 Prusa MK3 配置。
    static float auto_extrusion_width(FlowRole role, float nozzle_diameter);

    // 从完整配置中获取挤出宽度，考虑默认值（设置为零时）和比例（百分比）。
    // 精确值取决于层索引（第一层与其他层 vs 可变层高）、
    // 激活的挤出机等。因此，此函数计算的值仅应作为提示使用。
	static double extrusion_width(const std::string &opt_key, const ConfigOptionFloatOrPercent *opt, const ConfigOptionResolver &config, const unsigned int first_printing_extruder = 0);
	static double extrusion_width(const std::string &opt_key, const ConfigOptionResolver &config, const unsigned int first_printing_extruder = 0);

private:
    Flow(float width, float height, float spacing, float nozzle_diameter, bool bridge) : 
        m_width(width), m_height(height), m_spacing(spacing), m_nozzle_diameter(nozzle_diameter), m_bridge(bridge) 
        { 
            // 间隙填充违反此条件。
            //assert(width >= height); 
        }

    float       m_width { 0 };
    float       m_height { 0 };
    float       m_spacing { 0 };
    float       m_nozzle_diameter { 0 };
    bool        m_bridge { false };
};

extern Flow support_material_flow(const PrintObject* object, float layer_height = 0.f);
extern Flow support_transition_flow(const PrintObject *object); //BBS
extern Flow support_material_1st_layer_flow(const PrintObject *object, float layer_height = 0.f);
extern Flow support_material_interface_flow(const PrintObject *object, float layer_height = 0.f);

}

#endif
