#ifndef SRC_LIBSLIC3R_GEOMETRY_CURVES_HPP_
#define SRC_LIBSLIC3R_GEOMETRY_CURVES_HPP_

#include "libslic3r/Point.hpp"
#include "Bicubic.hpp"

#include <iostream>

//#define LSQR_DEBUG

namespace Slic3r {
namespace Geometry {

template<int Dimension, typename NumberType>
struct PolynomialCurve {
    Eigen::MatrixXf coefficients;

    Vec<Dimension, NumberType> get_fitted_value(const NumberType& value) const {
        Vec<Dimension, NumberType> result = Vec<Dimension, NumberType>::Zero();
        size_t order = this->coefficients.rows() - 1;
        auto x = NumberType(1.);
        for (size_t index = 0; index < order + 1; ++index, x *= value)
            result += x * this->coefficients.col(index);
        return result;
    }
};

//https://towardsdatascience.com/least-square-polynomial-CURVES-using-c-eigen-package-c0673728bd01
template<int Dimension, typename NumberType>
PolynomialCurve<Dimension, NumberType> fit_polynomial(const std::vector<Vec<Dimension, NumberType>> &observations,
        const std::vector<NumberType> &observation_points,
        const std::vector<NumberType> &weights, size_t order) {
    // 检查以确保输入正确
    size_t cols = order + 1;
    assert(observation_points.size() >= cols);
    assert(observation_points.size() == weights.size());
    assert(observations.size() == weights.size());

    Eigen::MatrixXf data_points(Dimension, observations.size());
    Eigen::MatrixXf T(observations.size(), cols);
    for (size_t i = 0; i < weights.size(); ++i) {
        auto squared_weight = sqrt(weights[i]);
        data_points.col(i) = observations[i] * squared_weight;
        // 填充矩阵
        auto x = squared_weight;
        auto c = observation_points[i];
        for (size_t j = 0; j < cols; ++j, x *= c)
            T(i, j) = x;
    }

    const auto QR = T.householderQr();
    Eigen::MatrixXf coefficients(Dimension, cols);
    // 求解线性最小二乘拟合
    for (size_t dim = 0; dim < Dimension; ++dim) {
        coefficients.row(dim) = QR.solve(data_points.row(dim).transpose());
    }

    return {std::move(coefficients)};
}

template<size_t Dimension, typename NumberType, typename KernelType>
struct PiecewiseFittedCurve {
    using Kernel = KernelType;

    Eigen::MatrixXf coefficients;
    NumberType start;
    NumberType segment_size;
    size_t endpoints_level_of_freedom;

