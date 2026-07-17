#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include <utility>
#include <tuple>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <cassert>
#include <optional>

namespace Slic3r { namespace opt {

//// 用于保存优化完成结果的类型。
template<size_t N> struct Result {
    int resultcode;     // 方法依赖
    std::array<double, N> optimum;
    double score;
};

//// 优化的一个可能输入值区间（interval）
class Bound {
    double m_min, m_max;

public:
    Bound(double min = std::numeric_limits<double>::min(),
          double max = std::numeric_limits<double>::max())
        : m_min(min), m_max(max)
    {}

    double min() const noexcept { return m_min; }
    double max() const noexcept { return m_max; }
};

//// 优化的辅助类型：函数输入和边界
template<size_t N> using Input = std::array<double, N>;
template<size_t N> using Bounds = std::array<Bound, N>;

//// 用于指定停止标准的类型。Setter 方法可以串联。
class StopCriteria {

    //// 如果两个分数之间的绝对值差异。
    double m_abs_score_diff = std::nan("");

    //// 如果两个分数之间的相对值差异。
    double m_rel_score_diff = std::nan("");

    //// 如果找到此值或更优值，则停止。
    double m_stop_score = std::nan("");

    //// 一个谓词，如果评估为 true，则优化应终止
    // 并返回终止前找到的最佳结果。
    std::function<bool()> m_stop_condition = [] { return false; };

    //// 允许的最大迭代次数。
    unsigned m_max_iterations = 0;

public:

    StopCriteria & abs_score_diff(double val)
    {
        m_abs_score_diff = val; return *this;
    }

    double abs_score_diff() const { return m_abs_score_diff; }

    StopCriteria & rel_score_diff(double val)
    {
        m_rel_score_diff = val; return *this;
    }

    double rel_score_diff() const { return m_rel_score_diff; }

    StopCriteria & stop_score(double val)
    {
        m_stop_score = val; return *this;
    }

    double stop_score() const { return m_stop_score; }

    StopCriteria & max_iterations(double val)
    {
        m_max_iterations = val; return *this;
    }

    double max_iterations() const { return m_max_iterations; }

    template<class Fn> StopCriteria & stop_condition(Fn &&cond)
    {
        m_stop_condition = cond; return *this;
    }

    bool stop_condition() { return m_stop_condition(); }
};

//// 用于使用涉及梯度的优化方法的辅助类。
template<size_t N> struct ScoreGradient {
    double score;
    std::optional<std::array<double, N>> gradient;

    ScoreGradient(double s, const std::array<double, N> &grad)
        : score{s}, gradient{grad}
    {}
};

//// 用于在 static_assert 中使用的辅助类。
template<class T> struct always_false { enum { value = false }; };

//// 优化器对象的基本接口
template<class Method, class Enable = void> class Optimizer {
public:

    Optimizer(const StopCriteria &)
    {
        static_assert (always_false<Method>::value,
                       "给定方法的优化器未实现！");
    }

    //// 切换优化为函数最小化
    Optimizer &to_min() { return *this; }

    //// 切换优化为函数最大化
    Optimizer &to_max() { return *this; }

    //// 为连续优化设置标准
    Optimizer &set_criteria(const StopCriteria &) { return *this; }

    //// 获取当前标准
    StopCriteria get_criteria() const { return {}; };

    //// 找到函数 Func 的最小值或最大值，其具有签名：
    //// double(const 输入<N> &输入) 且维度为 N
    //
    //// 初始起始点可以作为第二个参数给出。
    //
    //// 对于每个维度，必须给定一个区间（边界），标记该维度的边界。
    //
    // initvals 必须在指定的 bounds 内，否则行为未定义。
    //
    //// Func 可以返回 double 类型的分数，或者可选地返回 ScoreGradient
    // 类以指示函数的梯度，适用于使用梯度的优化方法。
    template<class Func, size_t N>
    Result<N> optimize(Func&& /*func*/,
                       const Input<N> &/*initvals*/,
                       const Bounds<N>& /*bounds*/) { return {}; }

    //// 随机方法的可选接口：
    void seed(long /*s*/) {}
};

namespace detail {

//// 辅助函数：将 C 风格数组转换为 std::array。在现代编译器中，此复制应被优化掉。
template<size_t N, class T> auto to_arr(const T *a)
{
    std::array<T, N> r;
    std::copy(a, a + N, std::begin(r));
    return r;
}

template<size_t N, class T> auto to_arr(const T (&a) [N])
{
    return to_arr<N>(static_cast<const T *>(a));
}

} // namespace detail

//// 创建边界、初始值的辅助函数
template<size_t N> Bounds<N> bounds(const Bound (&b) [N]) { return detail::to_arr(b); }
template<size_t N> Input<N> initvals(const double (&a) [N]) { return detail::to_arr(a); }
template<size_t N> auto score_gradient(double s, const double (&grad)[N])
{
    return ScoreGradient<N>(s, detail::to_arr(grad));
}

}} // namespace Slic3r::opt

#endif // OPTIMIZER_HPP
