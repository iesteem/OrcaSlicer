// AdaptivePAProcessor.cpp
// Snapmaker_Orca
//
// AdaptivePAProcessor类的实现，负责处理带有自适应压力提前的G-code层。

#include "../GCode.hpp"
#include "AdaptivePAProcessor.hpp"
#include <sstream>
#include <iostream>
#include <cmath>

namespace Slic3r {

/**
 * @brief AdaptivePAProcessor的构造函数。
 *
 * 此构造函数使用GCode对象的引用初始化AdaptivePAProcessor。
 * 它还初始化配置引用、压力提前插值对象
 * 以及用于处理G-code的正则表达式模式。
 *
 * @param gcodegen 生成G-code的GCode对象引用。
 */
AdaptivePAProcessor::AdaptivePAProcessor(GCode &gcodegen, const std::vector<unsigned int> &tools_used)
    : m_gcodegen(gcodegen),
      m_config(gcodegen.config()),
      m_last_predicted_pa(0.0),
      m_max_next_feedrate(0.0),
      m_next_feedrate(0.0),
      m_current_feedrate(0.0),
      m_last_extruder_id(-1),
      m_pa_change_pattern(R"(; PA_CHANGE:T(\d+) MM3MM:([0-9]*\.[0-9]+) ACCEL:(\d+) BR:(\d+) RC:(\d+) OV:(\d+))"),
      m_g1_f_pattern(R"(G1 F([0-9]+))")
{
    // 构造函数体可用于进一步的初始化（如有必要）
    for (unsigned int tool : tools_used) {
        // 仅当PA和自适应PA选项都启用时才为该工具启用模型
        if(m_config.adaptive_pressure_advance.get_at(tool) && m_config.enable_pressure_advance.get_at(tool)){
            auto interpolator = std::make_unique<AdaptivePAInterpolator>();
            // 从挤出机获取校准值
            std::string pa_calibration_values = m_config.adaptive_pressure_advance_model.get_at(tool);
            // 设置模型并将其存储在工具-插值模型映射中
            interpolator->parseAndSetData(pa_calibration_values);
            m_AdaptivePAInterpolators[tool] = std::move(interpolator);
        }
    }
}

// 获取特定工具ID的插值器的方法
AdaptivePAInterpolator* AdaptivePAProcessor::getInterpolator(unsigned int tool_id) {
    auto it = m_AdaptivePAInterpolators.find(tool_id);
    if (it != m_AdaptivePAInterpolators.end()) {
        return it->second.get();
    }
    return nullptr;  // 处理未找到tool_id的情况
}

/**
 * @brief 处理一层G-code并应用自适应压力提前。
 *
 * 此方法处理单层的G-code，识别适当的
 * 压力提前设置并基于当前状态和配置应用它们。
 *
 * @param gcode 包含该层G-code的字符串。
 * @return 包含已应用自适应压力提前的处理后G-code的字符串。
 */
std::string AdaptivePAProcessor::process_layer(std::string &&gcode) {
    std::istringstream stream(gcode);
    std::string line;
    std::ostringstream output;
    double mm3mm_value = 0.0;
    unsigned int accel_value = 0;
    std::string pa_change_line;
    bool wipe_command = false;

    // 遍历该层G-code的每一行
    while (std::getline(stream, line)) {

        // 如果找到擦拭开始命令，忽略所有速度变化直到找到擦拭结束部分
        if (line.find("WIPE_START") != std::string::npos) {
            wipe_command = true;
        }

        // 更新当前进给率（这位于挤出或擦拭命令之前）。忽略擦拭移动期间发出的任何速度变化。
        // 移动进给率作为G1 X Y (Z) F命令的一部分输出
        if ( (line.find("G1 F") == 0) && (!wipe_command) ) { // 在运行正则匹配前快速过滤行
            std::size_t pos = line.find('F');
            if (pos != std::string::npos){
                m_current_feedrate = std::stod(line.substr(pos + 1)) / 60.0; // 从mm/min转换为mm/s
            }
        }

        // 找到擦拭结束，继续搜索当前进给率。
        if (line.find("WIPE_END") != std::string::npos) {
            wipe_command = false;
        }

        // 将下一个进给率重置为零，以便在PA更改标签后
        // 搜索遇到的第一个进给率变化命令。
        m_next_feedrate = 0;

        // 检查行中是否有PA_CHANGE模式
        // 我们只会为启用了自适应PA的挤出机找到此模式。
        // 如果层中有混合的挤出机（即自适应PA打开和关闭），
        // 这只会更新启用了自适应PA的挤出机，
        // 因为只有这些挤出机才会输出PA模式。
        // 对于启用了和禁用了自适应PA的混合挤出机层，当选择新工具时，
        // 会设置该材料的PA。由于下面找不到该挤出机的标签，将保留原始PA。
        if (line.find("; PA_CHANGE") == 0) { // 在运行更昂贵的正则检查前快速过滤行
            if (std::regex_search(line, m_match, m_pa_change_pattern)) {
                int extruder_id = std::stoi(m_match[1].str());
                mm3mm_value = std::stod(m_match[2].str());
                accel_value = std::stod(m_match[3].str());
                int isBridge = std::stoi(m_match[4].str());
                int roleChange = std::stoi(m_match[5].str());
                int isOverhang = std::stoi(m_match[6].str());

                // 检查挤出机ID是否已更改
                bool extruder_changed = (extruder_id != m_last_extruder_id);
                m_last_extruder_id = extruder_id;

                // 保存PA_CHANGE行以便在找到进给率后输出
                pa_change_line = line;

                // 在包含G和E命令的任何行之前向前查找进给率
                std::streampos current_pos = stream.tellg();
                std::string next_line;
                double temp_feed_rate = 0;
                bool extrude_move_found = false;
                int line_counter = 0;

                // 在层的G-code行上继续搜索以找到打印速度
                // 如果找到G1 Fxxxx模式，则识别新速度
                // 继续搜索进给率以找到最大打印速度
                // 直到检测到特征变化模式或擦拭命令
                while (std::getline(stream, next_line)) {
                    line_counter++;
                    // 找到挤出移动，设置挤出移动找到标志并移动到下一行
                    if ((!extrude_move_found) && next_line.find("G1 ") == 0 &&
                        next_line.find('X') != std::string::npos &&
                        next_line.find('Y') != std::string::npos &&
                        next_line.find('E') != std::string::npos) {
                        // 模式匹配，跳出循环
                        extrude_move_found = true;
                        continue;
                    }

                    // 在找到至少一个挤出移动后找到移动移动
                    // 现在需要停止搜索速度，因为我们完成了此岛的打印
                    if (next_line.find("G1 ") == 0 &&
                        next_line.find('X') != std::string::npos && // X存在
                        next_line.find('Y') != std::string::npos && // Y存在
                        next_line.find('E') == std::string::npos && // 没有"E"
                        extrude_move_found) {                       // 已经发生了挤出移动
                        // 找到挤出移动后的第一个移动移动。停止搜索。
                        break;
                    }

                    // 找到WIPE命令
                    // 如果有擦拭命令，通常擦拭速度与最大打印速度不同（更大）
                    // 因此如果找到擦拭命令就停止搜索，因为我们不想
                    // 用擦拭速度覆盖用于PA计算的速度。
                    if (next_line.find("WIPE") != std::string::npos) {
                        break; // 如果找到擦拭命令则停止搜索
                    }

                    // 找到另一个PA_CHANGE模式
                    // 如果RC=1，表示有角色变化，所以停止尝试为该特征找到最大速度。
                    // 这可能有些冗余，因为新特征之前总会有移动移动，
                    // 但还是检查一下。不过最后检查以免无理由调用它...
                    if (next_line.find("; PA_CHANGE") == 0) { // 在运行模式匹配前快速过滤行
                        std::size_t rc_pos = next_line.rfind("RC:");
                        if (rc_pos != std::string::npos) {
                            int rc_value = std::stoi(next_line.substr(rc_pos + 3));
                            if (rc_value == 1) {
                                break; // 找到角色变化，停止搜索
                            }
                        }
                    }

                    // 找到进给率变化命令
                    // 如果新进给率大于PA更改命令后遇到的任何进给率，则使用它来计算PA值
                    // 如果这是我们遇到的第一个进给率，将其存储为下一个进给率。
                    if (next_line.find("G1 F") == 0) { // 在运行模式匹配前快速过滤行
                        std::size_t pos = next_line.find('F');
                        if (pos != std::string::npos) {
                            double feedrate = std::stod(next_line.substr(pos + 1)) / 60.0; // 从mm/min转换为mm/s
                            if(line_counter==1){ // 这是PA更改模式后的第一个命令，因此在任何挤出之前。将当前速度重置为此速度
                                m_current_feedrate = feedrate;
                            }
                            if (temp_feed_rate < feedrate) {
                                temp_feed_rate = feedrate;
                            }
                            if(m_next_feedrate < EPSILON){ // 这是PA更改命令后找到的第一个进给率
                                m_next_feedrate = feedrate;
                            }
                        }
                        continue;
                    }
                }

                // 如果在PA更改命令后找到新的最大进给率，使用它
                if (temp_feed_rate > 0) {
                    m_max_next_feedrate = temp_feed_rate;
                } else // 如果在PA更改命令后根本没有找到新的进给率，使用当前进给率。
                    m_max_next_feedrate = m_current_feedrate;

                // 恢复流位置
                stream.clear();
                stream.seekg(current_pos);

                // 使用即将到来的特征的最大进给率计算预测PA
                // 获取活动工具的插值器
                AdaptivePAInterpolator* interpolator = getInterpolator(m_last_extruder_id);

                double predicted_pa = 0;
                double adaptive_PA_speed = 0;

                if(!interpolator){ // 在插值器映射中未找到工具
                    // 工具未在PA插值器到工具映射中找到
                    predicted_pa = m_config.enable_pressure_advance.get_at(m_last_extruder_id) ? m_config.pressure_advance.get_at(m_last_extruder_id) : 0;
                    if(m_config.gcode_comments) output << "; APA: 工具没有启用APA\n";
                } else if (!interpolator->isInitialised() || (!m_config.adaptive_pressure_advance.get_at(m_last_extruder_id)) )
                    // 检查模型是否未被活动挤出机的构造函数初始化
                    // 也检查自适应PA是否已为该挤出机启用。这应该不需要，
                    // 因为如果自适应PA被禁用，PA更改标志不应该在上游（在GCode.cpp文件中）设置，
                    // 但为了健壮性还是检查一下。
                {
                    // 模型失败或自适应压力提前未启用 - 使用m_config中的默认值
                    predicted_pa = m_config.enable_pressure_advance.get_at(m_last_extruder_id) ? m_config.pressure_advance.get_at(m_last_extruder_id) : 0;
                    if(m_config.gcode_comments) output << "; APA: 插值器设置失败，使用默认压力提前\n";
                } else { // 模型设置成功
                    // 继续识别用于计算自适应PA值的打印速度
                    if(isOverhang > 0){  // 如果我们在悬垂区域，使用当前打印速度和
                                        // 之后任何速度中的最小值
                                        // 在大多数情况下，当前速度是最小值；
                                        // 但是如果启用层冷却减速，悬垂
                                        // 可能比当前速度减速更多。
                        adaptive_PA_speed = (m_current_feedrate == 0 || m_next_feedrate == 0) ?
                                                std::max(m_current_feedrate, m_next_feedrate) :
                                                std::min(m_current_feedrate, m_next_feedrate);
                    }else{                // 如果不是悬垂区域，使用当前速度和
                                          // 即将到来的岛的速度中的最大值。
                        adaptive_PA_speed = std::max(m_max_next_feedrate,m_current_feedrate);
                    }

                    // 计算自适应PA值
                    predicted_pa = (*interpolator)(mm3mm_value * adaptive_PA_speed, accel_value);

                    // 这是桥接，使用专用的PA设置。
                    if(isBridge && m_config.adaptive_pressure_advance_bridges.get_at(m_last_extruder_id) > EPSILON)
                        predicted_pa = m_config.adaptive_pressure_advance_bridges.get_at(m_last_extruder_id);

                    if (predicted_pa < 0) { // 如果外推失败，回退到挤出机的默认PA。
                        predicted_pa = m_config.enable_pressure_advance.get_at(m_last_extruder_id) ? m_config.pressure_advance.get_at(m_last_extruder_id) : 0;
                        if(m_config.gcode_comments) output << "; APA: 插值失败，使用回退压力提前值\n";
                    }
                }
                if(m_config.gcode_comments) {
                    // 输出调试G-code注释
                    output << pa_change_line << '\n'; // 输出PA更改命令标签
                    if(isBridge && m_config.adaptive_pressure_advance_bridges.get_at(m_last_extruder_id) > EPSILON)
                        output << "; APA 模型覆盖（桥接）\n";
                    output << "; APA 当前速度: " << std::to_string(m_current_feedrate) << "\n";
                    output << "; APA 下一个速度: " << std::to_string(m_next_feedrate) << "\n";
                    output << "; APA 最大下一个速度: " << std::to_string(m_max_next_feedrate) << "\n";
                    output << "; APA 使用的速度: " << std::to_string(adaptive_PA_speed) << "\n";
                    output << "; APA 流量: " << std::to_string(mm3mm_value * m_max_next_feedrate) << "\n";
                    output << "; APA 上一个PA: " << std::to_string(m_last_predicted_pa) << " 新PA: " << std::to_string(predicted_pa) << "\n";
                }
                if (extruder_changed || std::fabs(predicted_pa - m_last_predicted_pa) > EPSILON) {
                    output << m_gcodegen.writer().set_pressure_advance(predicted_pa); // 使用m_writer设置压力提前
                    m_last_predicted_pa = predicted_pa; // 更新上次预测的PA值
                }
            }
        }else {
            // 输出当前行，因为这不是PA更改标签
            output << line << '\n';
        }
    }

    return output.str();
}

} // namespace Slic3r
