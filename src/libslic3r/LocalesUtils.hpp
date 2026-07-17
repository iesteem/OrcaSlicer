#ifndef slic3r_LocalesUtils_hpp_
#define slic3r_LocalesUtils_hpp_

#include <string>
#include <clocale>
#include <iomanip>
#include <cassert>
#include <string_view>

#ifdef __APPLE__
#include <xlocale.h>
#endif

namespace Slic3r {

// RAII 包装器，在构造时设置 LC_NUMERIC 为 "C"，在析构时恢复旧值。
class CNumericLocalesSetter {
public:
    CNumericLocalesSetter();
    ~CNumericLocalesSetter();

private:
#ifdef _WIN32
    std::string m_orig_numeric_locale;
#else
    locale_t m_original_locale;
    locale_t m_new_locale;
#endif

};

// 检查当前 C 语言环境是否使用小数点作为分隔符的函数。
// 主要用于断言。
bool is_decimal_separator_point();


// std::to_string 的替代品，根据 C++ 语言环境工作，而不是 C 语言环境。
// 在我们需要确保使用小数点作为分隔符时使用。
// （我们在大部分代码中使用用户 C 语言环境和 "C" C++ 语言环境。）
std::string float_to_string_decimal_point(double value, int precision = -1);
//std::string float_to_string_decimal_point(float value,  int precision = -1);
double string_to_double_decimal_point(const std::string_view str, size_t* pos = nullptr);

} // namespace Slic3r

#endif // slic3r_LocalesUtils_hpp_
