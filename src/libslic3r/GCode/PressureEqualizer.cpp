#include <iostream>
#include <memory.h>
#include <cstring>
#include <cfloat>
#include <algorithm>

#include "../libslic3r.h"
#include "../PrintConfig.hpp"
#include "../LocalesUtils.hpp"
#include "../GCode.hpp"

#include "PressureEqualizer.hpp"
#include "fast_float/fast_float.h"
#include "GCodeWriter.hpp"

namespace Slic3r {

static const std::string EXTRUSION_ROLE_TAG = ";_EXTRUSION_ROLE:";
static const std::string EXTRUDE_END_TAG = ";_EXTRUDE_END";
static const std::string EXTRUDE_SET_SPEED_TAG = ";_EXTRUDE_SET_SPEED";
static const std::string EXTERNAL_PERIMETER_TAG = ";_EXTERNAL_PERIMETER";

// 从最新行向回调整流量多少条G代码行。
// 较大的值会大大影响G代码导出速度，而较小的值可能会
// 影响流量调整传播的距离。
static constexpr int max_look_back_limit = 128;

// 两个连续挤出之间最大的非挤出XY距离（移动移动）（单位mm），在此距离内我们假装
// 它们是一条连续的挤出线。超过此距离我们假设挤出机压力降至0。
// 这之所以存在，是因为在诸如填充线之间经常有小移动，
// 此时一些挤出机压力会保留（因此我们应在这些小移动之间进行均衡）
static constexpr long max_ignored_gap_between_extruding_segments = 3;

PressureEqualizer::PressureEqualizer(const Slic3r::GCodeConfig &config) : m_use_relative_e_distances(config.use_relative_e_distances.value)
{
    // 预分配一些数据，以便output_buffer.data()返回空字符串。
    output_buffer.assign(32, 0);
    output_buffer_length      = 0;
    output_buffer_prev_length = 0;

    m_current_extruder = 0;
    // 清零XYZE轴的位置 + 当前进给
    memset(m_current_pos, 0, sizeof(float) * 5);
    m_current_extrusion_role = ExtrusionRole::erNone;
    // 期望第一个命令填充喷嘴（取消回抽）。
    m_retracted = true;

    m_max_segment_length = 2.f;

    // 计算多个挤出机的丝材横截面积。
    m_filament_crossections.clear();
    for (double r : config.filament_diameter.values) {
        double a = 0.25f * M_PI * r * r;
        m_filament_crossections.push_back(float(a));
    }

    // 0.45mm x 0.2mm挤出在60mm/s XY移动下的体积率：0.45*0.2*60*60=5.4*60 = 324 mm^3/min
    // 0.45mm x 0.2mm挤出在20mm/s XY移动下的体积率：0.45*0.2*20*60=1.8*60 = 108 mm^3/min
    // 体积率斜率，从20mm/s到60mm/s在2秒内变化：(5.4-1.8)*60*60/2=60*60*1.8 = 6480 mm^3/min^2 = 1.8 mm^3/s^2

    if(config.max_volumetric_extrusion_rate_slope.value > 0){
        m_max_volumetric_extrusion_rate_slope_positive = float(config.max_volumetric_extrusion_rate_slope.value) * 60.f * 60.f;
        m_max_volumetric_extrusion_rate_slope_negative = float(config.max_volumetric_extrusion_rate_slope.value) * 60.f * 60.f;
        m_max_segment_length = float(config.max_volumetric_extrusion_rate_slope_segment_length.value);
        m_extrusion_rate_smoothing_external_perimeter_only = bool(config.extrusion_rate_smoothing_external_perimeter_only.value);
    }

    for (ExtrusionRateSlope &extrusion_rate_slope : m_max_volumetric_extrusion_rate_slopes) {
        extrusion_rate_slope.negative = m_max_volumetric_extrusion_rate_slope_negative;
        extrusion_rate_slope.positive = m_max_volumetric_extrusion_rate_slope_positive;
    }

    // 不要调节熨烫前后的压力。
    for (const ExtrusionRole er : {ExtrusionRole::erIroning}) {
        m_max_volumetric_extrusion_rate_slopes[size_t(er)].negative = 0;
        m_max_volumetric_extrusion_rate_slopes[size_t(er)].positive = 0;
    }

    opened_extrude_set_speed_block = false;

#ifdef PRESSURE_EQUALIZER_STATISTIC
    m_stat.reset();
#endif

#ifdef PRESSURE_EQUALIZER_DEBUG
    line_idx = 0;
#endif
}

void PressureEqualizer::process_layer(const std::string &gcode)
{
    if (!gcode.empty()) {
        const char *gcode_begin = gcode.c_str();
        while (*gcode_begin != 0) {
            // 找到行尾。
            const char *gcode_end = gcode_begin;
            // Slic3r总是以Unix样式生成行尾。
            for (; *gcode_end != 0 && *gcode_end != '\n'; ++gcode_end);

            m_gcode_lines.emplace_back();
            if (!this->process_line(gcode_begin, gcode_end, m_gcode_lines.back())) {
                // 该行必须被忘记。它包含注释标记，应从目标g-code中过滤掉。
                m_gcode_lines.pop_back();
            }
            gcode_begin = gcode_end;
            if (*gcode_begin == '\n')
                ++gcode_begin;
        }
        assert(!this->opened_extrude_set_speed_block);
    }

    // 此时，我们将一整个层的gcode行加载到m_gcode_lines中
    // 现在我们将移动和挤出混合分割成连续挤出段并处理它们
    // 我们跳过大的移动，假装小移动是连续挤出段的一部分
    long idx_end_current_extrusion = 0;
    while (idx_end_current_extrusion < m_gcode_lines.size()) {
        // 从当前位置找到下一个挤出段的开始
        const long idx_begin_current_extrusion   = find_if(m_gcode_lines.begin() + idx_end_current_extrusion, m_gcode_lines.end(),
                                                          [](GCodeLine line) { return line.extruding(); }) - m_gcode_lines.begin();
        // （挤出开始idx = 挤出结束idx）这里因为我们从长度为0的挤出开始
        idx_end_current_extrusion = idx_begin_current_extrusion;

        // 内部循环在小的移动移动上扩展挤出段
        while (idx_end_current_extrusion < m_gcode_lines.size()) {
            // 找到当前挤出段的结尾
            const auto just_after_end_extrusion = find_if(m_gcode_lines.begin() + idx_end_current_extrusion, m_gcode_lines.end(),
                                                          [](GCodeLine line) { return !line.extruding(); });
            idx_end_current_extrusion = std::max<long>(0,(just_after_end_extrusion - m_gcode_lines.begin()) - 1);
            const long idx_begin_segment_continuation = advance_segment_beyond_small_gap(idx_end_current_extrusion);
            if (idx_begin_segment_continuation > idx_end_current_extrusion) {
                // 在小间隙上扩展连续线
                idx_end_current_extrusion = idx_begin_segment_continuation;
                continue; // 继续，再次循环以找到挤出段的新结尾
            } else {
                // 到下一个挤出的间隙太大，停止向前看。我们已找到此段的结尾
                break;
            }
        }

        // 现在像压路机一样在段上运行压力均衡器
        // 它在gcode行上逐个向前移动的滑动窗口上操作
        for (int i = idx_begin_current_extrusion; i < idx_end_current_extrusion; ++i) {
            // 向压力均衡器馈送过去的行，回溯到max_look_back_limit（或段开头）
            const auto start_idx = std::max<long>(idx_begin_current_extrusion, i - max_look_back_limit);
            adjust_volumetric_rate(start_idx, i);
        }
        // 当前挤出已全部处理完毕，为下一次循环前进到下一个
        idx_end_current_extrusion++;
    }
}

long PressureEqualizer::advance_segment_beyond_small_gap(const long idx_orig)
{
    // 这应仅在间隙之前的最后挤出线上运行
    assert(m_gcode_lines[idx_orig].extruding());
    double distance_traveled = 0.0;
    // 从间隙开头开始，前进直到找到挤出或间隙太大
    for (auto idx_cur_pos = idx_orig + 1; idx_cur_pos < m_gcode_lines.size(); idx_cur_pos++) {
        // 再次开始挤出！返回段扩展
        if (m_gcode_lines[idx_cur_pos].extruding()) {
            return idx_cur_pos;
        }

        distance_traveled += m_gcode_lines[idx_cur_pos].dist_xy();
        // 间隙太大，不扩展段
        if (distance_traveled > max_ignored_gap_between_extruding_segments) {
            return idx_orig;
        }
    }
    // 循环到层末尾且无法扩展挤出
     return idx_orig;
}

LayerResult PressureEqualizer::process_layer(LayerResult &&input)
{
    const bool   is_first_layer       = m_layer_results.empty();
    const size_t next_layer_first_idx = m_gcode_lines.size();

    if (!input.nop_layer_result) {
        this->process_layer(input.gcode);
        input.gcode.clear(); // GCode已处理，因此不需要存储。
        m_layer_results.emplace(new LayerResult(input));
    }

    if (is_first_layer) // 缓冲前一个输入结果并输出NOP。
        return LayerResult::make_nop_layer_result();

    // 导出前一层。
    LayerResult *prev_layer_result = m_layer_results.front();
    m_layer_results.pop();

    output_buffer_length      = 0;
    output_buffer_prev_length = 0;
    for (size_t line_idx = 0; line_idx < next_layer_first_idx; ++line_idx)
        output_gcode_line(line_idx);
    m_gcode_lines.erase(m_gcode_lines.begin(), m_gcode_lines.begin() + int(next_layer_first_idx));

    if (output_buffer_length > 0)
        prev_layer_result->gcode = std::string(output_buffer.data());

    assert(!input.nop_layer_result || m_layer_results.empty());
    LayerResult out = *prev_layer_result;
    delete prev_layer_result;
    return out;
}

// 是空白吗？
static inline bool is_ws(const char c) { return c == ' ' || c == '\t'; }
// 是行尾吗？将注释视为行尾。
static inline bool is_eol(const char c) { return c == 0 || c == '\r' || c == '\n' || c == ';'; }
// 是空白或行尾吗？
static inline bool is_ws_or_eol(const char c) { return is_ws(c) || is_eol(c); }

// 消耗空白。
static void eatws(const char *&line)
{
    while (is_ws(*line))
        ++ line;
}

// 从行的当前位置解析int。
// 如果成功，则前进line指针。
static inline int parse_int(const char *&line)
{
    char *endptr = nullptr;
    long result = strtol(line, &endptr, 10);
    if (endptr == nullptr || !is_ws_or_eol(*endptr))
        throw Slic3r::InvalidArgument("PressureEqualizer: 解析int时出错");
    line = endptr;
    return int(result);
}

float string_to_float_decimal_point(const char *line, const size_t str_len, size_t* pos)
{
    float out;
    size_t p = fast_float::from_chars(line, line + str_len, out).ptr - line;
    if (pos)
        *pos = p;
    return out;
}

// 从行的当前位置解析int。
// 如果成功，则前进line指针。
static inline float parse_float(const char *&line, const size_t line_length)
{
    size_t endptr = 0;
    auto   result = string_to_float_decimal_point(line, line_length, &endptr);
    if (endptr == 0 || !is_ws_or_eol(*(line + endptr)))
        throw Slic3r::RuntimeError("PressureEqualizer: 解析float时出错");
    line = line + endptr;
    return result;
}

bool PressureEqualizer::process_line(const char *line, const char *line_end, GCodeLine &buf)
{
    const size_t len = line_end - line;
    if (strncmp(line, EXTRUSION_ROLE_TAG.data(), EXTRUSION_ROLE_TAG.length()) == 0) {
        line += EXTRUSION_ROLE_TAG.length();
        int role = atoi(line);
        m_current_extrusion_role = ExtrusionRole(role);
#ifdef PRESSURE_EQUALIZER_DEBUG
        ++line_idx;
#endif
        return false;
    }

    // 设置类型，将行复制到缓冲区。
    buf.type = GCODELINETYPE_OTHER;
    buf.modified = false;
    if (buf.raw.size() < len + 1)
        buf.raw.assign(line, line + len + 1);
    else
        memcpy(buf.raw.data(), line, len);
    buf.raw[len] = 0;
    buf.raw_length = len;

    memcpy(buf.pos_start, m_current_pos, sizeof(float)*5);
    memcpy(buf.pos_end, m_current_pos, sizeof(float)*5);
    memset(buf.pos_provided, 0, 5);

    buf.volumetric_extrusion_rate = 0.f;
    buf.volumetric_extrusion_rate_start = 0.f;
    buf.volumetric_extrusion_rate_end = 0.f;
    buf.max_volumetric_extrusion_rate_slope_positive = 0.f;
    buf.max_volumetric_extrusion_rate_slope_negative = 0.f;
    buf.extrusion_role = m_current_extrusion_role;

    std::string str_line(line, line_end);
    const bool found_extrude_set_speed_tag = boost::contains(str_line, EXTRUDE_SET_SPEED_TAG);
    const bool found_extrude_end_tag = boost::contains(str_line, EXTRUDE_END_TAG);
    assert(!found_extrude_set_speed_tag || !found_extrude_end_tag);

    if (found_extrude_set_speed_tag)
        this->opened_extrude_set_speed_block = true;
    else if (found_extrude_end_tag)
        this->opened_extrude_set_speed_block = false;

    // 解析G-code行，将结果存入buf。
    switch (toupper(*line ++)) {
    case 'G': {
        int gcode = -1;
        try {
            gcode = parse_int(line);
        } catch (Slic3r::InvalidArgument &) {
            // 忽略无效GCode。
            eatws(line);
            break;
        }

        assert(gcode != -1);
        eatws(line);
        switch (gcode) {
        case 0:
        case 1:
        {
            // G0, G1: FFF 3D打印机不区分两者。
            buf.adjustable_flow = this->opened_extrude_set_speed_block;
            buf.extrude_set_speed_tag = found_extrude_set_speed_tag;
            buf.extrude_end_tag = found_extrude_end_tag;
            float new_pos[5];
            memcpy(new_pos, m_current_pos, sizeof(float)*5);
            bool  changed[5] = { false, false, false, false, false };
            while (!is_eol(*line)) {
                const char axis = toupper(*line++);
                int  i = -1;
                switch (axis) {
                case 'X':
                case 'Y':
                case 'Z':
                    i = axis - 'X';
                    break;
                case 'E':
                    i = 3;
                    break;
                case 'F':
                    i = 4;
                    break;
                default:
                    break;
                }
                if (i != -1) {
                    buf.pos_provided[i] = true;
                    new_pos[i] = parse_float(line, line_end - line);
                    if (i == 3 && m_use_relative_e_distances)
                        new_pos[i] += m_current_pos[i];
                    changed[i] = new_pos[i] != m_current_pos[i];
                    eatws(line);
                }
            }
            if (changed[3]) {
                // 挤出、回抽或取消回抽。
                float diff = new_pos[3] - m_current_pos[3];
                if (diff < 0) {
                    buf.type = GCODELINETYPE_RETRACT;
                    m_retracted = true;
                } else if (! changed[0] && ! changed[1] && ! changed[2]) {
                    // assert(m_retracted);
                    buf.type = GCODELINETYPE_UNRETRACT;
                    m_retracted = false;
                } else {
                    assert(changed[0] || changed[1]);
                    // 在XY平面移动。
                    buf.type = GCODELINETYPE_EXTRUDE;
                    // 计算体积挤出率。
                    float diff[4];
                    for (size_t i = 0; i < 4; ++ i)
                        diff[i] = new_pos[i] - m_current_pos[i];
                    // 体积挤出率 = A_filament * F_xyz * L_e / L_xyz [mm^3/min]
                    float len2 = diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2];
                    float rate = m_filament_crossections[m_current_extruder] * new_pos[4] * sqrt((diff[3]*diff[3])/len2);
                    buf.volumetric_extrusion_rate       = rate;
                    buf.volumetric_extrusion_rate_start = rate;
                    buf.volumetric_extrusion_rate_end   = rate;

#ifdef PRESSURE_EQUALIZER_STATISTIC
                    m_stat.update(rate, sqrt(len2));
#endif
#ifdef PRESSURE_EQUALIZER_DEBUG
                    if (rate < 40.f) {
                        printf("极低流量：%f. 行 %d, 长度: %f, 挤出: %f 旧位置: (%f, %f, %f), 新位置: (%f, %f, %f)\n",
                               rate, int(line_idx), sqrt(len2), sqrt((diff[3] * diff[3]) / len2), m_current_pos[0], m_current_pos[1], m_current_pos[2],
                               new_pos[0], new_pos[1], new_pos[2]);
                    }
#endif
                }
            } else if (changed[0] || changed[1] || changed[2]) {
                // 移动但不挤出。
                buf.type = GCODELINETYPE_MOVE;
            }
            memcpy(m_current_pos, new_pos, sizeof(float) * 5);
            break;
        }
        case 92:
        {
            // G92 : 设置位置
            // 设置逻辑坐标位置为新值而不实际移动机器电机。
            // 设置哪些轴？
            while (!is_eol(*line)) {
                const char axis = toupper(*line++);
                switch (axis) {
                case 'X':
                case 'Y':
                case 'Z':
                    m_current_pos[axis - 'X'] = (!is_ws_or_eol(*line)) ? parse_float(line, line_end - line) : 0.f;
                    break;
                case 'E':
                    m_current_pos[3] = (!is_ws_or_eol(*line)) ? parse_float(line, line_end - line) : 0.f;
                    break;
                default:
                    break;
                }
                eatws(line);
            }
            break;
        }
        case 10:
        case 22:
            // 固件回抽。
            buf.type = GCODELINETYPE_RETRACT;
            m_retracted = true;
            break;
        case 11:
        case 23:
            // 固件取消回抽。
            buf.type = GCODELINETYPE_UNRETRACT;
            m_retracted = false;
            break;
        default:
            // 忽略其余部分。
        break;
        }
        break;
    }
    case 'M': {
        eatws(line);
        // 忽略其余M代码。
        break;
    }
    case 'T':
    {
        // 激活挤出机头。
        int new_extruder = -1;
        try {
            new_extruder = parse_int(line);
        } catch (Slic3r::InvalidArgument &) {
            // 忽略以T开头的无效GCode。
            eatws(line);
            break;
        }
        assert(new_extruder != -1);

        if (new_extruder != int(m_current_extruder)) {
            m_current_extruder = new_extruder;
            m_retracted = true;
            buf.type = GCODELINETYPE_TOOL_CHANGE;
        } else {
            buf.type = GCODELINETYPE_NOOP;
        }
        break;
    }
    }

