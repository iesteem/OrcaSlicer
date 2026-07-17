#include "FanMover.hpp"

#include "GCodeReader.hpp"

#include <iomanip>
/*
#include <memory.h>
#include <string.h>
#include <float.h>

#include "../libslic3r.h"
#include "../PrintConfig.hpp"
#include "../Utils.hpp"
#include "Print.hpp"

#include <boost/log/trivial.hpp>
*/


namespace Slic3r {

const std::string& FanMover::process_gcode(const std::string& gcode, bool flush)
{
    m_process_output = "";

    // 重新计算缓冲区时间以从舍入中恢复
    m_buffer_time_size = 0;
    for (auto& data : m_buffer) m_buffer_time_size += data.time;

    if(!gcode.empty())
        m_parser.parse_buffer(gcode,
            [this](GCodeReader& reader, const GCodeReader::GCodeLine& line) { /*m_process_output += line.raw() + "\n";*/ this->_process_gcode_line(reader, line); });

    if (flush) {
        while (!m_buffer.empty()) {
            m_process_output += m_buffer.front().raw + "\n";
            remove_from_buffer(m_buffer.begin());
        }
    }

    return m_process_output;
}

bool is_end_of_word(char c) {
   return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0;
}

float get_axis_value(const std::string& line, char axis)
{
    char match[3] = " X";
    match[1] = axis;

    size_t pos = line.find(match);
    if (pos == std::string::npos) {
        return NAN;
    }
    pos += 2;
    //size_t end = std::min(line.find(' ', pos + 1), line.find(';', pos + 1));
    // 尝试解析数值。
    const char* c = line.c_str();
    char* pend = nullptr;
    errno = 0;
    double  v = strtod(c + pos, &pend);
    if (pend != nullptr && errno == 0 && pend != c) {
        // 轴值已正确解析。
        return float(v);
    }
    return NAN;
}

void change_axis_value(std::string& line, char axis, const float new_value, const int decimal_digits)
{

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimal_digits) << new_value;

    char match[3] = " X";
    match[1] = axis;

    size_t pos = line.find(match) + 2;
    size_t end = std::min(line.find(' ', pos + 1), line.find(';', pos + 1));
    line = line.replace(pos, end - pos, ss.str());
}

int16_t get_fan_speed(const std::string &line, GCodeFlavor flavor) {
    if (line.compare(0, 4, "M106") == 0) {
        if (flavor == (gcfMach3) || flavor == (gcfMachinekit)) {
            return (int16_t)get_axis_value(line, 'P');
        } else {
            // Bambu机器对部件冷却风扇同时使用M106 P1（不是P0！）和M106。
            // 非Bambu机器通常使用M106（不带P参数）用于部件冷却风扇。
            // P2保留用于辅助风扇，无论是否是Bambu机器。
            // 为了与Bambu机器保持兼容性，我们接受M106和M106 P1作为控制部件冷却风扇的
            // 唯一两种有效的gcode形式。任何其他命令都将被忽略！
            const auto idx = get_axis_value(line, 'P');
            if (!isnan(idx) && idx != 1.0f) {
                return -1;
            }
            return (int16_t)get_axis_value(line, 'S');
        }
    } else if (line.compare(0, 4, "M127") == 0 || line.compare(0, 4, "M107") == 0) {
        return 0;
    } else if ((flavor == (gcfMakerWare) || flavor == (gcfSailfish)) && line.compare(0, 4, "M126") == 0) {
        return (int16_t)get_axis_value(line, 'T');
    } else {
        return -1;
    }

}

