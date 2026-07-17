#ifndef MTUTILS_HPP
#define MTUTILS_HPP

#include <atomic>       // for std::atomic_flag and memory orders
#include <mutex>        // for std::lock_guard
#include <functional>   // for std::function
#include <utility>      // for std::forward
#include <vector>
#include <algorithm>
#include <cmath>

#include "libslic3r.h"

namespace Slic3r {

/// 用于缓存网格的便捷自旋互斥锁。
/// 实现"Lockable"概念
class SpinMutex
{
    std::atomic_flag                m_flg;
    static const /*constexpr*/ auto MO_ACQ = std::memory_order_acquire;
    static const /*constexpr*/ auto MO_REL = std::memory_order_release;

public:
    inline SpinMutex() { m_flg.clear(MO_REL); }
    inline void lock() { while (m_flg.test_and_set(MO_ACQ)) ; }
    inline bool try_lock() { return !m_flg.test_and_set(MO_ACQ); }
    inline void unlock() { m_flg.clear(MO_REL); }
};

/// 围绕需要线程安全缓存的任意对象的包装类。
template<class T> class CachedObject
{
public:
    // 在对象失效时刷新对象的方法类型
    using Setter = std::function<void(T &)>;

private:
    T         m_obj;   // the object itself
    bool      m_valid; // invalidation flag
    SpinMutex m_lck;   // to make the caching thread safe

    // setter 将在即将获取对象的 const 值之前被调用。
    std::function<void(T &)> m_setter;

public:
    // Forwarded constructor
    template<class... Args>
    inline CachedObject(Setter &&fn, Args &&... args)
        : m_obj(std::forward<Args>(args)...), m_valid(false), m_setter(fn)
    {}

    // 使对象的值失效。对象将在下次检索时刷新（将调用 Setter）。
    // setter 函数中使用的数据在修改期间也应受到保护，
    // 因此修改必须在 fn 中进行。
    template<class Fn> void invalidate(Fn &&fn)
    {
        std::lock_guard<SpinMutex> lck(m_lck);
        fn();
        m_valid = false;
    }

    // 获取正确更新的 const 对象。
    inline const T &get()
    {
        std::lock_guard<SpinMutex> lck(m_lck);
        if (!m_valid) {
            m_setter(m_obj);
            m_valid = true;
        }
        return m_obj;
    }
};

template<class C> bool all_of(const C &container)
{
    return std::all_of(container.begin(),
                       container.end(),
                       [](const typename C::value_type &v) {
                           return static_cast<bool>(v);
                       });
}

//template<class T>
//using remove_cvref_t = std::remove_reference_t<std::remove_cv_t<T>>;

/// 完全类似于 Matlab https://www.mathworks.com/help/matlab/ref/linspace.html
template<class T, class I, class = IntegerOnly<I>>
inline std::vector<T> linspace_vector(const ArithmeticOnly<T> &start, 
                                      const T &stop, 
                                      const I &n)
{
    std::vector<T> vals(n, T());

    T      stride = (stop - start) / n;
    size_t i      = 0;
    std::generate(vals.begin(), vals.end(), [&i, start, stride] {
        return start + i++ * stride;
    });

    return vals;
}

template<size_t N, class T>
inline std::array<ArithmeticOnly<T>, N> linspace_array(const T &start, const T &stop)
{
    std::array<T, N> vals = {T()};

    T      stride = (stop - start) / N;
    size_t i      = 0;
    std::generate(vals.begin(), vals.end(), [&i, start, stride] {
        return start + i++ * stride;
    });

    return vals;
}

/// 一组等距的值，从 'start'（包含）开始，以小于或等于 'end' 的最接近 'stride' 的倍数结束，
/// 每个值之间留有 'stride' 空间。
/// 非常类似于 Matlab 的 [start:stride:end] 表示法。
template<class T>
inline std::vector<ArithmeticOnly<T>> grid(const T &start, 
                                           const T &stop, 
                                           const T &stride)
{
    std::vector<T> vals(size_t(std::ceil((stop - start) / stride)), T());
    
    int i = 0;
    std::generate(vals.begin(), vals.end(), [&i, start, stride] {
        return start + i++ * stride; 
    });
     
    return vals;
}

} // namespace Slic3r

#endif // MTUTILS_HPP
