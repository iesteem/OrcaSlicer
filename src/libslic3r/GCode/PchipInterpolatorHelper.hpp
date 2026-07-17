// PchipInterpolatorHelper.hpp
// Snapmaker_Orca
//
// PchipInterpolatorHelper类的头文件，负责对给定数据点执行分段三次埃尔米特插值多项式(PCHIP)插值。

#ifndef PCHIPINTERPOLATORHELPER_HPP
#define PCHIPINTERPOLATORHELPER_HPP

#include <vector>

/**
 * @class PchipInterpolatorHelper
 * @brief 一个辅助类，用于执行分段三次埃尔米特插值多项式(PCHIP)插值。
 */
class PchipInterpolatorHelper {
public:
    /**
     * @brief 默认构造函数。
     */
    PchipInterpolatorHelper() = default;

    /**
     * @brief 使用给定数据点构造PCHIP插值器。
     * @param x 数据点的x坐标。
     * @param y 数据点的y坐标。
     */
    PchipInterpolatorHelper(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * @brief 为插值器设置数据点。
     * @param x 数据点的x坐标。
     * @param y 数据点的y坐标。
     * @throw std::invalid_argument 如果x和y大小不同或包含少于两个点。
     */
    void setData(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * @brief 在给定点处插值。
     * @param xi 要插值的x坐标。
     * @return 插值后的y坐标。
     */
    double interpolate(double xi) const;

private:
    std::vector<double> x_; ///< 数据点的x坐标。
    std::vector<double> y_; ///< 数据点的y坐标。
    std::vector<double> h_; ///< 连续x坐标之间的差值。
    std::vector<double> delta_; ///< 连续数据点之间的线段斜率。
    std::vector<double> d_; ///< 数据点处的导数。

    /**
     * @brief 计算PCHIP系数。
     */
    void computePCHIP();

    /**
     * @brief 按x坐标对数据点排序。
     */
    void sortData();

    /**
     * @brief 计算连续x坐标之间的差值。
     * @param i x坐标的索引。
     * @return x_[i+1]和x_[i]之间的差值。
     */
    double h(int i) const { return x_[i+1] - x_[i]; }

    /**
     * @brief 计算连续数据点之间线段的斜率。
     * @param i 线段的索引。
     * @return y_[i]和y_[i+1]之间线段的斜率。
     */
    double delta(int i) const { return (y_[i+1] - y_[i]) / h(i); }
};

#endif // PCHIPINTERPOLATORHELPER_HPP
