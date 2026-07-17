#include "../GCode.hpp"
#include "CoolingBuffer.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <float.h>
#include <system_error>
#include <unordered_map>

#if 0
    #define DEBUG
    #define _DEBUG
    #undef NDEBUG
#endif

#include <assert.h>

namespace Slic3r {

CoolingBuffer::CoolingBuffer(GCode &gcodegen) : m_config(gcodegen.config()), m_toolchange_prefix(gcodegen.writer().toolchange_prefix()), m_current_extruder(0)
{
    this->reset(gcodegen.writer().get_position());

    const std::vector<Extruder> &extruders = gcodegen.writer().extruders();
    m_extruder_ids.reserve(extruders.size());
    for (const Extruder &ex : extruders) {
        m_num_extruders = std::max(ex.id() + 1, m_num_extruders);
        m_extruder_ids.emplace_back(ex.id());
    }
}

void CoolingBuffer::reset(const Vec3d &position)
{
    // BBS: 添加I和J轴以存储圆弧中心
    m_current_pos.assign(7, 0.f);
    m_current_pos[0] = float(position.x());
    m_current_pos[1] = float(position.y());
    m_current_pos[2] = float(position.z());
    m_current_pos[4] = float(m_config.travel_speed.value);
    m_fan_speed = -1;
    m_additional_fan_speed = -1;
    m_current_fan_speed = -1;
}

struct CoolingLine
{
    enum Type {
        TYPE_SET_TOOL           = 1 << 0,
        TYPE_EXTRUDE_END        = 1 << 1,
        TYPE_OVERHANG_FAN_START = 1 << 2,
        TYPE_OVERHANG_FAN_END   = 1 << 3,
        TYPE_G0                 = 1 << 4,
        TYPE_G1                 = 1 << 5,
        TYPE_ADJUSTABLE         = 1 << 6,
        TYPE_EXTERNAL_PERIMETER = 1 << 7,
        // 此行设置进给率。
        TYPE_HAS_F              = 1 << 8,
        TYPE_WIPE               = 1 << 9,
        TYPE_G4                 = 1 << 10,
        TYPE_G92                = 1 << 11,
        //BBS: 添加G2 G3类型
        TYPE_G2                 = 1 << 12,
        TYPE_G3                 = 1 << 13,
        TYPE_FORCE_RESUME_FAN   = 1 << 14,
        TYPE_SUPPORT_INTERFACE_FAN_START     = 1 << 15,
        TYPE_SUPPORT_INTERFACE_FAN_END       = 1 << 16,
        // ORCA: 添加对单独内部桥接风扇速度控制的支持
        TYPE_INTERNAL_BRIDGE_FAN_START = 1 << 17,
        TYPE_INTERNAL_BRIDGE_FAN_END   = 1 << 18,
        // ORCA: 添加对熨烫风扇速度控制的支持
        TYPE_IRONING_FAN_START         = 1 << 19,
        TYPE_IRONING_FAN_END           = 1 << 20,
    };

    CoolingLine(unsigned int type, size_t  line_start, size_t  line_end) :
        type(type), line_start(line_start), line_end(line_end),
        length(0.f), feedrate(0.f), time(0.f), time_max(0.f), slowdown(false) {}

    bool adjustable(bool slowdown_external_perimeters) const {
        return (this->type & TYPE_ADJUSTABLE) &&
               (! (this->type & TYPE_EXTERNAL_PERIMETER) || slowdown_external_perimeters) &&
               this->time < this->time_max;
    }

    bool adjustable() const {
        return (this->type & TYPE_ADJUSTABLE) && this->time < this->time_max;
    }

    size_t  type;
    // 此行在G-code片段中的起始位置。
    size_t  line_start;
    // 此行在G-code片段中的结束位置。
    size_t  line_end;
    // 此段的XY欧几里得长度。
    float   length;
    // 当前进给率，可能已调整。
    float   feedrate;
    // 此段的当前持续时间。
    float   time;
    // 此段的最大持续时间。
    float   time_max;
    // 如果标记了"slowdown"标志，则表示该行已被减速。
    bool    slowdown;
};

// 计算每个挤出机所需的时间拉伸。
struct PerExtruderAdjustments
{
    // 计算此挤出机的总经过时间，已调整减速。
    float elapsed_time_total() const {
        float time_total = 0.f;
        for (const CoolingLine &line : lines)
            time_total += line.time;
        return time_total;
    }
    // 计算减速时减速到当前材料定义的最小挤出进给率后的总经过时间。
    float maximum_time_after_slowdown(bool slowdown_external_perimeters) const {
        float time_total = 0.f;
        for (const CoolingLine &line : lines)
            if (line.adjustable(slowdown_external_perimeters)) {
                if (line.time_max == FLT_MAX)
                    return FLT_MAX;
                else
                    time_total += line.time_max;
            } else
                time_total += line.time;
        return time_total;
    }
    // 计算总时间的可调整部分。
    float adjustable_time(bool slowdown_external_perimeters) const {
        float time_total = 0.f;
        for (const CoolingLine &line : lines)
            if (line.adjustable(slowdown_external_perimeters))
                time_total += line.time;
        return time_total;
    }
    // 计算总时间的不可调整部分。
    float non_adjustable_time(bool slowdown_external_perimeters) const {
        float time_total = 0.f;
        for (const CoolingLine &line : lines)
            if (! line.adjustable(slowdown_external_perimeters))
                time_total += line.time;
        return time_total;
    }
    // 将可调整挤出减速到当前挤出机材料允许的最小进给率。
    // 由比例和非比例减速使用。
    float slowdown_to_minimum_feedrate(bool slowdown_external_perimeters) {
        float time_total = 0.f;
        for (CoolingLine &line : lines) {
            if (line.adjustable(slowdown_external_perimeters)) {
                assert(line.time_max >= 0.f && line.time_max < FLT_MAX);
                line.slowdown = true;
                line.time     = line.time_max;
                line.feedrate = line.length / line.time;
            }
            time_total += line.time;
        }
        this->time_total = time_total;
        return time_total;
    }
    // 按因子比例减速每个可调整G-code行。
    // 由比例减速使用。
    float slow_down_proportional(float factor, bool slowdown_external_perimeters) {
        assert(factor >= 1.f);
        float time_total = 0.f;
        for (CoolingLine &line : lines) {
            if (line.adjustable(slowdown_external_perimeters)) {
                line.slowdown = true;
                line.time     = std::min(line.time_max, line.time * factor);
                line.feedrate = line.length / line.time;
            }
            time_total += line.time;
        }
        this->time_total = time_total;
        return time_total;
    }