    Vec<Dimension, NumberType> get_fitted_value(const NumberType &observation_point) const {
        Vec<Dimension, NumberType> result = Vec<Dimension, NumberType>::Zero();

        // 查找对应的段索引；期望核居中
        int middle_right_segment_index = floor((observation_point - start) / segment_size);
        // 查找受点i影响的第一个段的索引；这可以从kernel_span推导出来
        int start_segment_idx = middle_right_segment_index - Kernel::kernel_span / 2 + 1;
        for (int segment_index = start_segment_idx; segment_index < int(start_segment_idx + Kernel::kernel_span);
                segment_index++) {
            NumberType segment_start = start + segment_index * segment_size;
            NumberType normalized_segment_distance = (segment_start - observation_point) / segment_size;

            int parameter_index = segment_index + endpoints_level_of_freedom;
            parameter_index = std::clamp(parameter_index, 0, int(coefficients.cols()) - 1);
            result += Kernel::kernel(normalized_segment_distance) * coefficients.col(parameter_index);
        }
        return result;
    }
};

// observations: 待曲线拟合的数据
// observation points: 观测点的递增序列
//      换句话说，对于函数f(x) = y，observations是y0...yn，observation points是x0...xn
// weights: 观测的重要性
// segments_count: 曲线有效长度内的段数
// endpoints_level_of_freedom: 每端额外参数的数量；合理值取决于核跨度
template<typename Kernel, int Dimension, typename NumberType>
PiecewiseFittedCurve<Dimension, NumberType, Kernel> fit_curve(
        const std::vector<Vec<Dimension, NumberType>> &observations,
        const std::vector<NumberType> &observation_points,
        const std::vector<NumberType> &weights,
        size_t segments_count,
        size_t endpoints_level_of_freedom) {

    // 检查以确保输入正确
    assert(segments_count > 0);
    assert(observations.size() > 0);
    assert(observation_points.size() == observations.size());
    assert(observation_points.size() == weights.size());
    assert(segments_count <= observations.size());

    // 准备权重的平方根，然后将其应用于矩阵T和观测数据：https://en.wikipedia.org/wiki/Weighted_least_squares
    std::vector<NumberType> sqrt_weights(weights.size());
    for (size_t index = 0; index < weights.size(); ++index) {
        assert(weights[index] > 0);
        sqrt_weights[index] = sqrt(weights[index]);
    }

    // 准备结果并计算元数据
    PiecewiseFittedCurve<Dimension, NumberType, Kernel> result { };

    NumberType valid_length = observation_points.back() - observation_points.front();
    NumberType segment_size = valid_length / NumberType(segments_count);
    result.start = observation_points.front();
    result.segment_size = segment_size;
    result.endpoints_level_of_freedom = endpoints_level_of_freedom;

    // 准备观测数据
    // Eigen默认使用列主序内存布局。
    Eigen::MatrixXf data_points(Dimension, observations.size());
    for (size_t index = 0; index < observations.size(); ++index) {
        data_points.col(index) = observations[index] * sqrt_weights[index];
    }
    // 参数数量始终增加1，以使曲线的参数空间对称。
    // 如果没有这个修正，曲线的末端灵活性不如前端
    size_t parameters_count = segments_count + 1 + 2 * endpoints_level_of_freedom;
    // 为每个点和每个段创建权重矩阵T；
    Eigen::MatrixXf T(observation_points.size(), parameters_count);
    T.setZero();
    // 填充权重矩阵
    for (size_t i = 0; i < observation_points.size(); ++i) {
        NumberType observation_point = observation_points[i];
        // 查找对应的段索引；期望核居中
        int middle_right_segment_index = floor((observation_point - result.start) / result.segment_size);
        // 查找受点i影响的第一个段的索引；这可以从kernel_span推导出来
        int start_segment_idx = middle_right_segment_index - int(Kernel::kernel_span / 2) + 1;
        for (int segment_index = start_segment_idx; segment_index < int(start_segment_idx + Kernel::kernel_span);
                segment_index++) {
            NumberType segment_start = result.start + segment_index * result.segment_size;
            NumberType normalized_segment_distance = (segment_start - observation_point) / result.segment_size;

            int parameter_index = segment_index + endpoints_level_of_freedom;
            parameter_index = std::clamp(parameter_index, 0, int(parameters_count) - 1);
            T(i, parameter_index) += Kernel::kernel(normalized_segment_distance) * sqrt_weights[i];
        }
    }

#ifdef LSQR_DEBUG
    std::cout << "weight matrix: " << std::endl;
    for (int obs = 0; obs < observation_points.size(); ++obs) {
        std::cout << std::endl;
        for (int segment = 0; segment < parameters_count; ++segment) {
            std::cout << T(obs, segment) << "  ";
        }
    }
    std::cout << std::endl;
#endif

    // 求解线性最小二乘拟合
    result.coefficients.resize(Dimension, parameters_count);
    const auto QR = T.fullPivHouseholderQr();
    for (size_t dim = 0; dim < Dimension; ++dim) {
        result.coefficients.row(dim) = QR.solve(data_points.row(dim).transpose());
    }

    return result;
}


template<int Dimension, typename NumberType>
PiecewiseFittedCurve<Dimension, NumberType, LinearKernel<NumberType>>
fit_linear_spline(
        const std::vector<Vec<Dimension, NumberType>> &observations,
        std::vector<NumberType> observation_points,
        std::vector<NumberType> weights,
        size_t segments_count,
        size_t endpoints_level_of_freedom = 0) {
    return fit_curve<LinearKernel<NumberType>>(observations, observation_points, weights, segments_count,
            endpoints_level_of_freedom);
}

template<int Dimension, typename NumberType>
PiecewiseFittedCurve<Dimension, NumberType, CubicBSplineKernel<NumberType>>
fit_cubic_bspline(
        const std::vector<Vec<Dimension, NumberType>> &observations,
        std::vector<NumberType> observation_points,
        std::vector<NumberType> weights,
        size_t segments_count,
        size_t endpoints_level_of_freedom = 0) {
    return fit_curve<CubicBSplineKernel<NumberType>>(observations, observation_points, weights, segments_count,
            endpoints_level_of_freedom);
}

template<int Dimension, typename NumberType>
PiecewiseFittedCurve<Dimension, NumberType, CubicCatmulRomKernel<NumberType>>
fit_catmul_rom_spline(
        const std::vector<Vec<Dimension, NumberType>> &observations,
        std::vector<NumberType> observation_points,
        std::vector<NumberType> weights,
        size_t segments_count,
        size_t endpoints_level_of_freedom = 0) {
    return fit_curve<CubicCatmulRomKernel<NumberType>>(observations, observation_points, weights, segments_count,
            endpoints_level_of_freedom);
}

}
}

#endif /* SRC_LIBSLIC3R_GEOMETRY_CURVES_HPP_ */
