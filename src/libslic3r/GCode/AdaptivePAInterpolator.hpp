// AdaptivePAInterpolator.hpp
// Snapmaker_Orca
//
// AdaptivePAInterpolator类的头文件，负责使用PCHIP插值基于流量和加速度插值压力提前(PA)值。

#ifndef ADAPTIVEPAINTERPOLATOR_HPP
#define ADAPTIVEPAINTERPOLATOR_HPP

#include <vector>
#include <string>
#include <map>
#include "PchipInterpolatorHelper.hpp"

/**
 * @class AdaptivePAInterpolator
 * @brief 一个基于流量和加速度使用分段三次埃尔米特插值多项式(PCHIP)插值压力提前(PA)值的类。
 */
class AdaptivePAInterpolator {
public:
    /**
     * @brief 默认构造函数。
     */
    AdaptivePAInterpolator() : m_isInitialised(false) {}

    /**
     * @brief 解析输入数据并设置插值器。
     * @param data 包含CSV格式数据的字符串(PA, 流量, 加速度)。
     * @return 成功返回0，错误返回-1。
     */
    int parseAndSetData(const std::string& data);

    /**
     * @brief 根据给定的流量和加速度插值PA值。
     * @param flow_rate 插值时的流量。
     * @param acceleration 插值时的加速度。
     * @return 插值后的PA值，如果插值失败则返回-1。
     */
    double operator()(double flow_rate, double acceleration);

    /**
     * @brief 返回初始化状态。
     * @return m_isInitialised的值。
     */
    bool isInitialised() const {
        return m_isInitialised;
    }

private:
    std::map<double, PchipInterpolatorHelper> flow_interpolators_; ///< 将每个加速度映射到流量-PA插值器。
    std::vector<double> accelerations_; ///< 存储唯一的加速度值。
    bool m_isInitialised;
};

#endif // ADAPTIVEPAINTERPOLATOR_HPP