    // 对行排序，可调整的在前，较高进给率的在前。
    // 由非比例减速使用。
    void sort_lines_by_decreasing_feedrate() {
        std::sort(lines.begin(), lines.end(), [](const CoolingLine &l1, const CoolingLine &l2) {
            bool adj1 = l1.adjustable();
            bool adj2 = l2.adjustable();
            return (adj1 == adj2) ? l1.feedrate > l2.feedrate : adj1;
        });
        for (n_lines_adjustable = 0;
            n_lines_adjustable < lines.size() && this->lines[n_lines_adjustable].adjustable();
            ++ n_lines_adjustable);
        time_non_adjustable = 0.f;
        for (size_t i = n_lines_adjustable; i < lines.size(); ++ i)
            time_non_adjustable += lines[i].time;
    }

    // 计算减速到最小进给率时的最大时间拉伸。
    // 允许为此挤出机的材料减速到最小进给率。
    // 由非比例减速使用。
    float time_stretch_when_slowing_down_to_feedrate(float min_feedrate) const {
        float time_stretch = 0.f;
        assert(this->slow_down_min_speed < min_feedrate + EPSILON);
        for (size_t i = 0; i < n_lines_adjustable; ++ i) {
            const CoolingLine &line = lines[i];
            if (line.feedrate > min_feedrate)
                time_stretch += line.time * (line.feedrate / min_feedrate - 1.f);
        }
        return time_stretch;
    }

    // 将所有可调整行减速到最小进给率。
    // 允许为此挤出机的材料减速到最小进给率。
    // 由非比例减速使用。
    void slow_down_to_feedrate(float min_feedrate) {
        assert(this->slow_down_min_speed < min_feedrate + EPSILON);
        float time_total = 0.f;
        for (size_t i = 0; i < n_lines_adjustable; ++ i) {
            CoolingLine &line = lines[i];
            if (line.feedrate > min_feedrate) {
                line.time *= std::max(1.f, line.feedrate / min_feedrate);
                line.feedrate = min_feedrate;
                line.slowdown = true;
            }
            time_total += line.time;
        }
        this->time_total = time_total;
    }

    // 挤出机，将为其调整G-code。
    unsigned int                extruder_id         = 0;
    // 是否为此挤出机的材料启用了冷却减速逻辑？
    bool                        cooling_slow_down_enabled = false;
    // 如果总层时间低于slow_down_layer_time，将打印减速到slow_down_min_speed。
    float                       slow_down_layer_time = 0.f;
    // 此挤出机允许的最小打印速度。
    float                       slow_down_min_speed     = 0.f;

    bool                        dont_slow_down_outer_wall = false;


    // 解析的行。
    std::vector<CoolingLine>    lines;
    // 以下两个值由sort_lines_by_decreasing_feedrate()设置：
    // 可调整行的数量，位于行开始处。
    size_t                      n_lines_adjustable  = 0;
    // 从n_lines_adjustable开始行的不可调整时间。
    float                       time_non_adjustable = 0;
    // 此挤出机的当前总时间。
    float                       time_total          = 0;
    // 应用最大减速时此挤出机的最大时间。
    float                       time_maximum        = 0;