void FanMover::_put_in_middle_G1(std::list<BufferData>::iterator item_to_split, float nb_sec_since_itemtosplit_start, BufferData &&line_to_write) {
    assert(item_to_split != m_buffer.end());
    if (nb_sec_since_itemtosplit_start > item_to_split->time * 0.9) {
        // 不需要真正分割，在后面打印
        m_buffer.insert(next(item_to_split), line_to_write);
    } else if (nb_sec_since_itemtosplit_start < item_to_split->time * 0.1) {
        // 不需要真正分割，在前面打印
        //如果line_to_split.time == 0也会在前面打印
        m_buffer.insert(item_to_split, line_to_write);
    } else if (item_to_split->raw.size() > 2
        && item_to_split->raw[0] == 'G' && item_to_split->raw[1] == '1' && item_to_split->raw[2] == ' ') {
        float percent = nb_sec_since_itemtosplit_start / item_to_split->time;
        BufferData before = *item_to_split;
        before.time *= percent;
        item_to_split->time *= (1-percent);
        if (item_to_split->dx != 0) {
            before.dx = item_to_split->dx * percent;
            item_to_split->x += before.dx;
            item_to_split->dx = item_to_split->dx * (1-percent);
            change_axis_value(before.raw, 'X', before.x + before.dx, 3);
        }
        if (item_to_split->dy != 0) {
            before.dy = item_to_split->dy * percent;
            item_to_split->y += before.dy;
            item_to_split->dy = item_to_split->dy * (1 - percent);
            change_axis_value(before.raw, 'Y', before.y + before.dy, 3);
        }
        if (item_to_split->dz != 0) {
            before.dz = item_to_split->dz * percent;
            item_to_split->z += before.dz;
            item_to_split->dz = item_to_split->dz * (1 - percent);
            change_axis_value(before.raw, 'Z', before.z + before.dz, 3);
        }
        if (item_to_split->de != 0) {
            if (relative_e) {
                before.de = item_to_split->de * percent;
                change_axis_value(before.raw, 'E', before.de, 5);
                item_to_split->de = item_to_split->de * (1 - percent);
                change_axis_value(item_to_split->raw, 'E', item_to_split->de, 5);
            } else {
                before.de = item_to_split->de * percent;
                item_to_split->e += before.de;
                item_to_split->de = item_to_split->de * (1 - percent);
                change_axis_value(before.raw, 'E', before.e + before.de, 5);
            }
        }
        //先添加before，然后line_to_write，最后是修改后的数据。
        m_buffer.insert(item_to_split, before);
        m_buffer.insert(item_to_split, line_to_write);

    } else {
        //不是G1，在前面打印
        m_buffer.insert(item_to_split, line_to_write);
    }
}

void FanMover::_print_in_middle_G1(BufferData& line_to_split, float nb_sec, const std::string &line_to_write) {
    if (nb_sec < line_to_split.time * 0.1) {
        // 不需要真正分割，在后面打印
        m_process_output += line_to_split.raw + "\n";
        m_process_output += line_to_write + (line_to_write.back() == '\n'?"":"\n");
    } else if (nb_sec > line_to_split.time * 0.9) {
        // 不需要真正分割，在前面打印
        //如果line_to_split.time == 0也会在前面打印
        m_process_output += line_to_write + (line_to_write.back() == '\n' ? "" : "\n");
        m_process_output += line_to_split.raw + "\n";
    }else if(line_to_split.raw.size() > 2
        && line_to_split.raw[0] == 'G' && line_to_split.raw[1] == '1' && line_to_split.raw[2] == ' ') {
        float percent = nb_sec / line_to_split.time;
        std::string before = line_to_split.raw;
        std::string& after = line_to_split.raw;
        if (line_to_split.dx != 0) {
            change_axis_value(before, 'X', line_to_split.x + line_to_split.dx * percent, 3);
        }
        if (line_to_split.dy != 0) {
            change_axis_value(before, 'Y', line_to_split.y + line_to_split.dy * percent, 3);
        }
        if (line_to_split.dz != 0) {
            change_axis_value(before, 'Z', line_to_split.z + line_to_split.dz * percent, 3);
        }
        if (line_to_split.de != 0) {
            if (relative_e) {
                change_axis_value(before, 'E', line_to_split.de * percent, 5);
                change_axis_value(after, 'E', line_to_split.de * (1 - percent), 5);
            } else {
                change_axis_value(before, 'E', line_to_split.e + line_to_split.de * percent, 5);
            }
        }
        m_process_output += before + "\n";
        m_process_output += line_to_write + (line_to_write.back() == '\n' ? "" : "\n");
        m_process_output += line_to_split.raw + "\n";

    } else {
        //不是G1，在前面打印
        m_process_output += line_to_write + (line_to_write.back() == '\n' ? "" : "\n");
        m_process_output += line_to_split.raw + "\n";
    }
}