    buf.extruder_id = m_current_extruder;
    memcpy(buf.pos_end, m_current_pos, sizeof(float)*5);
#ifdef PRESSURE_EQUALIZER_DEBUG
    ++line_idx;
#endif
    return true;
}

void PressureEqualizer::output_gcode_line(const size_t line_idx)
{
    GCodeLine &line = m_gcode_lines[line_idx];
    if (!line.modified) {
        push_to_output(line.raw.data(), line.raw_length, true);
        return;
    }

    // 该行已被修改。
    // 找到注释。
    const char *comment = line.raw.data();
    while (*comment != ';' && *comment != 0) ++comment;
    if (*comment != ';')
        comment = nullptr;

    // 获取gcode行长度
    float l = line.dist_xyz();
    // 此行可被分割成的段数
    auto nSegments = size_t(ceil(l / m_max_segment_length));

    // Orca:
    // 计算该行起点和终点之间体积挤出率的绝对差值。
    // 将其量化为1mm3/min（0.016mm3/sec）。
    int delta_volumetric_rate = std::round(fabs(line.volumetric_extrusion_rate_end - line.volumetric_extrusion_rate_start));

    // 以降低的挤出率输出该行。
    // Orca:
    // 首先，检查体积挤出率的变化是否微不足道（小于10mm3/min -> 0.16mm3/sec（0.25mm喷嘴的5mm/sec速度））。
    // 或者该行长度是否等于最小段长度。
    // 如果是，则作为单个挤出输出该行，即不分割成段。
    if ( nSegments == 1 || delta_volumetric_rate < 10) {
        push_line_to_output(line_idx, line.feedrate() * line.volumetric_correction_avg(), comment);
    } else // 需要将该行分割成段并应用挤出率平滑
    {
        bool accelerating = line.volumetric_extrusion_rate_start < line.volumetric_extrusion_rate_end;
        // 更新初始和最终进给率值。
        line.pos_start[4] = line.volumetric_extrusion_rate_start * line.pos_end[4] / line.volumetric_extrusion_rate;
        line.pos_end  [4] = line.volumetric_extrusion_rate_end   * line.pos_end[4] / line.volumetric_extrusion_rate;
        float feed_avg = 0.5f * (line.pos_start[4] + line.pos_end[4]);
        // 此段的限制体积挤出率斜率。
        float max_volumetric_extrusion_rate_slope = accelerating ? line.max_volumetric_extrusion_rate_slope_positive :
                                                                   line.max_volumetric_extrusion_rate_slope_negative;
        // 段的修正总时间，用于可能降低的体积进给率，
        // 如果加速/减速跨越整个段。
        float t_total = line.dist_xyz() / feed_avg;
        // 如果以最大体积挤出率斜率加速/减速，段的加速/减速部分的时间。
        float t_acc    = 0.5f * (line.volumetric_extrusion_rate_start + line.volumetric_extrusion_rate_end) / max_volumetric_extrusion_rate_slope;
        float l_acc    = l;
        float l_steady = 0.f;
        if (t_acc < t_total) {
            // 如果部分段不受速度限制，可以达到更高的打印速度。
            l_acc    = t_acc * feed_avg;
            l_steady = l - l_acc;
            if (l_steady < 0.5f * m_max_segment_length) {
                l_acc    = l;
                l_steady = 0.f;
            } else
                nSegments = size_t(ceil(l_acc / m_max_segment_length));
        }
        float pos_start[5];
        float pos_end[5];
        float pos_end2[4];
        memcpy(pos_start, line.pos_start, sizeof(float) * 5);
        memcpy(pos_end, line.pos_end, sizeof(float) * 5);
        if (l_steady > 0.f) {
            // 将有一个稳定的进给段输出。
            if (accelerating) {
                // 准备最终的稳定进给段。
                memcpy(pos_end2, pos_end, sizeof(float)*4);
                float t = l_acc / l;
                for (int i = 0; i < 4; ++ i) {
                    pos_end[i] = pos_start[i] + (pos_end[i] - pos_start[i]) * t;
                    line.pos_provided[i] = true;
                }
            } else {
                // 输出稳定的进给段。
                float t = l_steady / l;
                for (int i = 0; i < 4; ++ i) {
                    line.pos_end[i] = pos_start[i] + (pos_end[i] - pos_start[i]) * t;
                    line.pos_provided[i] = true;
                }
                push_line_to_output(line_idx, pos_start[4], comment);
                comment = nullptr;

                float new_pos_start_feedrate = pos_start[4];

                memcpy(line.pos_start, line.pos_end, sizeof(float)*5);
                memcpy(pos_start, line.pos_end, sizeof(float)*5);

                line.pos_start[4] = new_pos_start_feedrate;
                pos_start[4] = new_pos_start_feedrate;
            }
        }
        // 将段分割成片段。
        for (size_t i = 1; i < nSegments; ++ i) {
            float t = float(i) / float(nSegments);
            for (size_t j = 0; j < 4; ++ j) {
                line.pos_end[j] = pos_start[j] + (pos_end[j] - pos_start[j]) * t;
                line.pos_provided[j] = true;
            }
            // 在段中心插值进给率。
            push_line_to_output(line_idx, pos_start[4] + (pos_end[4] - pos_start[4]) * (float(i) - 0.5f) / float(nSegments), comment);
            comment = nullptr;
            memcpy(line.pos_start, line.pos_end, sizeof(float)*5);
        }
        if (l_steady > 0.f && accelerating) {
            for (int i = 0; i < 4; ++ i) {
                line.pos_end[i] = pos_end2[i];
                line.pos_provided[i] = true;
            }
            push_line_to_output(line_idx, pos_end[4], comment);
        } else {
            for (int i = 0; i < 4; ++ i) {
                line.pos_end[i] = pos_end[i];
                line.pos_provided[i] = true;
            }
            push_line_to_output(line_idx, pos_end[4], comment);
        }
    }
}

