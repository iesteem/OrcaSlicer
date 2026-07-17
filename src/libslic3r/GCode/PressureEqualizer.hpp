#ifndef slic3r_GCode_PressureEqualizer_hpp_
#define slic3r_GCode_PressureEqualizer_hpp_

#include "../libslic3r.h"
#include "../PrintConfig.hpp"

#include <queue>

namespace Slic3r {

struct LayerResult;

class GCodeG1Formatter;

//#define PRESSURE_EQUALIZER_STATISTIC
//#define PRESSURE_EQUALIZER_DEBUG

// 处理G-code。找到体积挤出速度的变化并调整
// 这些路径之间的过渡，以限制体积挤出速度的快速变化。
class PressureEqualizer
{
public:
    PressureEqualizer() = delete;
    explicit PressureEqualizer(const Slic3r::GCodeConfig &config);
    ~PressureEqualizer() = default;

    // 处理下一批G-code行。
    // 最后一个LayerResult必须是LayerResult::make_nop_layer_result()，因为它总是返回前一个层的G-code。
    // 当为第一层调用process_layer时，返回LayerResult::make_nop_layer_result()。
    LayerResult process_layer(LayerResult &&input);
private:

    void process_layer(const std::string &gcode);

#ifdef PRESSURE_EQUALIZER_STATISTIC
    struct Statistics
    {
        void reset()
        {
            volumetric_extrusion_rate_min = std::numeric_limits<float>::max();
            volumetric_extrusion_rate_max = 0.f;
            volumetric_extrusion_rate_avg = 0.f;
            extrusion_length              = 0.f;
        }
        void update(float volumetric_extrusion_rate, float length)
        {
            volumetric_extrusion_rate_min  = std::min(volumetric_extrusion_rate_min, volumetric_extrusion_rate);
            volumetric_extrusion_rate_max  = std::max(volumetric_extrusion_rate_max, volumetric_extrusion_rate);
            volumetric_extrusion_rate_avg += volumetric_extrusion_rate * length;
            extrusion_length              += length;
        }
        float volumetric_extrusion_rate_min;
        float volumetric_extrusion_rate_max;
        float volumetric_extrusion_rate_avg;
        float extrusion_length;
    };

    struct Statistics m_stat;
#endif

    // 私有配置值
    // 体积挤出率可以增加/减少的速度？mm^3/sec^2
    struct ExtrusionRateSlope {
        float positive;
        float negative;
    };
    ExtrusionRateSlope              m_max_volumetric_extrusion_rate_slopes[size_t(ExtrusionRole::erCount)];
    float                           m_max_volumetric_extrusion_rate_slope_positive;
    float                           m_max_volumetric_extrusion_rate_slope_negative;

    // 从配置中提取的配置。
    // 每种丝材的横截面积。需要计算体积流量。
    std::vector<float>              m_filament_crossections;

    // 内部数据。
    // X,Y,Z,E,F
    float                           m_current_pos[5];
    size_t                          m_current_extruder;
    ExtrusionRole     m_current_extrusion_role;
    bool                            m_retracted;
    bool                            m_use_relative_e_distances;

    // 如果初始和最终流量不同，分割长段的最大段长度。
    // 较小的值意味着两种不同流量之间更平滑的过渡。
    float                           m_max_segment_length;

    // 仅对外部周长和悬垂应用ERS
    bool                           m_extrusion_rate_smoothing_external_perimeter_only;

    // 指示挤出设置速度块是否使用标签";_EXTRUDE_SET_SPEED"打开
    // 或未打开（或已使用标签";_EXTRUDE_END"关闭）。
    bool                            opened_extrude_set_speed_block = false;

    enum GCodeLineType {
        GCODELINETYPE_INVALID,
        GCODELINETYPE_NOOP,
        GCODELINETYPE_OTHER,
        GCODELINETYPE_RETRACT,
        GCODELINETYPE_UNRETRACT,
        GCODELINETYPE_TOOL_CHANGE,
        GCODELINETYPE_MOVE,
        GCODELINETYPE_EXTRUDE,
    };

    struct GCodeLine
    {
        GCodeLine() :
            type(GCODELINETYPE_INVALID),
            raw_length(0),
            modified(false),
            extruder_id(0),
            volumetric_extrusion_rate(0.f),
            volumetric_extrusion_rate_start(0.f),
            volumetric_extrusion_rate_end(0.f)
            {}

