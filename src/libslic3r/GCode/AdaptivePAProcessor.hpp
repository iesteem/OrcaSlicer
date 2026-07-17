// AdaptivePAProcessor.hpp
// Snapmaker_Orca
//
// AdaptivePAProcessor类的头文件，负责处理G-code层以应用自适应压力提前。

#ifndef ADAPTIVEPAPROCESSOR_H
#define ADAPTIVEPAPROCESSOR_H

#include <string>
#include <sstream>
#include <regex>
#include <memory>
#include <map>
#include <vector>
#include "AdaptivePAInterpolator.hpp"

namespace Slic3r {

// GCode类的前向声明
class GCode;

/**
 * @brief 用于处理带有自适应压力提前的G-code层的类。
 */
class AdaptivePAProcessor {
public:
    /**
     * @brief AdaptivePAProcessor的构造函数。
     *
     * 此构造函数使用GCode对象的引用初始化AdaptivePAProcessor。
     * 它还初始化配置引用、压力提前插值对象
     * 以及用于处理G-code的正则表达式模式。
     *
     * @param gcodegen 生成G-code的GCode对象引用。
     */
    AdaptivePAProcessor(GCode &gcodegen, const std::vector<unsigned int> &tools_used);

    /**
     * @brief 处理一层G-code并应用自适应压力提前。
     *
     * 此方法处理单层的G-code，识别适当的
     * 压力提前设置并基于当前状态和配置应用它们。
     *
     * @param gcode 包含该层G-code的字符串。
     * @return 包含已应用自适应压力提前的处理后G-code的字符串。
     */
    std::string process_layer(std::string &&gcode);

    /**
     * @brief 手动设置自适应PA内部值。
     *
     * 此方法手动设置自适应PA内部持有的值。
     * 在更换工具或任何其他内部假设的上次PA值可能不正确的
     * 情况下调用此方法。
     */
    void resetPreviousPA(double PA){ m_last_predicted_pa = PA; };

private:
    GCode &m_gcodegen; ///< GCode对象的引用。
    std::unordered_map<unsigned int, std::unique_ptr<AdaptivePAInterpolator>> m_AdaptivePAInterpolators; ///< 插值器对象与工具ID之间的映射
    const PrintConfig &m_config; ///< 打印配置的引用。
    double m_last_predicted_pa; ///< 上次预测的压力提前值。
    double m_max_next_feedrate; ///< 即将到来岛的最大进给率(速度)。如果未找到速度，则使用上一个岛的速度。
    double m_next_feedrate; ///< 即将到来岛的第一个进给率(速度)。
    double m_current_feedrate; ///< 当前的最近进给率。
    int m_last_extruder_id; ///< 上次使用的挤出机ID。

    std::regex m_pa_change_pattern; ///< 检测PA_CHANGE模式的正则表达式。
    std::regex m_g1_f_pattern; ///< 检测G1 F模式的正则表达式。
    std::smatch m_match; ///< 正则表达式的匹配结果。

    /**
     * @brief 获取附加到指定工具ID的PA插值器。
     *
     * 此方法手动设置自适应PA内部持有的值。
     * 在更换工具或任何其他内部假设的上次PA值可能不正确的
     * 情况下调用此方法。
     *
     * @param 要返回其PA插值模型的工具ID的整数。
     * @return 对应于该工具的Adaptive PA Interpolator对象。
     */
    AdaptivePAInterpolator* getInterpolator(unsigned int tool_id);
};

} // namespace Slic3r

#endif // ADAPTIVEPAPROCESSOR_H
