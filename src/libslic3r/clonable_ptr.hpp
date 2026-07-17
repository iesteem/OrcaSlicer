// clonable_ptr: 一种智能指针，用法类似于unique_ptr，但不同之处在于
// 拷贝构造函数/拷贝赋值运算符通过调用->clone()方法工作。

// 派生自 https://github.com/SRombauts/shared_ptr/blob/master/include/unique_ptr.hpp
/**
 * @file  clonable_ptr.hpp
 * @brief clonable_ptr是一个假实现，用于在较旧编译器上编译时代替C++11 std::clonable_ptr。
 *
 * @see http://www.cplusplus.com/reference/memory/clonable_ptr/
 *
 * Copyright (c) 2014-2019 Sebastien Rombauts (sebastien.rombauts@gmail.com)
 *
 * 根据MIT许可证(MIT)分发（参见随附的LICENSE.txt文件
 * 或拷贝自 http://opensource.org/licenses/MIT）
 */

#include "assert.h"

namespace Slic3r {

// 检测编译器是否支持C++11 noexcept异常规范。
#if defined(_MSC_VER) && _MSC_VER < 1900 && ! defined(noexcept)
    #define noexcept throw()
#endif

template<class T>
class clonable_ptr
{
public:
    /// 被管理对象的类型，别名作为成员类型
    typedef T element_type;

    /// @brief 默认构造函数
    clonable_ptr() noexcept :
        px(nullptr)
    {
    }
    /// @brief 使用提供的指针进行管理的构造函数
    explicit clonable_ptr(T* p) noexcept :
        px(p)
    {
    }
    /// @brief 拷贝构造函数，通过调用rhs.clone()方法克隆
    clonable_ptr(const clonable_ptr& rhs) :
		px(rhs ? rhs.px->clone() : nullptr)
    {
    }
    /// @brief 移动构造函数，从不抛出异常
    clonable_ptr(clonable_ptr&& rhs) noexcept :
        px(rhs.px)
    {
        rhs.px = nullptr;
    }
    /// @brief 赋值运算符
    clonable_ptr& operator=(const clonable_ptr& rhs)
    {
		delete px;
		px = rhs ? rhs.px->clone() : nullptr;
        return *this;
    }
    /// @brief 移动运算符，从不抛出异常
    clonable_ptr& operator=(clonable_ptr&& rhs)
    {
		delete px;
        px = rhs.px;
        rhs.px = nullptr;
        return *this;
    }
    /// @brief 析构函数释放其所有权并销毁对象
    inline ~clonable_ptr() noexcept
    {
        destroy();
    }
    /// @brief 此reset释放其所有权并销毁对象
    inline void reset() noexcept
    {
        destroy();
    }
    /// @brief 此reset释放其所有权并重新获取另一个
    void reset(T* p) noexcept
    {
        assert((nullptr == p) || (px != p)); // auto-reset not allowed
        destroy();
        px = p;
    }

    /// @brief 用于copy-and-swap惯用法的交换方法（拷贝构造函数和交换方法）
    void swap(clonable_ptr& rhs) noexcept
    {
        T *tmp = px;
        px = rhs.px;
        rhs.px = tmp;
    }

    /// @brief 释放px指针的所有权而不销毁对象！
    inline void release() noexcept
    {
        px = nullptr;
    }

    // 引用计数器操作：
    inline operator bool() const noexcept
    {
        return (nullptr != px); // TODO nullptrptr
    }

    // 底层指针操作：
    inline T& operator*()  const noexcept
    {
        assert(nullptr != px);
        return *px;
    }
    inline T* operator->() const noexcept
    {
        assert(nullptr != px);
        return px;
    }
    inline T* get()  const noexcept
    {
        // no assert, can return nullptr
        return px;
    }

private:
    /// @brief 释放px指针的所有权并销毁对象
    inline void destroy() noexcept
    {
        delete px;
        px = nullptr;
    }

    /// @brief hack: const-cast释放px指针的所有权而不销毁对象！
    inline void release() const noexcept
    {
        px = nullptr;
    }

private:
    T* px; //!< 原生指针
};

// comparison operators
template<class T, class U> inline bool operator==(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() == r.get());
}
template<class T, class U> inline bool operator!=(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() != r.get());
}
template<class T, class U> inline bool operator<=(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() <= r.get());
}
template<class T, class U> inline bool operator<(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() < r.get());
}
template<class T, class U> inline bool operator>=(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() >= r.get());
}
template<class T, class U> inline bool operator>(const clonable_ptr<T>& l, const clonable_ptr<U>& r) noexcept
{
    return (l.get() > r.get());
}

} // namespace Slic3r
