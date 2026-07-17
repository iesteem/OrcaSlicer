#ifndef ZIPPER_HPP
#define ZIPPER_HPP

#include <cstdint>
#include <string>
#include <memory>

namespace Slic3r {

// 用于创建zip归档的类。
class Zipper {
public:
    // 支持三种压缩级别
    enum e_compression {
        NO_COMPRESSION,
        FAST_COMPRESSION,
        TIGHT_COMPRESSION
    };

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::string m_data;
    std::string m_entry;
    e_compression m_compression;

public:

    // 如果无法创建文件，将引发运行时异常。
    explicit Zipper(const std::string& zipfname,
                    e_compression level = FAST_COMPRESSION);
    ~Zipper();

    // 不允许拷贝，这是一个文件资源...
    Zipper(const Zipper&) = delete;
    Zipper& operator=(const Zipper&) = delete;

    // 移动是可以的。
    // Zipper(Zipper&&) = default;
    // Zipper& operator=(Zipper&&) = default;
    // All becouse of VS2013:
    Zipper(Zipper &&m);
    Zipper& operator=(Zipper &&m);

    /// 添加条目意味着新归档内的一个文件。Name参数是新文件的名称。
    /// 要创建目录，请附加正斜杠。
    /// 上一个条目已完成（参见finish_entry）
    void add_entry(const std::string& name);

    /// 使用即时给定的字节缓冲区添加新的二进制文件条目。
    /// 此方法抛出的异常与finish_entry()完全相同。
    void add_entry(const std::string& name, const void* data, size_t bytes);

    // 将数据写入归档的工作原理与标准流相同。zip文件中的目标
    // 是使用add_entry方法创建的条目。

    // 模板仅接受算术值，std::to_string可以处理这些值。
    template<class T> inline
    typename std::enable_if<std::is_arithmetic<T>::value, Zipper&>::type
    operator<<(T &&val) {
        return this->operator<<(std::to_string(std::forward<T>(val)));
    }

    // 模板仅应用于std::string可以处理追加和复制的类型。
    // 这包括C风格字符串...
    template<class T> inline
    typename std::enable_if<!std::is_arithmetic<T>::value, Zipper&>::type
    operator<<(T &&val) {
        if(m_data.empty()) m_data = std::forward<T>(val);
        else m_data.append(val);
        return *this;
    }

    /// 完成条目意味着后续写入将不再追加到上一个条目。
    /// 它们将被写入内部缓冲区，一旦添加了条目，缓冲区将绑定到新条目。
    /// 如果缓冲区已写入但没有添加条目，则在此调用后缓冲区将被清除。
    ///
    /// 如果发生错误，此方法将抛出运行时异常。
    /// 条目仍将保持打开（数据完整），但文件的状态取决于minz在错误写入后的状态。
    void finish_entry();

    void finalize();

    const std::string & get_filename() const;
};


}

#endif // ZIPPER_HPP
