﻿#ifndef EXECUTION_HPP
#define EXECUTION_HPP

#include <type_traits>
#include <utility>
#include <cstddef>
#include <iterator>

#include "libslic3r/libslic3r.h"

namespace Slic3r {

//// 用于 有效 的 覆盖 execution policies
template<class EP> struct IsExecutionPolicy_ : public std::false_type {};

template<class EP> constexpr bool IsExecutionPolicy =
    IsExecutionPolicy_<remove_cvref_t<EP>>::value;

template<class EP, class T = void>
using ExecutionPolicyOnly = std::enable_if_t<IsExecutionPolicy<EP>, T>;

namespace execution {

//// 用于 每个 的 此 结构体 needs to 为 specialized execution policy.
//// 用于 example 的 See ExecutionSeq.hpp and ExecutionTBB.hpp.
template<class EP, class En = void> struct Traits {};

template<class EP> using AsTraits = Traits<remove_cvref_t<EP>>;

//// 每个 execution policy 应 declare two types of mutexes. A a spin lock and
//// a blocking mutex. 这些 types 应 satisfy the BasicLockable concept.
template<class EP> using SpinningMutex = typename Traits<EP>::SpinningMutex;
template<class EP> using BlockingMutex = typename Traits<EP>::BlockingMutex;

//// 用于 concurrency 的 Query the available threads.
template<class EP, class = ExecutionPolicyOnly<EP> >
size_t max_concurrency(const EP &ep)
{
    return AsTraits<EP>::max_concurrency(ep);
}

//// foreach 循环 with the execution policy passed as 参数. Granularity 可以
//// 用于 optimal 的 为 指定 explicitly. max_concurrency() 可以 为 used results.
template<class EP, class It, class Fn, class = ExecutionPolicyOnly<EP>>
void for_each(const EP &ep, It from, It to, Fn &&fn, size_t granularity = 1)
{
    AsTraits<EP>::for_each(ep, from, to, std::forward<Fn>(fn), granularity);
}

//// A reduce operation with the execution policy passed as 参数.
//// mergefn 有 T(const T&, const T&) 签名
//// accessfn 有 T(I) 签名 如果 I 是 an integral 类型 and
// T(const I::value_type &) if I is an iterator type.
template<class EP,
         class I,
         class MergeFn,
         class T,
         class AccessFn,
         class = ExecutionPolicyOnly<EP> >
T reduce(const EP & ep,
         I          from,
         I          to,
         const T &  init,
         MergeFn && mergefn,
         AccessFn &&accessfn,
         size_t     granularity = 1)
{
    return AsTraits<EP>::reduce(ep, from, to, init,
                                std::forward<MergeFn>(mergefn),
                                std::forward<AccessFn>(accessfn),
                                granularity);
}

//// An 重载 of reduce 方法 to 为 used with iterators as 'from' and 'to'
//// arguments. Access functor 是 omitted 此处.
template<class EP,
         class I,
         class MergeFn,
         class T,
         class = ExecutionPolicyOnly<EP> >
T reduce(const EP &ep,
         I         from,
         I         to,
         const T & init,
         MergeFn &&mergefn,
         size_t    granularity = 1)
{
    return reduce(
        ep, from, to, init, std::forward<MergeFn>(mergefn),
        [](const auto &i) { return i; }, granularity);
}

template<class EP,
         class I,
         class T,
         class AccessFn,
         class = ExecutionPolicyOnly<EP>>
T accumulate(const EP & ep,
             I          from,
             I          to,
             const T &  init,
             AccessFn &&accessfn,
             size_t     granularity = 1)
{
    return reduce(ep, from, to, init, std::plus<T>{},
                  std::forward<AccessFn>(accessfn), granularity);
}


template<class EP,
         class I,
         class T,
         class = ExecutionPolicyOnly<EP> >
T accumulate(const EP &ep,
             I         from,
             I         to,
             const T & init,
             size_t    granularity = 1)
{
    return reduce(
        ep, from, to, init, std::plus<T>{}, [](const auto &i) { return i; },
        granularity);
}

} // namespace execution_policy
} // namespace Slic3r

#endif // EXECUTION_HPP