void PressureEqualizer::adjust_volumetric_rate(const size_t fist_line_idx, const size_t last_line_idx)
{
    // 如果没有什么gcode要调整，就不必调整体积率
    if (last_line_idx-fist_line_idx < 2)
        return;

    size_t       line_idx      = last_line_idx;
    if (line_idx == fist_line_idx || !m_gcode_lines[line_idx].extruding())
        // 无事可做，最后一个移动不是挤出。
        return;
    std::array<float, size_t(ExtrusionRole::erCount)> feedrate_per_extrusion_role{};
    feedrate_per_extrusion_role.fill(std::numeric_limits<float>::max());
    feedrate_per_extrusion_role[int(m_gcode_lines[line_idx].extrusion_role)] = m_gcode_lines[line_idx].volumetric_extrusion_rate_start;

    while (line_idx != fist_line_idx) {
        size_t idx_prev = line_idx - 1;
        for (; !m_gcode_lines[idx_prev].extruding() && idx_prev != fist_line_idx; --idx_prev);
        if (!m_gcode_lines[idx_prev].extruding())
            break;
        // 不在熨烫前减速。
        if (m_gcode_lines[line_idx].extrusion_role == ExtrusionRole::erIroning) {            line_idx = idx_prev;
            continue;
        }
        // 紧随段开始处的体积挤出率。
        float rate_succ = m_gcode_lines[line_idx].volumetric_extrusion_rate_start;
        // idx_prev和idx之间的挤出率梯度是多少？
        line_idx        = idx_prev;
        GCodeLine &line = m_gcode_lines[line_idx];

        for (size_t iRole = 1; iRole < size_t(ExtrusionRole::erCount); ++ iRole) {
            const float &rate_slope = m_max_volumetric_extrusion_rate_slopes[iRole].negative;
            if (rate_slope == 0 || feedrate_per_extrusion_role[iRole] == std::numeric_limits<float>::max())
                continue; // 负速率不限或ExtrusionRole iRole的速率不限。

            float rate_end = feedrate_per_extrusion_role[iRole];
            if (iRole == size_t(line.extrusion_role) && rate_succ < rate_end)
                // 受后续体积流量限制。
                rate_end = rate_succ;

            // 不改变这些挤出类型的流量
            // Orca: 如果用户选择了此选项，将ERS限制为外部周长和悬垂
            if (!line.adjustable_flow || line.extrusion_role == ExtrusionRole::erBridgeInfill || line.extrusion_role == ExtrusionRole::erIroning ||
                (m_extrusion_rate_smoothing_external_perimeter_only && line.extrusion_role != ExtrusionRole::erOverhangPerimeter && line.extrusion_role != ExtrusionRole::erExternalPerimeter)) {
                rate_end = line.volumetric_extrusion_rate_end;
            } else if (line.volumetric_extrusion_rate_end > rate_end) {
                line.volumetric_extrusion_rate_end = rate_end;
                line.max_volumetric_extrusion_rate_slope_negative = rate_slope;
                line.modified = true;
            } else if (iRole == size_t(line.extrusion_role)) {
                rate_end = line.volumetric_extrusion_rate_end;
            } else {
                // 使用原始的"浮动"挤出率作为限制器的起点。
            }

            if (line.adjustable_flow) {
                float rate_start = sqrt(rate_end * rate_end + 2 * line.volumetric_extrusion_rate * line.dist_xyz() * rate_slope / line.feedrate());
                if (rate_start < line.volumetric_extrusion_rate_start) {
                    // 由于将在未来挤出的ExtrusionType iRole的段，限制此段开始处的体积挤出率。
                    line.volumetric_extrusion_rate_start = rate_start;
                    line.max_volumetric_extrusion_rate_slope_negative = rate_slope;
                    line.modified = true;
                }
            }
//            feedrate_per_extrusion_role[iRole] = (iRole == line.extrusion_role) ? line.volumetric_extrusion_rate_start : rate_start;
            // 不存储熨烫的进给率
            if (line.extrusion_role != ExtrusionRole::erIroning)
                feedrate_per_extrusion_role[iRole] = line.volumetric_extrusion_rate_start;
        }
    }

    feedrate_per_extrusion_role.fill(std::numeric_limits<float>::max());
    feedrate_per_extrusion_role[size_t(m_gcode_lines[line_idx].extrusion_role)] = m_gcode_lines[line_idx].volumetric_extrusion_rate_end;

    assert(m_gcode_lines[line_idx].extruding());
    while (line_idx != last_line_idx) {
        size_t idx_next = line_idx + 1;
        for (; !m_gcode_lines[idx_next].extruding() && idx_next != last_line_idx; ++idx_next);
        if (!m_gcode_lines[idx_next].extruding())
            break;
        // 不在熨烫后加速。
        if (m_gcode_lines[line_idx].extrusion_role == ExtrusionRole::erIroning) {
            line_idx = idx_next;
            continue;
        }
        float rate_prec = m_gcode_lines[line_idx].volumetric_extrusion_rate_end;
        // idx_prev和idx之间的挤出率梯度是多少？
        line_idx = idx_next;
        GCodeLine &line = m_gcode_lines[line_idx];

        for (size_t iRole = 1; iRole < size_t(ExtrusionRole::erCount); ++ iRole) {
            const float &rate_slope = m_max_volumetric_extrusion_rate_slopes[iRole].positive;
            if (rate_slope == 0 || feedrate_per_extrusion_role[iRole] == std::numeric_limits<float>::max())
                continue; // 正速率不限或ExtrusionRole iRole的速率不限。

            float rate_start = feedrate_per_extrusion_role[iRole];
            // 不改变这些挤出类型的流量
            // Orca: 如果用户选择了此选项，将ERS限制为外部周长和悬垂
            if (!line.adjustable_flow || line.extrusion_role == ExtrusionRole::erBridgeInfill || line.extrusion_role == ExtrusionRole::erIroning ||
                (m_extrusion_rate_smoothing_external_perimeter_only && line.extrusion_role != ExtrusionRole::erOverhangPerimeter && line.extrusion_role != ExtrusionRole::erExternalPerimeter)) {
                rate_start = line.volumetric_extrusion_rate_start;
            } else if (iRole == size_t(line.extrusion_role) && rate_prec < rate_start)
                rate_start = rate_prec;

            if (line.volumetric_extrusion_rate_start > rate_start) {
                line.volumetric_extrusion_rate_start = rate_start;
                line.max_volumetric_extrusion_rate_slope_positive = rate_slope;
                line.modified = true;
            } else if (iRole == size_t(line.extrusion_role)) {
                rate_start = line.volumetric_extrusion_rate_start;
            } else {
                // 使用原始的"浮动"挤出率作为限制器的起点。
            }

            if (line.adjustable_flow) {
                float rate_end = sqrt(rate_start * rate_start + 2 * line.volumetric_extrusion_rate * line.dist_xyz() * rate_slope / line.feedrate());
                if (rate_end < line.volumetric_extrusion_rate_end) {
                    // 由于之前挤出的ExtrusionType iRole的段，限制此段开始处的体积挤出率。
                    line.volumetric_extrusion_rate_end                = rate_end;
                    line.max_volumetric_extrusion_rate_slope_positive = rate_slope;
                    line.modified                                     = true;
                }
            }
//            feedrate_per_extrusion_role[iRole] = (iRole == line.extrusion_role) ? line.volumetric_extrusion_rate_end : rate_end;
            // 不存储熨烫的进给率
            if (line.extrusion_role != ExtrusionRole::erIroning)
                feedrate_per_extrusion_role[iRole] = line.volumetric_extrusion_rate_end;
        }
    }
}

