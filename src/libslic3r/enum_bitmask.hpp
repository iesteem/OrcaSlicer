#ifndef slic3r_enum_bitmask_hpp_
#define slic3r_enum_bitmask_hpp_

// enum_bitmask用于以类型安全的方式将一组属性传递给函数。
// 改编自 https://gpfault.net/posts/typesafe-bitmasks.txt.html
// 参考 https://www.strikerx3.dev/cpp/2019/02/27/typesafe-enum-class-bitmasks-in-cpp.html

#include <type_traits>

namespace Slic3r {

// enum_bitmasks只能用于枚举。
template<class option_type, typename = typename std::enable_if<std::is_enum<option_type>::value>::type>
class enum_bitmask {
    // 用于存储位掩码值的类型应与枚举的底层类型相同。
    using underlying_type = typename std::underlying_type<option_type>::type;

    // 此方法帮助我们避免显式地将枚举值设置为2的幂。
    static constexpr underlying_type mask_value(option_type o) { return 1 << static_cast<underlying_type>(o); }

    // 内部使用的私有构造函数。
    explicit constexpr enum_bitmask(underlying_type o) : m_bits(o) {}

public:
    // 默认构造函数创建没有选择任何选项的位掩码。
    constexpr enum_bitmask() : m_bits(0) {}

    // 创建一个只设置了一个位的enum_bitmask。
    // 这个构造函数故意不是explicit的，以允许将选项传递给函数：
    // FunctionExpectingBitmask(Options::Opt1)
    constexpr enum_bitmask(option_type o) : m_bits(mask_value(o)) {}

    // 设置对应于给定选项的位。
    constexpr enum_bitmask operator|(option_type t) const { return enum_bitmask(m_bits | mask_value(t)); }

    // 与另一个相同类型的enum_bitmask组合。
    constexpr enum_bitmask operator|(enum_bitmask<option_type> t) const { return enum_bitmask(m_bits | t.m_bits); }
    
    // 设置对应于给定选项的位。
    constexpr void operator|=(option_type t) { m_bits = enum_bitmask(m_bits | mask_value(t)); }

    // 与另一个相同类型的enum_bitmask组合。
    constexpr void operator|=(enum_bitmask<option_type> t) { m_bits = enum_bitmask(m_bits | t.m_bits); }

    // 获取对应于给定选项的位的值。
    constexpr bool operator&(option_type t) const { return m_bits & mask_value(t); }
    constexpr bool has(option_type t) const { return m_bits & mask_value(t); }
    
    constexpr bool operator==(const enum_bitmask r) const { return m_bits == r.m_bits; }
    constexpr bool operator!=(const enum_bitmask r) const { return m_bits != r.m_bits; }
    // 用于按枚举值排序。
    constexpr bool lower(const enum_bitmask r) const { return m_bits < r.m_bits; }

private:
    underlying_type m_bits = 0;
};

// 用于启用从枚举的位操作产生enum_bitmask<>类型的自由函数。
template<typename Enum> struct is_enum_bitmask_type { static const bool enable = false; };
#define ENABLE_ENUM_BITMASK_OPERATORS(x) template<> struct is_enum_bitmask_type<x> { static const bool enable = true; };
template<class Enum> inline constexpr bool is_enum_bitmask_type_v = is_enum_bitmask_type<Enum>::enable;

// 从两个选项创建enum_bitmask，方便将选项传递给函数：
// FunctionExpectingBitmask(Options::Opt1 | Options::Opt2 | Options::Opt3)
template <class option_type>
constexpr std::enable_if_t<is_enum_bitmask_type_v<option_type>, enum_bitmask<option_type>> operator|(option_type lhs, option_type rhs) {
    static_assert(std::is_enum_v<option_type>);
    return enum_bitmask<option_type>{lhs} | rhs;
}

template <class option_type>
constexpr std::enable_if_t<is_enum_bitmask_type_v<option_type>, enum_bitmask<option_type>> operator|(option_type lhs, enum_bitmask<option_type> rhs) {
    static_assert(std::is_enum_v<option_type>);
    return enum_bitmask<option_type>{lhs} | rhs;
}

template <class option_type>
constexpr std::enable_if_t<is_enum_bitmask_type_v<option_type>, enum_bitmask<option_type>> only_if(bool condition, option_type opt) {
    static_assert(std::is_enum_v<option_type>);
    return condition ? enum_bitmask<option_type>{opt} : enum_bitmask<option_type>{};
}

template <class option_type>
constexpr std::enable_if_t<is_enum_bitmask_type_v<option_type>, enum_bitmask<option_type>> only_if(bool condition, enum_bitmask<option_type> opt) {
    static_assert(std::is_enum_v<option_type>);
    return condition ? opt : enum_bitmask<option_type>{};
}

} // namespace Slic3r

#endif // slic3r_enum_bitmask_hpp_