    // 处理减速的临时变量。两个阈值从0到n_lines_adjustable。
    size_t                      idx_line_begin      = 0;
    size_t                      idx_line_end        = 0;
};

// 当对速度快于min_feedrate的段减速time_stretch时计算新进给率。
// 由非比例减速使用。
float new_feedrate_to_reach_time_stretch(
    std::vector<PerExtruderAdjustments*>::const_iterator it_begin, std::vector<PerExtruderAdjustments*>::const_iterator it_end,
    float min_feedrate, float time_stretch, size_t max_iter = 20)
{
    float new_feedrate = min_feedrate;
    for (size_t iter = 0; iter < max_iter; ++ iter) {
        double nomin = 0;
        double denom = time_stretch;
        for (auto it = it_begin; it != it_end; ++ it) {
            assert((*it)->slow_down_min_speed < min_feedrate + EPSILON);
            for (size_t i = 0; i < (*it)->n_lines_adjustable; ++i) {
                const CoolingLine &line = (*it)->lines[i];
                if (line.feedrate > min_feedrate) {
                    nomin += (double)line.time * (double)line.feedrate;
                    denom += (double)line.time;
                }
            }
        }
        assert(denom > 0);
        if (denom < 0)
            return min_feedrate;
        new_feedrate = (float)(nomin / denom);
        assert(new_feedrate > min_feedrate - EPSILON);
        if (new_feedrate < min_feedrate + EPSILON)
            goto finished;
        for (auto it = it_begin; it != it_end; ++ it)
            for (size_t i = 0; i < (*it)->n_lines_adjustable; ++i) {
                const CoolingLine &line = (*it)->lines[i];
                if (line.feedrate > min_feedrate && line.feedrate < new_feedrate)
                    // 在nomin/denom计算中考虑的一些线段现在比new_feedrate慢，
                    // 这使得new_feedrate低于应有的值。
                    // 使用新的min_feedrate限制重新运行计算，以便当前进给率低于new_feedrate的段
                    // 不被考虑。
                    goto not_finished_yet;
            }
        goto finished;
not_finished_yet:
        min_feedrate = new_feedrate;
    }
    // 未能找到time_stretch的新进给率。

finished:
    // 测试是否达到了time_stretch。
#ifndef NDEBUG
    {
        float time_stretch_final = 0.f;
        for (auto it = it_begin; it != it_end; ++ it)
            time_stretch_final += (*it)->time_stretch_when_slowing_down_to_feedrate(new_feedrate);
        assert(std::abs(time_stretch - time_stretch_final) < EPSILON);
    }
#endif /* NDEBUG */

    return new_feedrate;
}

std::string CoolingBuffer::process_layer(std::string &&gcode, size_t layer_id, bool flush)
{
    // 缓存输入的G-code。
    if (m_gcode.empty())
        m_gcode = std::move(gcode);
    else
        m_gcode += gcode;

    std::string out;
    if (flush) {
        // 这是对象层或最后一个打印层。计算收集的支撑层
        // 和一个对象层上的冷却。
        std::vector<PerExtruderAdjustments> per_extruder_adjustments = this->parse_layer_gcode(m_gcode, m_current_pos);
        float layer_time_stretched = this->calculate_layer_slowdown(per_extruder_adjustments);
        out = this->apply_layer_cooldown(m_gcode, layer_id, layer_time_stretched, per_extruder_adjustments);
        m_gcode.clear();
    }
    return out;
}

// 解析层G-code中可能调整的移动。
// 返回按挤出机分组的解析行列表。
std::vector<PerExtruderAdjustments> CoolingBuffer::parse_layer_gcode(const std::string &gcode, std::vector<float> &current_pos) const
{
    std::vector<PerExtruderAdjustments> per_extruder_adjustments(m_extruder_ids.size());
    std::vector<size_t>                 map_extruder_to_per_extruder_adjustment(m_num_extruders, 0);
    for (size_t i = 0; i < m_extruder_ids.size(); ++ i) {
        PerExtruderAdjustments &adj         = per_extruder_adjustments[i];
        unsigned int            extruder_id = m_extruder_ids[i];
        adj.extruder_id               = extruder_id;
        adj.cooling_slow_down_enabled = m_config.slow_down_for_layer_cooling.get_at(extruder_id);
        adj.slow_down_layer_time = float(m_config.slow_down_layer_time.get_at(extruder_id));
        adj.slow_down_min_speed           = float(m_config.slow_down_min_speed.get_at(extruder_id));
        // ORCA: 支持按丝材（挤出机）启用不减速外壁功能
        adj.dont_slow_down_outer_wall   = m_config.dont_slow_down_outer_wall.get_at(extruder_id);
        map_extruder_to_per_extruder_adjustment[extruder_id] = i;
    }

    unsigned int      current_extruder  = m_current_extruder;
    PerExtruderAdjustments *adjustment  = &per_extruder_adjustments[map_extruder_to_per_extruder_adjustment[current_extruder]];
    const char       *line_start = gcode.c_str();
    const char       *line_end   = line_start;
    // 当前调整中现有CoolingLine的索引，该行保存一系列挤出移动的进给率设置命令。
    size_t            active_speed_modifier = size_t(-1);

    // Orca: 此层是否已有第一次挤出。
    // 第一次挤出前的任何其他移动的时间将从层时间中排除。
    bool layer_had_extrusion = false;

    for (; *line_start != 0; line_start = line_end)
    {
        while (*line_end != '\n' && *line_end != 0)
            ++ line_end;
        // sline不包含尾随的'\n'。
        std::string sline(line_start, line_end);
        // CoolingLine将包含尾随的'\n'。
        if (*line_end == '\n')
            ++ line_end;
        CoolingLine line(0, line_start - gcode.c_str(), line_end - gcode.c_str());
        if (boost::starts_with(sline, "G0 "))
            line.type = CoolingLine::TYPE_G0;
        else if (boost::starts_with(sline, "G1 "))
            line.type = CoolingLine::TYPE_G1;
        else if (boost::starts_with(sline, "G92 "))
            line.type = CoolingLine::TYPE_G92;
        else if (boost::starts_with(sline, "G2 "))
            line.type = CoolingLine::TYPE_G2;
        else if (boost::starts_with(sline, "G3 "))
            line.type = CoolingLine::TYPE_G3;
        if (line.type) {
            // G0、G1或G92
            // 解析G-code行。
            std::vector<float> new_pos(current_pos);
            const char *c = sline.data() + 3;
            for (;;) {
                // 跳过空白。
                for (; *c == ' ' || *c == '\t'; ++ c);
                if (*c == 0 || *c == ';')
                    break;

                assert(is_decimal_separator_point()); // for atof
                //BBS: 解析轴。
                size_t axis = (*c >= 'X' && *c <= 'Z') ? (*c - 'X') :
                              (*c == 'E') ? 3 : (*c == 'F') ? 4 :
                              (*c == 'I') ? 5 : (*c == 'J') ? 6 : size_t(-1);
                if (axis != size_t(-1)) {
                    new_pos[axis] = float(atof(++c));
                    if (axis == 4) {
                        // 将mm/min转换为mm/sec。
                        new_pos[4] /= 60.f;
                        if ((line.type & CoolingLine::TYPE_G92) == 0)
                            // 这是G0或G1行，它设置进给率。此标记用于减少重复的F调用。
                            line.type |= CoolingLine::TYPE_HAS_F;
                    } else if (axis == 5 || axis == 6) {
                        // BBS: 获取圆弧中心位置
                        new_pos[axis] += current_pos[axis - 5];
                    }
                }
                // 跳过此字。
                for (; *c != ' ' && *c != '\t' && *c != 0; ++ c);
            }
            bool external_perimeter = boost::contains(sline, ";_EXTERNAL_PERIMETER");
            bool wipe               = boost::contains(sline, ";_WIPE");
            if (external_perimeter)
                line.type |= CoolingLine::TYPE_EXTERNAL_PERIMETER;
            if (wipe)
                line.type |= CoolingLine::TYPE_WIPE;

            // Orca: 仅减速从第一次挤出开始的移动
            if (boost::contains(sline, ";_EXTRUDE_SET_SPEED"))
                layer_had_extrusion = true;

            // ORCA: 层时间功能不减速外壁
            // 使用adjustment指针确保使用当前挤出机（丝材）的值。
            bool adjust_external = true;
            if(adjustment->dont_slow_down_outer_wall && external_perimeter) adjust_external = false;

            // ORCA: 层时间功能不减速外壁的工作原理是不将外壁标记为可调整，
            // 因此减速算法会忽略它。
            if (boost::contains(sline, ";_EXTRUDE_SET_SPEED") && ! wipe && adjust_external) {
                line.type |= CoolingLine::TYPE_ADJUSTABLE;
                active_speed_modifier = adjustment->lines.size();
            }
            if ((line.type & CoolingLine::TYPE_G92) == 0) {
                //BBS: G0、G1、G2、G3。计算持续时间。
                if (m_config.use_relative_e_distances.value)
                    // 重置挤出机累加器。
                    current_pos[3] = 0.f;
                float dif[4];
                for (size_t i = 0; i < 4; ++ i)
                    dif[i] = new_pos[i] - current_pos[i];
                float dxy2 = 0;
                //BBS: 支持计算弧长
                if (line.type & CoolingLine::TYPE_G2 || line.type & CoolingLine::TYPE_G3) {
                    Vec3f start(current_pos[0], current_pos[1], 0);
                    Vec3f end(new_pos[0], new_pos[1], 0);
                    Vec3f center(new_pos[5], new_pos[6], 0);
                    bool is_ccw = line.type & CoolingLine::TYPE_G3;
                    float dxy = ArcSegment::calc_arc_length(start, end, center, is_ccw);
                    dxy2 = dxy * dxy;
                } else {
                    dxy2 = dif[0] * dif[0] + dif[1] * dif[1];
                }
                float dxyz2 = dxy2 + dif[2] * dif[2];
                if (dxyz2 > 0.f) {
                    // 在xyz中移动，从xyz欧几里得距离计算时间。
                    line.length = sqrt(dxyz2);
                } else if (std::abs(dif[3]) > 0.f) {
                    // 在挤出机轴中移动。
                    line.length = std::abs(dif[3]);
                }
                line.feedrate = new_pos[4];
                assert((line.type & CoolingLine::TYPE_ADJUSTABLE) == 0 || line.feedrate > 0.f);
                if (line.length > 0)
                    line.time = line.length / line.feedrate;
                line.time_max = line.time;
                if ((line.type & CoolingLine::TYPE_ADJUSTABLE) || active_speed_modifier != size_t(-1))
                    line.time_max = (adjustment->slow_down_min_speed == 0.f) ? FLT_MAX : std::max(line.time, line.length / adjustment->slow_down_min_speed);
                // BBS: 添加G2和G3支持
                if (active_speed_modifier < adjustment->lines.size() && ((line.type & CoolingLine::TYPE_G1) ||
                                                                         (line.type & CoolingLine::TYPE_G2) ||
                                                                         (line.type & CoolingLine::TYPE_G3))) {
                    // 在";_EXTRUDE_SET_SPEED"块内，不能有G1 Fxx条目。
                    assert((line.type & CoolingLine::TYPE_HAS_F) == 0);
                    CoolingLine &sm = adjustment->lines[active_speed_modifier];
                    assert(sm.feedrate > 0.f);
                    sm.length   += line.length;
                    sm.time     += line.time;
                    if (sm.time_max != FLT_MAX) {
                        if (line.time_max == FLT_MAX)
                            sm.time_max = FLT_MAX;
                        else
                            sm.time_max += line.time_max;
                    }
                    // 不存储此行。
                    line.type = 0;
                }
            }
            current_pos = std::move(new_pos);
        } else if (boost::starts_with(sline, ";_EXTRUDE_END")) {
            line.type = CoolingLine::TYPE_EXTRUDE_END;
            active_speed_modifier = size_t(-1);
        } else if (boost::starts_with(sline, m_toolchange_prefix)) {
            unsigned int new_extruder = 0;
            auto ret = std::from_chars(sline.data() + m_toolchange_prefix.size(), sline.data() + sline.size(), new_extruder);
            if (std::errc::invalid_argument != ret.ec) {
                // 仅在数字有意义时才更换挤出机。用户可能通过自定义gcodes提供了越界索引 --
                // 这些应被忽略。
                if (new_extruder < map_extruder_to_per_extruder_adjustment.size()) {
                    if (new_extruder != current_extruder) {
                        // 切换工具。
                        line.type        = CoolingLine::TYPE_SET_TOOL;
                        current_extruder = new_extruder;
                        adjustment       = &per_extruder_adjustments[map_extruder_to_per_extruder_adjustment[current_extruder]];
                    }
                } else {
                    // 仅在MM打印机时记录错误。单挤出机打印机可能忽略任何T命令。
                    if (map_extruder_to_per_extruder_adjustment.size() > 1)
                        BOOST_LOG_TRIVIAL(error) << "CoolingBuffer遇到无效的toolchange，可能来自自定义gcode: " << sline;
                }
            }
        } else if (boost::starts_with(sline, ";_OVERHANG_FAN_START")) {
            line.type = CoolingLine::TYPE_OVERHANG_FAN_START;
        } else if (boost::starts_with(sline, ";_OVERHANG_FAN_END")) {
            line.type = CoolingLine::TYPE_OVERHANG_FAN_END;
        } else if (boost::starts_with(sline, ";_INTERNAL_BRIDGE_FAN_START")) { // ORCA: 添加对单独内部桥接风扇速度控制的支持
            line.type = CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START;
        } else if (boost::starts_with(sline, ";_INTERNAL_BRIDGE_FAN_END")) { // ORCA: 添加对单独内部桥接风扇速度控制的支持
            line.type = CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_END;
        } else if (boost::starts_with(sline, ";_SUPP_INTERFACE_FAN_START")) {
            line.type = CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START;
        } else if (boost::starts_with(sline, ";_SUPP_INTERFACE_FAN_END")) {
            line.type = CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_END;
        } else if (boost::starts_with(sline, ";_IRONING_FAN_START")) { // ORCA: 添加对熨烫风扇速度控制的支持
            line.type = CoolingLine::TYPE_IRONING_FAN_START;
        } else if (boost::starts_with(sline, ";_IRONING_FAN_END")) { // ORCA: 添加对熨烫风扇速度控制的支持
            line.type = CoolingLine::TYPE_IRONING_FAN_END;
        } else if (boost::starts_with(sline, "G4 ")) {
            // 解析等待时间。
            line.type = CoolingLine::TYPE_G4;
            size_t pos_S = sline.find('S', 3);
            size_t pos_P = sline.find('P', 3);
            assert(is_decimal_separator_point()); // for atof
            line.time = line.time_max = float(
                (pos_S > 0) ? atof(sline.c_str() + pos_S + 1) :
                (pos_P > 0) ? atof(sline.c_str() + pos_P + 1) * 0.001 : 0.);
        } else if (boost::starts_with(sline, ";_FORCE_RESUME_FAN_SPEED")) {
            line.type = CoolingLine::TYPE_FORCE_RESUME_FAN;
        }

        // Orca: 对于此层第一次挤出之前的任何移动，我们将其从层时间计算中排除。
        if (!layer_had_extrusion) {
            assert((line.type & CoolingLine::TYPE_ADJUSTABLE) == 0);
            line.time = line.time_max = 0;
        }

        if (line.type != 0)
            adjustment->lines.emplace_back(std::move(line));
    }

    return per_extruder_adjustments;
}

// 将挤出机范围减速到slow_down_layer_time。
// 返回完整层的总时间。
static inline void extruder_range_slow_down_non_proportional(
    std::vector<PerExtruderAdjustments*>::iterator it_begin,
    std::vector<PerExtruderAdjustments*>::iterator it_end,
    float time_stretch)
{
    // 减速。尝试均衡进给率。
    std::vector<PerExtruderAdjustments*> by_min_print_speed(it_begin, it_end);
    // 找出挤出机中下一个最高的可调整进给率。
    float feedrate = 0;
    for (PerExtruderAdjustments *adj : by_min_print_speed) {
        adj->idx_line_begin = 0;
        adj->idx_line_end   = 0;
        if (adj->idx_line_begin < adj->n_lines_adjustable && adj->lines[adj->idx_line_begin].feedrate> feedrate)
            feedrate = adj->lines[adj->idx_line_begin].feedrate;
    }
    assert(feedrate > 0.f);
    // 按slow_down_min_speed排序，最大速度在前。
    std::sort(by_min_print_speed.begin(), by_min_print_speed.end(),
        [](const PerExtruderAdjustments *p1, const PerExtruderAdjustments *p2){ return p1->slow_down_min_speed > p2->slow_down_min_speed; });
    // 减速，先快速移动。
    for (;;) {
        // 对于每个挤出机，找到进给率接近feedrate的行范围。
        for (PerExtruderAdjustments *adj : by_min_print_speed) {
            for (adj->idx_line_end = adj->idx_line_begin;
                adj->idx_line_end < adj->n_lines_adjustable && adj->lines[adj->idx_line_end].feedrate > feedrate - EPSILON;
                 ++ adj->idx_line_end) ;
        }
        // 找出挤出机中下一个最高的可调整进给率。
        float feedrate_next = 0.f;
        for (PerExtruderAdjustments *adj : by_min_print_speed)
            if (adj->idx_line_end < adj->n_lines_adjustable && adj->lines[adj->idx_line_end].feedrate > feedrate_next)
                feedrate_next = adj->lines[adj->idx_line_end].feedrate;
        // 减速，受max(feedrate_next, slow_down_min_speed)限制。
        for (auto adj = by_min_print_speed.begin(); adj != by_min_print_speed.end();) {
            // 最多减速time_stretch。
            if ((*adj)->slow_down_min_speed == 0.f) {
                // 所有可调整速度现在降到相同速度，
                // 最小速度设为零。
                float time_adjustable = 0.f;
                for (auto it = adj; it != by_min_print_speed.end(); ++ it)
                    time_adjustable += (*it)->adjustable_time(true);
                float rate = (time_adjustable + time_stretch) / time_adjustable;
                for (auto it = adj; it != by_min_print_speed.end(); ++ it)
                    (*it)->slow_down_proportional(rate, true);
                return;
            } else {
                float feedrate_limit = std::max(feedrate_next, (*adj)->slow_down_min_speed);
                bool  done           = false;
                float time_stretch_max = 0.f;
                for (auto it = adj; it != by_min_print_speed.end(); ++ it)
                    time_stretch_max += (*it)->time_stretch_when_slowing_down_to_feedrate(feedrate_limit);
                if (time_stretch_max >= time_stretch) {
                    feedrate_limit = new_feedrate_to_reach_time_stretch(adj, by_min_print_speed.end(), feedrate_limit, time_stretch, 20);
                    done = true;
                } else
                    time_stretch -= time_stretch_max;
                for (auto it = adj; it != by_min_print_speed.end(); ++ it)
                    (*it)->slow_down_to_feedrate(feedrate_limit);
                if (done)
                    return;
            }
            // 跳过具有几乎相同slow_down_min_speed的其他挤出机，因为它们已处理。
            auto next = adj;
            for (++ next; next != by_min_print_speed.end() && (*next)->slow_down_min_speed > (*adj)->slow_down_min_speed - EPSILON; ++ next);
            adj = next;
        }
        if (feedrate_next == 0.f)
            // 没有其他挤出可用于减速。
            break;
        for (PerExtruderAdjustments *adj : by_min_print_speed) {
            adj->idx_line_begin = adj->idx_line_end;
            feedrate = feedrate_next;
        }
    }
}

// 计算所有挤出机的减速。
float CoolingBuffer::calculate_layer_slowdown(std::vector<PerExtruderAdjustments> &per_extruder_adjustments)
{
    // 按递增的slow_down_layer_time对挤出机排序。
    // 具有较低slow_down_layer_time的层与所有其他
    // slow_down_layer_time高于该值的层一起减速。
    std::vector<PerExtruderAdjustments*> by_slowdown_time;
    by_slowdown_time.reserve(per_extruder_adjustments.size());
    // 仅插入可调整的条目（具有冷却启用和非零可拉伸时间）。
    // 收集不可调整挤出机的总打印时间。
    float elapsed_time_total0 = 0.f;
    for (PerExtruderAdjustments &adj : per_extruder_adjustments) {
        // 此挤出机的当前总时间。
        adj.time_total  = adj.elapsed_time_total();
        // 此挤出机的最大时间，当所有挤出移动减速到最小挤出速度时。
        adj.time_maximum = adj.maximum_time_after_slowdown(true);
        if (adj.cooling_slow_down_enabled && adj.lines.size() > 0) {
            by_slowdown_time.emplace_back(&adj);
            // 对行排序，还设置adj.time_non_adjustable
            adj.sort_lines_by_decreasing_feedrate();
        } else
            elapsed_time_total0 += adj.elapsed_time_total();
    }

    std::sort(by_slowdown_time.begin(), by_slowdown_time.end(),
        [](const PerExtruderAdjustments *adj1, const PerExtruderAdjustments *adj2)
            { return adj1->slow_down_layer_time < adj2->slow_down_layer_time; });

    for (auto cur_begin = by_slowdown_time.begin(); cur_begin != by_slowdown_time.end(); ++ cur_begin) {
        PerExtruderAdjustments &adj = *(*cur_begin);
        // 计算非最终化挤出机上的当前调整后elapsed_time_total。
        float total = elapsed_time_total0;
        for (auto it = cur_begin; it != by_slowdown_time.end(); ++ it)
            total += (*it)->time_total;
        float slow_down_layer_time = adj.slow_down_layer_time * 1.001f;
        if (total > slow_down_layer_time) {
            // 当前总时间高于其余挤出机的最小阈值，不调整任何内容。
        } else {
            // 调整此挤出机和所有后续（更高m_config.slow_down_layer_time）挤出机。
            // 如果包括外壁在内的所有内容都减速，则求和最大减速时间。
            float max_time = elapsed_time_total0;
            for (auto it = cur_begin; it != by_slowdown_time.end(); ++ it)
                max_time += (*it)->time_maximum;
            if (max_time > slow_down_layer_time) {
                extruder_range_slow_down_non_proportional(cur_begin, by_slowdown_time.end(), slow_down_layer_time - total);
            } else {
                // 减速到最大可能。
                for (auto it = cur_begin; it != by_slowdown_time.end(); ++ it)
                    (*it)->slowdown_to_minimum_feedrate(true);
            }
        }
        elapsed_time_total0 += adj.elapsed_time_total();
    }

    return elapsed_time_total0;
}

// 对存储在per_extruder_adjustments中的G-code行应用减速，必要时启用风扇。
// 返回调整后的G-code。
std::string CoolingBuffer::apply_layer_cooldown(
    // 当前层的源G-code。
    const std::string                      &gcode,
    // 当前层的ID，用于禁用前n层的风扇。
    size_t                                  layer_id,
    // 减速后此层的总时间，用于控制风扇。
    float                                   layer_time,
    // 每个挤出机的G-code行列表及其冷却属性。
    std::vector<PerExtruderAdjustments>    &per_extruder_adjustments)
{
    // 首先按它们在源G-code中的位置对多个挤出机的调整行进行排序。
    std::vector<const CoolingLine*> lines;
    {
        size_t n_lines = 0;
        for (const PerExtruderAdjustments &adj : per_extruder_adjustments)
            n_lines += adj.lines.size();
        lines.reserve(n_lines);
        for (const PerExtruderAdjustments &adj : per_extruder_adjustments)
            for (const CoolingLine &line : adj.lines)
                lines.emplace_back(&line);
        std::sort(lines.begin(), lines.end(), [](const CoolingLine *ln1, const CoolingLine *ln2) { return ln1->line_start < ln2->line_start; } );
    }
    // 其次生成调整后的G-code。
    std::string new_gcode;
    new_gcode.reserve(gcode.size() * 2);
    bool overhang_fan_control= false;
    int  overhang_fan_speed   = 0;
    bool internal_bridge_fan_control= false; // ORCA: 添加对单独内部桥接风扇速度控制的支持
    int  internal_bridge_fan_speed   = 0; // ORCA: 添加对单独内部桥接风扇速度控制的支持
    bool supp_interface_fan_control= false;
    int  supp_interface_fan_speed = 0;
    bool ironing_fan_control= false; // ORCA: 添加对熨烫风扇速度控制的支持
    int  ironing_fan_speed   = 0; // ORCA: 添加对熨烫风扇速度控制的支持
    auto change_extruder_set_fan = [ this, layer_id, layer_time, &new_gcode,
        &overhang_fan_control, &overhang_fan_speed,
        &internal_bridge_fan_control, &internal_bridge_fan_speed,
        &supp_interface_fan_control, &supp_interface_fan_speed,
        &ironing_fan_control, &ironing_fan_speed
    ](bool immediately_apply) {
#define EXTRUDER_CONFIG(OPT) m_config.OPT.get_at(m_current_extruder)
        float fan_min_speed = EXTRUDER_CONFIG(fan_min_speed);
        float fan_speed_new = EXTRUDER_CONFIG(reduce_fan_stop_start_freq) ? fan_min_speed : 0;
        //BBS
        int additional_fan_speed_new = EXTRUDER_CONFIG(additional_cooling_fan_speed);
        int close_fan_the_first_x_layers = EXTRUDER_CONFIG(close_fan_the_first_x_layers);
        // 是否启用了风扇速度斜坡？
        int full_fan_speed_layer = EXTRUDER_CONFIG(full_fan_speed_layer);
        supp_interface_fan_speed = EXTRUDER_CONFIG(support_material_interface_fan_speed);

        if (close_fan_the_first_x_layers <= 0 && full_fan_speed_layer > 0) {
            // 当从close_fan_the_first_x_layers斜坡增加到full_fan_speed_layer时，强制close_fan_the_first_x_layers大于零，
            // 这样至少在第一层会有零风扇速度。
            close_fan_the_first_x_layers = 1;
        }
        if (int(layer_id) >= close_fan_the_first_x_layers) {
            float   fan_max_speed             = EXTRUDER_CONFIG(fan_max_speed);
            float slow_down_layer_time = float(EXTRUDER_CONFIG(slow_down_layer_time));
            float fan_cooling_layer_time      = float(EXTRUDER_CONFIG(fan_cooling_layer_time));
            //BBS: 始终启用根据层时间插值风扇速度
            //if (EXTRUDER_CONFIG(cooling)) {
                if (layer_time < slow_down_layer_time) {
                    // 层时间非常短。全速启用风扇。
                    fan_speed_new = fan_max_speed;
                } else if (layer_time < fan_cooling_layer_time) {
                    // 层时间相当短。根据当前层时间比例启用风扇。
                    assert(layer_time >= slow_down_layer_time);
                    double t = (layer_time - slow_down_layer_time) / (fan_cooling_layer_time - slow_down_layer_time);
                    fan_speed_new = int(floor(t * fan_min_speed + (1. - t) * fan_max_speed) + 0.5);
                }
            //}
            overhang_fan_speed   = EXTRUDER_CONFIG(overhang_fan_speed);
            if (int(layer_id) >= close_fan_the_first_x_layers && int(layer_id) + 1 < full_fan_speed_layer) {
                // 从close_fan_the_first_x_layers到full_fan_speed_layer斜坡增加风扇速度。
                float factor = float(int(layer_id + 1) - close_fan_the_first_x_layers) / float(full_fan_speed_layer - close_fan_the_first_x_layers);
                fan_speed_new    = std::clamp(int(float(fan_speed_new) * factor + 0.5f), 0, 255);
                overhang_fan_speed = std::clamp(int(float(overhang_fan_speed) * factor + 0.5f), 0, 255);
            }
            supp_interface_fan_speed = EXTRUDER_CONFIG(support_material_interface_fan_speed);
            supp_interface_fan_control = supp_interface_fan_speed >= 0;

            overhang_fan_control = overhang_fan_speed > fan_speed_new;

            // ORCA: 添加对单独内部桥接风扇速度控制的支持
            internal_bridge_fan_speed   = EXTRUDER_CONFIG(internal_bridge_fan_speed);
            internal_bridge_fan_control = internal_bridge_fan_speed >=0;

            if( internal_bridge_fan_speed < 0 ) { // ORCA: Orca内部桥接风扇速度设置的向后兼容性 - 如果设置为-1（默认值），使用悬垂风扇速度设置。
                internal_bridge_fan_speed = overhang_fan_speed;
                internal_bridge_fan_control = overhang_fan_control;
            }

            // ORCA: 添加对熨烫风扇速度控制的支持
            ironing_fan_speed   = EXTRUDER_CONFIG(ironing_fan_speed);
            ironing_fan_control = ironing_fan_speed >= 0;
#undef EXTRUDER_CONFIG

        } else {
            overhang_fan_control = false;
            overhang_fan_speed   = 0;
            fan_speed_new      = 0;
            additional_fan_speed_new = 0;
            supp_interface_fan_control = false;
            supp_interface_fan_speed   = 0;
            internal_bridge_fan_control = false; // ORCA: 添加对单独内部桥接风扇速度控制的支持
            internal_bridge_fan_speed = 0; // ORCA: 添加对单独内部桥接风扇速度控制的支持
            ironing_fan_control = false; // ORCA: 添加对熨烫风扇速度控制的支持
            ironing_fan_speed = 0; // ORCA: 添加对熨烫风扇速度控制的支持
        }
        if (fan_speed_new != m_fan_speed) {
            m_fan_speed = fan_speed_new;
            m_current_fan_speed = fan_speed_new;
            if (immediately_apply)
                new_gcode  += GCodeWriter::set_fan(m_config.gcode_flavor, m_fan_speed);
        }
        //BBS
        if (additional_fan_speed_new != m_additional_fan_speed) {
            m_additional_fan_speed = additional_fan_speed_new;
            if (immediately_apply && m_config.auxiliary_fan.value)
                new_gcode += GCodeWriter::set_additional_fan(m_additional_fan_speed);
        }
    };

    const char         *pos               = gcode.c_str();
    int                 current_feedrate  = 0;
    change_extruder_set_fan(true);

    // Orca: 通过延迟GCodeWriter::set_fan调用来减少设置风扇命令。灵感来自SuperSlicer
    // 定义fan_speed_change_requests并用所有可能类型的风扇速度变化请求初始化
    std::unordered_map<int, bool> fan_speed_change_requests = {{CoolingLine::TYPE_OVERHANG_FAN_START, false},
                                                               {CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START, false}, // ORCA: 添加对单独内部桥接风扇速度控制的支持
                                                               {CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START, false},
                                                               {CoolingLine::TYPE_IRONING_FAN_START, false}, // ORCA: 添加对熨烫风扇速度控制的支持
                                                               {CoolingLine::TYPE_FORCE_RESUME_FAN, false}};
    bool need_set_fan = false;

    for (const CoolingLine *line : lines) {
        const char *line_start  = gcode.c_str() + line->line_start;
        const char *line_end    = gcode.c_str() + line->line_end;
        if (line_start > pos)
            new_gcode.append(pos, line_start - pos);
        if (line->type & CoolingLine::TYPE_SET_TOOL) {
            unsigned int new_extruder = 0;
            auto ret = std::from_chars(line_start + m_toolchange_prefix.size(), line_end, new_extruder);
            if (std::errc::invalid_argument != ret.ec) {
                if (new_extruder != m_current_extruder) {
                    m_current_extruder = new_extruder;
                    change_extruder_set_fan(true);
                }
            }
            new_gcode.append(line_start, line_end - line_start);
        } else if (line->type & CoolingLine::TYPE_OVERHANG_FAN_START) {
            if (overhang_fan_control && !fan_speed_change_requests[CoolingLine::TYPE_OVERHANG_FAN_START]) {
                need_set_fan = true;
                fan_speed_change_requests[CoolingLine::TYPE_OVERHANG_FAN_START] = true;
           }
        } else if (line->type & CoolingLine::TYPE_OVERHANG_FAN_END) {
            if (overhang_fan_control && fan_speed_change_requests[CoolingLine::TYPE_OVERHANG_FAN_START]) {
                fan_speed_change_requests[CoolingLine::TYPE_OVERHANG_FAN_START] = false;
            }
            need_set_fan = true;
        } else if (line->type & CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START) { // ORCA: 添加对单独内部桥接风扇速度控制的支持
            if (internal_bridge_fan_control && !fan_speed_change_requests[CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START]) {
                need_set_fan = true;
                fan_speed_change_requests[CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START] = true;
           }
        } else if (line->type & CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_END) { // ORCA: 添加对单独内部桥接风扇速度控制的支持
            if (internal_bridge_fan_control && fan_speed_change_requests[CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START]) {
                fan_speed_change_requests[CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START] = false;
            }
            need_set_fan = true;
        } else if (line->type & CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START) {
            if (supp_interface_fan_control && !fan_speed_change_requests[CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START]) {
                fan_speed_change_requests[CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START] = true;
                need_set_fan = true;
            }
        } else if (line->type & CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_END && fan_speed_change_requests[CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START]) {
            if (supp_interface_fan_control) {
                fan_speed_change_requests[CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START] = false;
            }
            need_set_fan = true;
        } else if (line->type & CoolingLine::TYPE_IRONING_FAN_START) {
            if (ironing_fan_control && !fan_speed_change_requests[CoolingLine::TYPE_IRONING_FAN_START]) {
                fan_speed_change_requests[CoolingLine::TYPE_IRONING_FAN_START] = true;
                need_set_fan = true;
            }
        } else if (line->type & CoolingLine::TYPE_IRONING_FAN_END && fan_speed_change_requests[CoolingLine::TYPE_IRONING_FAN_START]) {
            if (ironing_fan_control) {
                fan_speed_change_requests[CoolingLine::TYPE_IRONING_FAN_START] = false;
            }
            need_set_fan = true;
        } else if (line->type & CoolingLine::TYPE_FORCE_RESUME_FAN) {
            // 检查是否有任何风扇速度变化请求处于活动状态
            if (m_fan_speed != -1 && !std::any_of(fan_speed_change_requests.begin(), fan_speed_change_requests.end(), [](const std::pair<int, bool>& p) { return p.second; })){
                fan_speed_change_requests[CoolingLine::TYPE_FORCE_RESUME_FAN] = true;
                need_set_fan = true;
            }
            if (m_additional_fan_speed != -1 && m_config.auxiliary_fan.value)
                new_gcode += GCodeWriter::set_additional_fan(m_additional_fan_speed);
        }
        else if (line->type & CoolingLine::TYPE_EXTRUDE_END) {
            // 只删除此注释。
        } else if (line->type & (CoolingLine::TYPE_ADJUSTABLE | CoolingLine::TYPE_EXTERNAL_PERIMETER | CoolingLine::TYPE_WIPE | CoolingLine::TYPE_HAS_F)) {
            // 找到注释的开始，或滚动到行尾。
            const char *end = line_start;
            for (; end < line_end && *end != ';'; ++ end);
            // 找到'F'字。
            const char *fpos            = strstr(line_start + 2, " F") + 2;
            int         new_feedrate    = current_feedrate;
            // 修改当前G-code行的F字。
            bool        modify          = false;
            // 从当前G-code行中移除F字。
            bool        remove          = false;
            assert(fpos != nullptr);
            new_feedrate = line->slowdown ? int(floor(60. * line->feedrate + 0.5)) : atoi(fpos);
            if (new_feedrate == current_feedrate) {
                // 无需更改F值。
                if ((line->type & (CoolingLine::TYPE_ADJUSTABLE | CoolingLine::TYPE_EXTERNAL_PERIMETER | CoolingLine::TYPE_WIPE)) || line->length == 0.)
                    // 进给率未改变，此行不移动打印头。跳过完整的G-code行，包括G-code注释。
                    end = line_end;
                else
                    // 从G0/G1行中移除进给率。G-code行可能变空！
                    remove = true;
            } else if (line->slowdown) {
                // F值将被覆盖。
                modify = true;
            } else {
                // F值与current_feedrate不同，但未减速，因此不修改G-code行。
                // 无注释输出行。
                new_gcode.append(line_start, end - line_start);
                current_feedrate = new_feedrate;
            }
            if (modify || remove) {
                if (modify) {
                    // 替换进给率。
                    new_gcode.append(line_start, fpos - line_start);
                    current_feedrate = new_feedrate;
                    char buf[64];
                    sprintf(buf, "%d", int(current_feedrate));
                    new_gcode += buf;
                } else {
                    // 移除进给率字。
                    const char *f = fpos;
                    // 将指针滚动到'F'字之前。
                    for (f -= 2; f > line_start && (*f == ' ' || *f == '\t'); -- f);

                    if ((f - line_start == 1) && *line_start == 'G' && (*f == '1' || *f == '0')) {
                        // BBS: 移除'F'部分后仅保留"G1"或"G0"的行，不保存
                    } else {
                        // 追加到F字之前，不带尾随空格。
                        new_gcode.append(line_start, f - line_start + 1);
                    }
                }
                // 跳过F参数的非空白字符直到注释或行尾。
                for (; fpos != end && *fpos != ' ' && *fpos != ';' && *fpos != '\n'; ++ fpos);
                // 追加行的其余部分，无注释。
                if (fpos < end)
                    // G-code行尚未为空。输出其余部分。
                    new_gcode.append(fpos, end - fpos);
                else if (remove && new_gcode == "G1") {
                    // G-code行仅包含F字，现在为空。完全移除它，包括注释。
                    new_gcode.resize(new_gcode.size() - 2);
                    end = line_end;
                }
            }
            // 处理行的其余部分。
            if (end < line_end) {
                if (line->type & (CoolingLine::TYPE_ADJUSTABLE | CoolingLine::TYPE_EXTERNAL_PERIMETER | CoolingLine::TYPE_WIPE)) {
                    // 处理注释，移除";_EXTRUDE_SET_SPEED"、";_EXTERNAL_PERIMETER"、";_WIPE"
                    std::string comment(end, line_end);
                    boost::replace_all(comment, ";_EXTRUDE_SET_SPEED", "");
                    if (line->type & CoolingLine::TYPE_EXTERNAL_PERIMETER)
                        boost::replace_all(comment, ";_EXTERNAL_PERIMETER", "");
                    if (line->type & CoolingLine::TYPE_WIPE)
                        boost::replace_all(comment, ";_WIPE", "");
                    new_gcode += comment;
                } else {
                    // 直接附加源行的其余部分。
                    new_gcode.append(end, line_end - end);
                }
            }
        } else {
            new_gcode.append(line_start, line_end - line_start);
        }

        if (need_set_fan) {
            if (fan_speed_change_requests[CoolingLine::TYPE_OVERHANG_FAN_START]){
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, overhang_fan_speed);
                m_current_fan_speed = overhang_fan_speed;
            } else if (fan_speed_change_requests[CoolingLine::TYPE_INTERNAL_BRIDGE_FAN_START]){ // ORCA: 添加对单独内部桥接风扇速度控制的支持
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, internal_bridge_fan_speed);
                m_current_fan_speed = internal_bridge_fan_speed;
            }
            else if (fan_speed_change_requests[CoolingLine::TYPE_SUPPORT_INTERFACE_FAN_START]){
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, supp_interface_fan_speed);
                m_current_fan_speed = supp_interface_fan_speed;
            }
            else if (fan_speed_change_requests[CoolingLine::TYPE_IRONING_FAN_START]){
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, ironing_fan_speed);
                m_current_fan_speed = ironing_fan_speed;
            }
            else if(fan_speed_change_requests[CoolingLine::TYPE_FORCE_RESUME_FAN] && m_current_fan_speed != -1){
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, m_current_fan_speed);
                fan_speed_change_requests[CoolingLine::TYPE_FORCE_RESUME_FAN] = false;
            }
            else
                new_gcode += GCodeWriter::set_fan(m_config.gcode_flavor, m_fan_speed);
            need_set_fan = false;
        }
        pos = line_end;
    }
    const char *gcode_end = gcode.c_str() + gcode.size();
    if (pos < gcode_end)
        new_gcode.append(pos, gcode_end - pos);

    return new_gcode;
}

} // namespace Slic3r