inline void PressureEqualizer::push_to_output(GCodeG1Formatter &formatter)
{
    return this->push_to_output(formatter.string(), false);
}

inline void PressureEqualizer::push_to_output(const std::string &text, bool add_eol)
{
    return this->push_to_output(text.data(), text.size(), add_eol);
}

inline void PressureEqualizer::push_to_output(const char *text, const size_t len, bool add_eol)
{
    // 输出缓冲区内容的新长度。
    size_t len_new = output_buffer_length + len + 1;
    if (add_eol)
        ++len_new;

    // 将输出缓冲区大小调整为高于所需内存的2的幂。
    if (output_buffer.size() < len_new) {
        size_t v = len_new;
        // 计算32位v的下一个最高2的幂
        // http://graphics.stanford.edu/~seander/bithacks.html
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        output_buffer.resize(v);
    }

    // 将文本复制到输出。
    if (len != 0) {
        memcpy(output_buffer.data() + output_buffer_length, text, len);
        this->output_buffer_prev_length = this->output_buffer_length;
        output_buffer_length += len;
    }
    if (add_eol)
        output_buffer[output_buffer_length++] = '\n';
    output_buffer[output_buffer_length] = 0;
}

inline bool is_just_line_with_extrude_set_speed_tag(const std::string &line)
{
    if (line.empty() && !boost::starts_with(line, "G1 ") && !boost::ends_with(line, EXTRUDE_SET_SPEED_TAG))
        return false;

    const char       *p_line   = line.data() + 3;
    const char *const line_end = line.data() + line.length() - 1;
    while (!is_eol(*p_line)) {
        if (toupper(*p_line++) == 'F')
            break;
        else
            return false;
    }
    parse_float(p_line, line_end - p_line);
    eatws(p_line);
    p_line += EXTRUDE_SET_SPEED_TAG.length();
    return p_line <= line_end && is_eol(*p_line);
}

