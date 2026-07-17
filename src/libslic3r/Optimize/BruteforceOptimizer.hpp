#ifndef BRUTEFORCEOPTIMIZER_HPP
#define BRUTEFORCEOPTIMIZER_HPP

#include <libslic3r/Optimize/Optimizer.hpp>

namespace Slic3r { namespace opt {

namespace detail {
//// 实现暴力优化器

//// 返回到达特定网格位置（idx）所需的迭代次数
template<size_t N>
long num_iter(const std::array<size_t, N> &idx, size_t gridsz)
{
    long ret = 0;
    for (size_t i = 0; i < N; ++i) ret += idx[i] * std::pow(gridsz, i);
    return ret;
}

//// 网格搜索的实现，其中搜索区间被采样为
//// 每个维度的等距点。网格大小决定了一个维度的采样数量，
//// 因此函数调用次数为 gridsize ^ dimension。
struct AlgBurteForce {
    bool to_min;
    StopCriteria stc;
    size_t gridsz;

    AlgBurteForce(const StopCriteria &cr, size_t gs): stc{cr}, gridsz{gs} {}

    //// 此函数为每个维度递归调用，并生成
    //// 特定维度的网格值。如果 D 小于零，
    // 则为每个维度生成目标函数输入值并可以
    //// 进行评估。当前最佳分数与新返回的分数比较并适当更改。
    template<int D, size_t N, class Fn, class Cmp>
    bool run(std::array<size_t, N> &idx,
             Result<N> &result,
             const Bounds<N> &bounds,
             Fn &&fn,
             Cmp &&cmp)
    {
        if (stc.stop_condition()) return false;

        if constexpr (D < 0) { // 评估 fn
            Input<N> inp;

            auto max_iter = stc.max_iterations();
            if (max_iter && num_iter(idx, gridsz) >= max_iter)
                return false;

            for (size_t d = 0; d < N; ++d) {
                const Bound &b = bounds[d];
                double step = (b.max() - b.min()) / (gridsz - 1);
                inp[d] = b.min() + idx[d] * step;
            }

            auto score = fn(inp);
            if (cmp(score, result.score)) { // 将当前分数更改为新分数
                double absdiff = std::abs(score - result.score);

                result.score = score;
                result.optimum = inp;

                //// 检查是否达到了所需的精度。
                if (absdiff < stc.abs_score_diff() ||
                    absdiff < stc.rel_score_diff() * std::abs(score))
                    return false;
            }

        } else {
            for (size_t i = 0; i < gridsz; ++i) {
                idx[D] = i; // 标记当前网格位置并深入
                if (!run<D - 1>(idx, result, bounds, std::forward<Fn>(fn),
                                std::forward<Cmp>(cmp)))
                    return false;
            }
        }

        return true;
    }

    template<class Fn, size_t N>
    Result<N> optimize(Fn&& fn,
                       const Input<N> &/*initvals*/,
                       const Bounds<N>& bounds)
    {
        std::array<size_t, N> idx = {};
        Result<N> result;

        if (to_min) {
            result.score = std::numeric_limits<double>::max();
            run<int(N) - 1>(idx, result, bounds, std::forward<Fn>(fn),
                            std::less<double>{});
        }
        else {
            result.score = std::numeric_limits<double>::lowest();
            run<int(N) - 1>(idx, result, bounds, std::forward<Fn>(fn),
                            std::greater<double>{});
        }

        return result;
    }
};

} // namespace detail

using AlgBruteForce = detail::AlgBurteForce;

template<>
class Optimizer<AlgBruteForce> {
    AlgBruteForce m_alg;

public:

    Optimizer(const StopCriteria &cr = {}, size_t gridsz = 100)
        : m_alg{cr, gridsz}
    {}

    Optimizer& to_max() { m_alg.to_min = false; return *this; }
    Optimizer& to_min() { m_alg.to_min = true;  return *this; }

    template<class Func, size_t N>
    Result<N> optimize(Func&& func,
                       const Input<N> &initvals,
                       const Bounds<N>& bounds)
    {
        return m_alg.optimize(std::forward<Func>(func), initvals, bounds);
    }

    Optimizer &set_criteria(const StopCriteria &cr)
    {
        m_alg.stc = cr; return *this;
    }

    const StopCriteria &get_criteria() const { return m_alg.stc; }
};

}} // namespace Slic3r::opt

#endif // BRUTEFORCEOPTIMIZER_HPP
