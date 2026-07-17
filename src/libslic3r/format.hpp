#ifndef slic3r_format_hpp_
#define slic3r_format_hpp_

// 围绕boost::format的功能性包装。
// 有一天我们可能用C++20的format替换这个包装器
// https://en.cppreference.com/w/cpp/utility/format/format
// 尽管C++20的format对于位置无关参数使用了不同的模板模式。
//
// Boost::format通过丑陋的%链式操作符绕过了缺失的可变参数模板。boost::format的使用如下：
// (boost::format("template") % arg1 %arg2).str()
// 这个包装器允许更简洁的语法：
// Slic3r::format("template", arg1, arg2)
// 也可以重写Slic3r::internal::format::cook()函数，将Slic3r::format()参数转换为
// boost::format可以转换为字符串的内容，参见slic3r/GUI/I18N.hpp中用于将wxString转换为UTF8的"cook"函数。

#include <boost/format.hpp>

namespace Slic3r {

// 参考: https://gist.github.com/gchudnov/6a90d51af004d97337ec
namespace internal {
	namespace format {
		// 默认"cook"函数 - 直接转发。
		template<typename T>
		inline T&& cook(T&& arg) {
		  	return std::forward<T>(arg);
		}

		// 递归链的终点。
		inline std::string format_recursive(boost::format& message) {
		  	return message.str();
		}

		template<typename TValue, typename... TArgs>
		std::string format_recursive(boost::format& message, TValue&& arg, TArgs&&... args) {
			// 格式化，可能通过"cook"函数转换参数。
		  	message % cook(std::forward<TValue>(arg));
		  	return format_recursive(message, std::forward<TArgs>(args)...);
		}
	}
};

template<typename... TArgs>
inline std::string format(const char* fmt, TArgs&&... args) {
	boost::format message(fmt);
	return internal::format::format_recursive(message, std::forward<TArgs>(args)...);
}

template<typename... TArgs>
inline std::string format(const std::string& fmt, TArgs&&... args) {
	boost::format message(fmt);
	return internal::format::format_recursive(message, std::forward<TArgs>(args)...);
}

} // namespace Slic3r

#endif // slic3r_format_hpp_