void FanMover::_remove_slow_fan(int16_t min_speed, float past_sec) {
    //擦除缓冲区中的风扇 -> 如果在升速过程中则不减速。
    //我们从"最近"侧开始，只要不把past_sec推到0就继续删除
    auto it = m_buffer.begin();
    while (it != m_buffer.end() && past_sec > 0) {
        past_sec -= it->time;
        if (it->fan_speed >= 0 && it->fan_speed < min_speed){
            //找到比我们低的值
            it = remove_from_buffer(it);

        } else {
            ++it;
        }
    }

}

std::string FanMover::_set_fan(int16_t speed) {
    //const Tool* tool = m_writer.get_tool(m_currrent_extruder < 20 ? m_currrent_extruder : 0);
    return GCodeWriter::set_fan(m_writer.config.gcode_flavor.value, speed);
}


bool parse_number(const std::string_view sv, int& out)
{
    {
        // 传统转换，由于需要在转换前复制字符串而导致开销。
        try {
            assert(sv.size() < 1024);
            assert(sv.data() != nullptr);
            std::string str{ sv };
            size_t read = 0;
            out = std::stoi(str, &read);
            return str.size() == read;
        }
        catch (...) {
            return false;
        }
    }
}

//FIXME: 添加其他固件
// 或者直接创建那个新的gcode writer架构
void FanMover::_process_T(const std::string_view command)
{
    if (command.length() > 1) {
        int eid = 0;
        if (!parse_number(command.substr(1), eid) || eid < 0 || eid > 255) {
            GCodeFlavor flavor = m_writer.config.gcode_flavor;
            // 特定于MMU2 V2（参见https://www.help.prusa3d.com/en/article/prusa-specific-g-codes_112173）：
            if ((flavor == gcfMarlinLegacy || flavor == gcfMarlinFirmware) && (command == "Tx" || command == "Tc" || command == "T?"))
                return;

            // T-1是RepRap固件的有效gcode行（用于取消选择所有工具）参见https://github.com/prusa3d/PrusaSlicer/issues/5677
            if ((flavor != gcfRepRapFirmware && flavor != gcfRepRapSprinter) || eid != -1)
                m_currrent_extruder = static_cast<uint16_t>(0);
        } else {
            m_currrent_extruder = static_cast<uint16_t>(eid);
        }
    }
}