        bool        moving_xy()     const { return fabs(pos_end[0] - pos_start[0]) > 0.f || fabs(pos_end[1] - pos_start[1]) > 0.f; }
        bool        moving_z ()     const { return fabs(pos_end[2] - pos_start[2]) > 0.f; }
        bool        extruding()     const { return moving_xy() && pos_end[3] > pos_start[3]; }
        bool        retracting()    const { return pos_end[3] < pos_start[3]; }
        bool        deretracting()  const { return ! moving_xy() && pos_end[3] > pos_start[3]; }

        float       dist_xy2()      const { return (pos_end[0] - pos_start[0]) * (pos_end[0] - pos_start[0]) + (pos_end[1] - pos_start[1]) * (pos_end[1] - pos_start[1]); }
        float       dist_xyz2()     const { return (pos_end[0] - pos_start[0]) * (pos_end[0] - pos_start[0]) + (pos_end[1] - pos_start[1]) * (pos_end[1] - pos_start[1]) + (pos_end[2] - pos_start[2]) * (pos_end[2] - pos_start[2]); }
        float       dist_xy()       const { return sqrt(dist_xy2()); }
        float       dist_xyz()      const { return sqrt(dist_xyz2()); }
        float       dist_e()        const { return fabs(pos_end[3] - pos_start[3]); }

        float       feedrate()      const { return pos_end[4]; }
        float       time()          const { return dist_xyz() / feedrate(); }
        float       time_inv()      const { return feedrate() / dist_xyz(); }
        float       volumetric_correction_avg() const {
        // Orca: 将修正限制在0.05 - 1.00000001以避免零进给率
            float avg_correction = std::max(0.05f,0.5f * (volumetric_extrusion_rate_start + volumetric_extrusion_rate_end) / volumetric_extrusion_rate);
            assert(avg_correction > 0.f);
            assert(avg_correction <= 1.00000001f);
            return avg_correction;
        }
        float       time_corrected()  const { return time() * volumetric_correction_avg(); }

        GCodeLineType type;

        // 我们尝试保持字符串缓冲区一旦分配，就不会一次又一次地重新分配。
        std::vector<char>   raw;
        size_t              raw_length;
        // 如果modified，原始文本必须根据新的挤出率调整，
        // 或者该行可能需要分割为多行。
        bool                modified;

        // X,Y,Z,E,F。仅存储当前活动挤出机的状态。
        float       pos_start[5];
        float       pos_end[5];
        // 是否在G-code行上找到了轴？X,Y,Z,E,F
        bool        pos_provided[5];

        // 活动挤出机的索引。
        size_t      extruder_id;
        // 此段的挤出角色。
        ExtrusionRole extrusion_role;

        // 当前体积挤出率。
        float       volumetric_extrusion_rate;
        // 此段开始时的体积挤出率。
        float       volumetric_extrusion_rate_start;
        // 此段结束时的体积挤出率。
        float       volumetric_extrusion_rate_end;

        // 限制此段的体积挤出率斜率。
        // 如果设为零，则斜率不受限制。
        float       max_volumetric_extrusion_rate_slope_positive;
        float       max_volumetric_extrusion_rate_slope_negative;

        bool        adjustable_flow       = false;

        bool        extrude_set_speed_tag = false;
        bool        extrude_end_tag       = false;
    };

    // 输出缓冲区只会增长。不会一次又一次地重新分配。
    std::vector<char>               output_buffer;
    size_t                          output_buffer_length;
    size_t                          output_buffer_prev_length;

#ifdef PRESSURE_EQUALIZER_DEBUG
    // 用于调试目的。处理的G-code行索引。
    size_t                          line_idx;
#endif

    bool process_line(const char *line, const char *line_end, GCodeLine &buf);
    long advance_segment_beyond_small_gap(long idx_cur_pos);
    void output_gcode_line(size_t line_idx);

    // 从当前circular_buffer_pos向回走，降低进给率以减小挤出率变化的斜率。
    // 然后向前走并调整进给率以减小挤出率变化的斜率。
    void adjust_volumetric_rate(size_t first_line_idx, size_t last_line_idx);

    // 将文本推送到output_buffer的末尾。
    inline void push_to_output(GCodeG1Formatter &formatter);
    inline void push_to_output(const std::string &text, bool add_eol);
    inline void push_to_output(const char *text, size_t len, bool add_eol = true);
    // 将G-code行推送到输出。
    void push_line_to_output(size_t line_idx, float new_feedrate, const char *comment);

public:
    std::queue<LayerResult*> m_layer_results;

    std::vector<GCodeLine> m_gcode_lines;
};

} // namespace Slic3r

#endif /* slic3r_GCode_PressureEqualizer_hpp_ */
