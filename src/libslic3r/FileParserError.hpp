#ifndef slic3r_FileParserError_hpp_
#define slic3r_FileParserError_hpp_

#include "libslic3r.h"

#include <string>
#include <boost/filesystem/path.hpp>
#include <stdexcept>

namespace Slic3r {

// 通用文件解析器错误，主要从 boost::property_tree::file_parser_error 复制而来
class file_parser_error: public Slic3r::RuntimeError
{
public:
    file_parser_error(const std::string &msg, const std::string &file, unsigned long line = 0) :
        Slic3r::RuntimeError(format_what(msg, file, line)),
        m_message(msg), m_filename(file), m_line(line) {}
    file_parser_error(const std::string &msg, const boost::filesystem::path &file, unsigned long line = 0) :
        Slic3r::RuntimeError(format_what(msg, file.string(), line)),
        m_message(msg), m_filename(file.string()), m_line(line) {}
    // gcc 3.4.2 会因编译器生成的析构函数缺少 throw 说明符而发出警告
    ~file_parser_error() throw() {}

    // 获取错误消息（不包含行和文件信息 - 使用 what() 获取完整消息）
    std::string message() const { return m_message; }
    // 获取错误文件名
    std::string filename() const { return m_filename; }
    // 获取错误行号
    unsigned long line() const { return m_line; }

private:
    std::string     m_message;
    std::string     m_filename;
    unsigned long   m_line;

    // 格式化将由 Slic3r::RuntimeError::what() 返回的错误消息
    static std::string format_what(const std::string &msg, const std::string &file, unsigned long l)
    {
        std::stringstream stream;
        stream << (file.empty() ? "<unspecified file>" : file.c_str());
        if (l > 0)
            stream << '(' << l << ')';
        stream << ": " << msg;
        return stream.str();
    }
};

}; // Slic3r

#endif // slic3r_FileParserError_hpp_