void PressureEqualizer::push_line_to_output(const size_t line_idx, float new_feedrate, const char *comment)
{
    // Orca: 检查，1 mm/s是最小进给率。
    if (new_feedrate < 60)
        new_feedrate = 60;
    // 将速度变化量化为最小1mm/sec，以减少微小速度变化的gcode体积。
    new_feedrate = std::round(new_feedrate / 60.0) * 60.0;
    const GCodeLine &line = m_gcode_lines[line_idx];
    if (line_idx > 0 && output_buffer_length > 0) {
        const std::string prev_line_str = std::string(output_buffer.begin() + int(this->output_buffer_prev_length),
                                                      output_buffer.begin() + int(this->output_buffer_length) + 1);
        if (is_just_line_with_extrude_set_speed_tag(prev_line_str))
            this->output_buffer_length = this->output_buffer_prev_length; // 删除最后一行，因为它只为空的gcode块设置速度，所以无用。
        else
            push_to_output(EXTRUDE_END_TAG.data(), EXTRUDE_END_TAG.length(), true);
    } else
        push_to_output(EXTRUDE_END_TAG.data(), EXTRUDE_END_TAG.length(), true);

    GCodeG1Formatter feedrate_formatter;
    feedrate_formatter.emit_f(new_feedrate);
    feedrate_formatter.emit_string(std::string(EXTRUDE_SET_SPEED_TAG.data(), EXTRUDE_SET_SPEED_TAG.length()));
    if (line.extrusion_role == ExtrusionRole::erExternalPerimeter)
        feedrate_formatter.emit_string(std::string(EXTERNAL_PERIMETER_TAG.data(), EXTERNAL_PERIMETER_TAG.length()));
    push_to_output(feedrate_formatter);

    GCodeG1Formatter extrusion_formatter;
    for (size_t axis_idx = 0; axis_idx < 3; ++axis_idx)
        if (line.pos_provided[axis_idx])
            extrusion_formatter.emit_axis(char('X' + axis_idx), line.pos_end[axis_idx], GCodeFormatter::XYZF_EXPORT_DIGITS);
    extrusion_formatter.emit_axis('E', m_use_relative_e_distances ? (line.pos_end[3] - line.pos_start[3]) : line.pos_end[3], GCodeFormatter::E_EXPORT_DIGITS);

    if (comment != nullptr)
        extrusion_formatter.emit_string(std::string(comment));

    push_to_output(extrusion_formatter);
}

} // namespace Slic3r
