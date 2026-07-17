#ifndef PNGREAD_HPP
#define PNGREAD_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <istream>

namespace Slic3r { namespace png {

// 编码 PNG 图像数据的输入流接口。
struct IStream {
    virtual ~IStream() = default;
    virtual size_t read(std::uint8_t *outp, size_t amount) = 0;
    virtual bool is_ok() const = 0;
};

// decode_png 的输出格式：一个连续逐行存储的二维像素矩阵（行主序布局）。
template<class PxT> struct Image {
    std::vector<PxT> buf;
    size_t rows, cols;
    PxT get(size_t row, size_t col) const { return buf[row * cols + col]; }
};

using ImageGreyscale = Image<uint8_t>;
struct ImageColorscale:Image<unsigned char>
{
    int bytes_per_pixel;
};


// 仅解码真正的 8 位灰度 PNG 图像。对其他格式返回 false。
// TODO（如果需要）：实现将 RGB 图像转换为灰度...
bool decode_png(IStream &stream, ImageGreyscale &out_img);

//BBS: 解码其他格式的 png
bool decode_colored_png(IStream &in_buf, ImageColorscale &out_img);

// TODO（如果需要）
// struct RGB { uint8_t r, g, b; };
// using ImageRGB = Image<RGB>;
// bool decode_png(IStream &stream, ImageRGB &img);


// 编码的 PNG 数据缓冲区：一个简单的只读缓冲区及其大小。
struct ReadBuf { const void *buf = nullptr; const size_t sz = 0; };

bool is_png(const ReadBuf &pngbuf);

struct ReadBufStream: public IStream {
    const ReadBuf &rbuf_ref;
    size_t pos = 0;

    explicit ReadBufStream(const ReadBuf &buf): rbuf_ref{buf} {}

    size_t read(std::uint8_t *outp, size_t amount) override
    {
        if (amount > rbuf_ref.sz - pos) return 0;

        auto buf = static_cast<const std::uint8_t *>(rbuf_ref.buf);
        std::copy(buf + pos, buf + (pos + amount), outp);
        pos += amount;

        return amount;
    }

    bool is_ok() const override { return pos < rbuf_ref.sz; }
};

template<class Img> bool decode_png(const ReadBuf &in_buf, Img &out_img)
{
    struct ReadBufStream stream{in_buf};

    return decode_png(stream, out_img);
}

bool decode_colored_png(const ReadBuf &in_buf, ImageColorscale &out_img);


// TODO: std::istream 或 FILE* 可以类似地进行适配，以备需要时使用...



// 实用的函数，将打包的 RGB 图像存储到文件。主要用于调试目的。
bool write_rgb_to_file(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb);
bool write_rgb_to_file(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb);
bool write_rgb_to_file(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_rgb);
// 灰度变体
bool write_gray_to_file(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_gray);
bool write_gray_to_file(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_gray);
bool write_gray_to_file(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_gray);

// 缩放变体主要用于调试目的，例如导出低分辨率距离场的图像。
// 通过复制行和列来实现缩放，没有任何平滑处理，以突出原始像素。
bool write_rgb_to_file_scaled(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb, size_t scale);
bool write_rgb_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb, size_t scale);
bool write_rgb_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_rgb, size_t scale);
// 灰度变体
bool write_gray_to_file_scaled(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_gray, size_t scale);
bool write_gray_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_gray, size_t scale);
bool write_gray_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_gray, size_t scale);


}}     // namespace Slic3r::png

#endif // PNGREAD_HPP