void FanMover::_process_gcode_line(GCodeReader& reader, const GCodeReader::GCodeLine& line)
{
    // 处理"普通"gcode行
    bool need_flush = false;
    std::string cmd(line.cmd());
    double time = 0;
    int16_t fan_speed = -1;
    if (cmd.length() > 1) {
        if (line.has_f())
            m_current_speed = line.f() / 60.0f;
        switch (::toupper(cmd[0])) {
        case 'T':
        case 't':
            _process_T(cmd);
                break;
        case 'G':
        {
            if (::atoi(&cmd[1]) == 1 || ::atoi(&cmd[1]) == 0) {
                double distx = line.dist_X(reader);
                double disty = line.dist_Y(reader);
                double distz = line.dist_Z(reader);
                double dist = distx * distx + disty * disty + distz * distz;
                if (dist > 0) {
                    dist = std::sqrt(dist);
                    time = dist / m_current_speed;
                }
            }
            break;
        }
        case 'M':
        {
            fan_speed = get_fan_speed(line.raw(), m_writer.config.gcode_flavor);
            if (fan_speed >= 0) {
                const auto fan_baseline = 255.0;
                fan_speed = 100 * fan_speed / fan_baseline;
                //速度变化：停止kickstart回退（如果有）
                m_current_kickstart.time = -1;
                if (!m_is_custom_gcode) {
                    // 如果减速 => 放入队列。如果不减速 =>
                    if (m_back_buffer_fan_speed < fan_speed) {
                        if (nb_seconds_delay > 0 && (!only_overhangs || current_role == ExtrusionRole::erOverhangPerimeter)) {
                            //不将此命令放入队列
                            time = -1;
                            // 此M106需要放到过去
                            //检查是否有（kickstart且不在减速中）
                            if (kickstart > 0 && fan_speed > m_front_buffer_fan_speed) {
                                //停止当前的kickstart，它不再相关
                                if (m_current_kickstart.time > 0) {
                                    m_current_kickstart.time = (-1);
                                }

                                //如果kickstart
                                // 首先擦除所有低于该值的
                                _remove_slow_fan(fan_speed, m_buffer_time_size + 1);
                                // 然后擦除所有低于kickstart的
                                _remove_slow_fan(fan_baseline, kickstart);
                                // 打印我
                                if (!m_buffer.empty() && (m_buffer_time_size - m_buffer.front().time * 0.1) > nb_seconds_delay) {
                                    _print_in_middle_G1(m_buffer.front(), m_buffer_time_size - nb_seconds_delay, _set_fan(100));//m_writer.set_fan(100, true)); //FIXME extruder id (or use the gcode writer, but then you have to disable the multi-thread thing
                                    remove_from_buffer(m_buffer.begin());
                                } else {
                                    m_process_output += _set_fan(100);//m_writer.set_fan(100, true)); //FIXME extruder id (or use the gcode writer, but then you have to disable the multi-thread thing
                                }
                                //如果可能则写入队列
                                const float kickstart_duration = kickstart * float(fan_speed - m_front_buffer_fan_speed) / 100.f;
                                float time_count = kickstart_duration;
                                auto it = m_buffer.begin();
                                while (it != m_buffer.end() && time_count > 0) {
                                    time_count -= it->time;
                                    if (time_count< 0) {
                                        //找到比我们低的值
                                        _put_in_middle_G1(it, it->time + time_count, BufferData(std::string(line.raw()), 0, fan_speed, true));
                                        //找到，停止
                                        break;
                                    }
                                    ++it;
                                }
                                if (time_count > 0) {
                                    //无法放入缓冲区，使用m_current_kickstart
                                    m_current_kickstart.fan_speed = fan_speed;
                                    m_current_kickstart.time = time_count;
                                    m_current_kickstart.raw = line.raw();
                                }
                                m_front_buffer_fan_speed = fan_speed;
                            } else {
                                // 首先擦除所有低于该值的
                                _remove_slow_fan(fan_speed, m_buffer_time_size + 1);
                                // 然后写入风扇命令
                                if (!m_buffer.empty() && (m_buffer_time_size - m_buffer.front().time * 0.1) > nb_seconds_delay) {
                                    _print_in_middle_G1(m_buffer.front(), m_buffer_time_size - nb_seconds_delay, line.raw());
                                    remove_from_buffer(m_buffer.begin());
                                } else {
                                    m_process_output += line.raw() + "\n";
                                }
                                m_front_buffer_fan_speed = fan_speed;
                            }
                        } else {
                            if (kickstart <= 0) {
                                //无事可做
                                //我们不设置time = -1；所以它会像其他行一样在缓冲区中打印
                            } else if (m_current_kickstart.time > 0) {
                                //精挑细选这个
                                if (m_back_buffer_fan_speed >= fan_speed) {
                                    //停止kickstart
                                    m_current_kickstart.time = -1;
                                    //这将在time >=0时立即打印
                                } else {
                                    // 添加一些持续时间到kickstart并用于我。
                                    float kickstart_duration = kickstart * float(fan_speed - m_back_buffer_fan_speed) / 100.f;
                                    m_current_kickstart.fan_speed = fan_speed;
                                    m_current_kickstart.time += kickstart_duration;
                                    m_current_kickstart.raw = line.raw();
                                    //我由m_current_kickstart打印
                                    time = -1;
                                }
                            } else if(m_back_buffer_fan_speed < fan_speed - 10){ //仅当变化超过10%时才kickstart
                                //不写入此行，因为它需要延迟
                                time = -1;
                                //获取kickstart的持续时间
                                float kickstart_duration = kickstart * float(fan_speed - m_back_buffer_fan_speed) / 100.f;
                                //如果kickstart，先写入M106 S[fan_baseline]
                                //设置目标速度并设置kickstart标志
                                put_in_buffer(BufferData(_set_fan(100)//m_writer.set_fan(100, true)); //FIXME extruder id (or use the gcode writer, but then you have to disable the multi-thread thing
                                    , 0, fan_speed, true));
                                //kickstart!
                                //m_process_output += m_writer.set_fan(100, true);
                                //为将来添加正常速度行
                                m_current_kickstart.fan_speed = fan_speed;
                                m_current_kickstart.time = kickstart_duration;
                                m_current_kickstart.raw = line.raw();
                            }
                        }
                    }
                    //更新后端缓冲区风扇速度
                    m_back_buffer_fan_speed = fan_speed;
                } else {
                    // 必须刷新缓冲区以避免擦除风扇命令。
                    need_flush = true;
                }
            }
            break;
        }
        }
    } else {
        if(!line.raw().empty() && line.raw().front() == ';')
        {
            if (line.raw().size() > 10 && line.raw().rfind(";TYPE:", 0) == 0) {
                // 获取下一个挤出的类型
                std::string extrusion_string = line.raw().substr(6, line.raw().size() - 6);
                current_role = ExtrusionEntity::string_to_role(extrusion_string);
            }
            if (line.raw().size() > 16) {
                if (line.raw().rfind("; custom gcode", 0) != std::string::npos) {
                    if (line.raw().rfind("; custom gcode end", 0) != std::string::npos)
                        m_is_custom_gcode = false;
                    else
                        m_is_custom_gcode = true;
                }
            }
        }
    }

    if (time >= 0) {
        BufferData& new_data = put_in_buffer(BufferData(line.raw(), time, fan_speed));
        if (line.has(Axis::X)) {
            new_data.x = reader.x();
            new_data.dx = line.dist_X(reader);
        }
        if (line.has(Axis::Y)) {
            new_data.y = reader.y();
            new_data.dy = line.dist_Y(reader);
        }
        if (line.has(Axis::Z)) {
            new_data.z = reader.z();
            new_data.dz = line.dist_Z(reader);
        }
        if (line.has(Axis::E)) {
            new_data.e = reader.e();
            if (relative_e)
                new_data.de = line.e();
            else
                new_data.de = line.dist_E(reader);
        }

        if (m_current_kickstart.time > 0 && time > 0) {
            m_current_kickstart.time -= time;
            if (m_current_kickstart.time < 0) {
                //prev是可能的，因为我们刚做了emplace_back。
                _put_in_middle_G1(prev(m_buffer.end()), time + m_current_kickstart.time, BufferData{ m_current_kickstart.raw, 0, m_current_kickstart.fan_speed, true });
            }
        }
    }/* else {
        BufferData& new_data = put_in_buffer(BufferData("; del? "+line.raw(), 0, fan_speed));
        if (line.has(Axis::X)) {
            new_data.x = reader.x();
            new_data.dx = line.dist_X(reader);
        }
        if (line.has(Axis::Y)) {
            new_data.y = reader.y();
            new_data.dy = line.dist_Y(reader);
        }
        if (line.has(Axis::Z)) {
            new_data.z = reader.z();
            new_data.dz = line.dist_Z(reader);
        }
        if (line.has(Axis::E)) {
            new_data.e = reader.e();
            if (relative_e)
                new_data.de = line.e();
            else
                new_data.de = line.dist_E(reader);
        }
    }*/
    // 将行放回gcode
    //如果缓冲区太大，刷新它。
    if (time >= 0) {
        while (!m_buffer.empty() && (need_flush || m_buffer_time_size - m_buffer.front().time > nb_seconds_delay - EPSILON) ){
            BufferData& frontdata = m_buffer.front();
            if (frontdata.fan_speed < 0 || frontdata.fan_speed != m_front_buffer_fan_speed || frontdata.is_kickstart) {
                if (frontdata.is_kickstart && frontdata.fan_speed < m_front_buffer_fan_speed) {
                    //你必须减速！不是kickstart！重写风扇速度。
                    m_process_output += _set_fan(frontdata.fan_speed);//m_writer.set_fan(frontdata.fan_speed,true); //FIXME extruder id (or use the gcode writer, but then you have to disable the multi-thread thing

                    m_front_buffer_fan_speed = frontdata.fan_speed;
                } else {
                    m_process_output += frontdata.raw + "\n";
                    if (frontdata.fan_speed >= 0) {
                        //注意这是唯一一个我们设置风扇速度并从缓冲区打印的地方，因为如果fan_speed >= 0 => time == 0
                        //并且这会刷新队列尾部的所有time == 0的行...
                        m_front_buffer_fan_speed = frontdata.fan_speed;
                    }
                }
            }
            remove_from_buffer(m_buffer.begin());
        }
    }
    double sum = 0;
    for (auto& data : m_buffer) sum += data.time;
    assert( std::abs(m_buffer_time_size - sum) < 0.01);
}

} // namespace Slic3r
