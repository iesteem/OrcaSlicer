// AdaptivePAInterpolator.cpp
// Snapmaker_Orca
//
// AdaptivePAInterpolator类的实现文件，提供解析数据和执行PA插值的方法。

#include "AdaptivePAInterpolator.hpp"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <sstream>

/**
 * @brief 解析输入数据并设置插值器。
 * @param data 包含CSV格式数据的字符串(PA, 流量, 加速度)。
 * @return 成功返回0，错误返回-1。
 */
int AdaptivePAInterpolator::parseAndSetData(const std::string& data) {
    flow_interpolators_.clear();
    accelerations_.clear();

    try {
        std::istringstream ss(data);
        std::string line;
        std::map<double, std::vector<std::pair<double, double>>> acc_to_flow_pa;

        while (std::getline(ss, line)) {
            std::istringstream lineStream(line);
            std::string value;
            double paValue, flowRate, acceleration;
            paValue = flowRate = acceleration = 0.f; // 全部初始化为零。

            // 解析PA值
            if (std::getline(lineStream, value, ',')) {
                paValue = std::stod(value);
            }

            // 解析流量值
            if (std::getline(lineStream, value, ',')) {
                flowRate = std::stod(value);
            }

            // 解析加速度值
            if (std::getline(lineStream, value, ',')) {
                acceleration = std::stod(value);
            }

            // 将解析后的值存储在以加速度为键的映射中
            acc_to_flow_pa[acceleration].emplace_back(flowRate, paValue);
        }

        // 遍历映射以设置插值器
        for (const auto& kv : acc_to_flow_pa) {
            double acceleration = kv.first;
            const auto& data = kv.second;

            std::vector<double> flowRates;
            std::vector<double> paValues;

            for (const auto& pair : data) {
                flowRates.push_back(pair.first);
                paValues.push_back(pair.second);
            }

            // 只有在数据点足够时才设置插值器
            if (flowRates.size() > 1) {
                PchipInterpolatorHelper interpolator(flowRates, paValues);
                flow_interpolators_[acceleration] = interpolator;
                accelerations_.push_back(acceleration);
            }
        }
    } catch (const std::exception&) {
        m_isInitialised = false;
        return -1; // 错误：解析时出现异常
    }
    m_isInitialised = true;
    return 0; // 成功
}

/**
 * @brief 根据给定的流量和加速度插值PA值。
 * @param flow_rate 插值时的流量。
 * @param acceleration 插值时的加速度。
 * @return 插值后的PA值，如果插值失败则返回-1。
 */
double AdaptivePAInterpolator::operator()(double flow_rate, double acceleration) {
    std::vector<double> pa_values;
    std::vector<double> acc_values;

    // 对每个流量到PA模型估计给定流量的PA值
    for (const auto& kv : flow_interpolators_) {
        double pa_value = kv.second.interpolate(flow_rate);

        // 检查插值后的PA值是否有效
        if (pa_value != -1) {
            pa_values.push_back(pa_value);
            acc_values.push_back(kv.first);
        }
    }

    // 检查是否有足够的加速度值进行插值
    if (acc_values.size() < 2) {
        // 特殊情况：只有一个加速度值
        if (acc_values.size() == 1) {
            return std::round(pa_values[0] * 1000.0) / 1000.0; // 四舍五入到3位小数
        }
        return -1; // 错误：没有足够的数据点进行插值
    }

    // 创建用于PA-加速度插值的PchipInterpolatorHelper
    // 使用上面循环中估计的PA值及其对应的加速度来
    // 生成新的PCHIP模型。然后运行该模型来插值给定加速度值的PA值。
    PchipInterpolatorHelper pa_accel_interpolator(acc_values, pa_values);
    return std::round(pa_accel_interpolator.interpolate(acceleration) * 1000.0) / 1000.0; // 四舍五入到3位小数
}
