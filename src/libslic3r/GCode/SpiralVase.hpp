#ifndef slic3r_SpiralVase_hpp_
#define slic3r_SpiralVase_hpp_

#include "../libslic3r.h"
#include "../GCodeReader.hpp"

namespace Slic3r {

class SpiralVase
{
public:
    class SpiralPoint
    {
    public:
        SpiralPoint(float paramx, float paramy) : x(paramx), y(paramy) {}

    public:
        float x, y;
    };
    SpiralVase(const PrintConfig &config) : m_config(config)
    {
        m_reader.z() = (float)m_config.z_offset;
        m_reader.apply_config(m_config);
        m_previous_layer = NULL;
        m_smooth_spiral = config.spiral_mode_smooth;
    };

    void     enable(bool en) {
        m_transition_layer = en && ! m_enabled;
        m_enabled          = en;
    }

    std::string process_layer(const std::string &gcode, bool last_layer);
    void set_max_xy_smoothing(float max) {
        m_max_xy_smoothing = max;
    }
private:
    const PrintConfig  &m_config;
    GCodeReader         m_reader;
    float               m_max_xy_smoothing = 0.f;

    bool                m_enabled = false;
    // 第一个螺旋花瓶层。层高必须从零逐渐增加到目标层高。
    bool                m_transition_layer = false;
    // 是否与上一层插值XY坐标。结果是在层变化处没有接缝。
    bool                m_smooth_spiral = false;
    std::vector<SpiralPoint> * m_previous_layer;
};
}

#endif // slic3r_SpiralVase_hpp_
