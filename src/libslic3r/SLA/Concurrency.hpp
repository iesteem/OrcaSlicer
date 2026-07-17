#ifndef SLA_CONCURRENCY_H
#define SLA_CONCURRENCY_H

// FIXME: 已弃用

#include <libslic3r/Execution/ExecutionSeq.hpp>
#include <libslic3r/Execution/ExecutionTBB.hpp>

namespace Slic3r {
namespace sla {

// 设置为 true 以启用此模块中的完全并行性。
// 如果设置为 false，则只有经过充分测试的部分会并发执行。
const constexpr bool USE_FULL_CONCURRENCY = true;

template<bool> struct _ccr {};

template<> struct _ccr<true>
{
    using SpinningMutex = execution::SpinningMutex<ExecutionTBB>;
    using BlockingMutex = execution::BlockingMutex<ExecutionTBB>;

    template<class It, class Fn>
    static void for_each(It from, It to, Fn &&fn, size_t granularity = 1)
    {
        execution::for_each(ex_tbb, from, to, std::forward<Fn>(fn), granularity);
    }

    template<class...Args>
    static auto reduce(Args&&...args)
    {
        return execution::reduce(ex_tbb, std::forward<Args>(args)...);
    }

    static size_t max_concurreny()
    {
        return execution::max_concurrency(ex_tbb);
    }
};

template<> struct _ccr<false>
{
    using SpinningMutex = execution::SpinningMutex<ExecutionSeq>;
    using BlockingMutex = execution::BlockingMutex<ExecutionSeq>;

    template<class It, class Fn>
    static void for_each(It from, It to, Fn &&fn, size_t granularity = 1)
    {
        execution::for_each(ex_seq, from, to, std::forward<Fn>(fn), granularity);
    }

    template<class...Args>
    static auto reduce(Args&&...args)
    {
        return execution::reduce(ex_seq, std::forward<Args>(args)...);
    }

    static size_t max_concurreny()
    {
        return execution::max_concurrency(ex_seq);
    }
};

using ccr = _ccr<USE_FULL_CONCURRENCY>;
using ccr_seq = _ccr<false>;
using ccr_par = _ccr<true>;

}} // namespace Slic3r::sla

#endif // SLACONCURRENCY_H
