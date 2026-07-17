#include "PNGReadWrite.hpp"

#include <memory>

#include <cstdio>
#include <png.h>

#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/cstdio.hpp>

namespace Slic3r { namespace png {

struct PNGDescr {
    png_struct *png = nullptr; png_info *info = nullptr;

    PNGDescr() = default;
    PNGDescr(const PNGDescr&) = delete;
    PNGDescr(PNGDescr&&) = delete;
    PNGDescr& operator=(const PNGDescr&) = delete;
    PNGDescr& operator=(PNGDescr&&) = delete;

    ~PNGDescr()
    {
        if (png && info) png_destroy_info_struct(png, &info);
        if (png) png_destroy_read_struct( &png, nullptr, nullptr);
    }
};

bool is_png(const ReadBuf &rb)
{
    static const constexpr int PNG_SIG_BYTES = 8;

#if PNG_LIBPNG_VER_MINOR <= 2
    // 早期 libpng 版本中的 png_sig_cmp(png_bytep, ...) 不接受 const 指针。
    // 无法从输入缓冲区中去除 const 限定符，所以...是的...生活充满挑战...
    png_byte buf[PNG_SIG_BYTES];
    auto inbuf = static_cast<const std::uint8_t *>(rb.buf);
    std::copy(inbuf, inbuf + PNG_SIG_BYTES, buf);
#else
    auto buf = static_cast<png_const_bytep>(rb.buf);
#endif

    return rb.sz >= PNG_SIG_BYTES && !png_sig_cmp(buf, 0, PNG_SIG_BYTES);
}

// libpng 的缓冲区读取回调。它提供一个已分配的输出缓冲区和
// 期望从输入中读取的数据量。
static void png_read_callback(png_struct *png_ptr,
                              png_bytep   outBytes,
                              png_size_t  byteCountToRead)
{
    // 通过 png_ptr 获取我们的输入缓冲区
    auto reader = static_cast<IStream *>(png_get_io_ptr(png_ptr));

    if (!reader || !reader->is_ok()) return;

    reader->read(static_cast<std::uint8_t *>(outBytes), byteCountToRead);
}

bool decode_png(IStream &in_buf, ImageGreyscale &out_img)
{
    static const constexpr int PNG_SIG_BYTES = 8;

    std::vector<png_byte> sig(PNG_SIG_BYTES, 0);
    in_buf.read(sig.data(), PNG_SIG_BYTES);
    if (!png_check_sig(sig.data(), PNG_SIG_BYTES))
        return false;

    PNGDescr dsc;
    dsc.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                     nullptr);

    if(!dsc.png) return false;

    dsc.info = png_create_info_struct(dsc.png);
    if(!dsc.info) return false;

    png_set_read_fn(dsc.png, static_cast<void *>(&in_buf), png_read_callback);

    // 告知我们已经读取了前几个字节用于检查签名
    png_set_sig_bytes(dsc.png, PNG_SIG_BYTES);

    png_read_info(dsc.png, dsc.info);

    out_img.cols = png_get_image_width(dsc.png, dsc.info);
    out_img.rows = png_get_image_height(dsc.png, dsc.info);
    size_t color_type = png_get_color_type(dsc.png, dsc.info);
    size_t bit_depth  = png_get_bit_depth(dsc.png, dsc.info);

    if (color_type != PNG_COLOR_TYPE_GRAY || bit_depth != 8)
        return false;

    out_img.buf.resize(out_img.rows * out_img.cols);

    auto readbuf = static_cast<png_bytep>(out_img.buf.data());
    for (size_t r = 0; r < out_img.rows; ++r)
        png_read_row(dsc.png, readbuf + r * out_img.cols, nullptr);

    return true;
}

bool decode_colored_png(IStream &in_buf, ImageColorscale &out_img)
{
    static const constexpr int PNG_SIG_BYTES = 8;

    std::vector<png_byte> sig(PNG_SIG_BYTES, 0);
    in_buf.read(sig.data(), PNG_SIG_BYTES);
    if (!png_check_sig(sig.data(), PNG_SIG_BYTES)) {
        BOOST_LOG_TRIVIAL(error) << boost::format("decode_colored_png: png_check_sig failed");
        return false;
    }

    PNGDescr dsc;
    dsc.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                     nullptr);

    if(!dsc.png) {
        BOOST_LOG_TRIVIAL(error) << boost::format("decode_colored_png: png_create_read_struct failed");
        return false;
    }

    dsc.info = png_create_info_struct(dsc.png);
    if(!dsc.info) {
        BOOST_LOG_TRIVIAL(error) << boost::format("decode_colored_png: png_create_info_struct failed");
        png_destroy_read_struct(&dsc.png, &dsc.info, NULL);
        return false;
    }

    png_set_read_fn(dsc.png, static_cast<void *>(&in_buf), png_read_callback);

    // 告知我们已经读取了前几个字节用于检查签名
    png_set_sig_bytes(dsc.png, PNG_SIG_BYTES);

    png_read_info(dsc.png, dsc.info);

    out_img.cols = png_get_image_width(dsc.png, dsc.info);
    out_img.rows = png_get_image_height(dsc.png, dsc.info);
    size_t color_type = png_get_color_type(dsc.png, dsc.info);
    size_t bit_depth  = png_get_bit_depth(dsc.png, dsc.info);
    unsigned long rowbytes = png_get_rowbytes(dsc.png, dsc.info);

    switch(color_type)
    {
        case PNG_COLOR_TYPE_RGB:
            out_img.bytes_per_pixel = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            out_img.bytes_per_pixel = 4;
            break;
        default: //not supported currently
            png_destroy_read_struct(&dsc.png, &dsc.info, NULL);
            return false;
    }

    BOOST_LOG_TRIVIAL(info) << boost::format("png's cols %1%, rows %2%, color_type %3%, bit_depth %4%, bytes_per_pixel %5%, rowbytes %6%")%out_img.cols %out_img.rows %color_type %bit_depth %out_img.bytes_per_pixel %rowbytes;
    out_img.buf.resize(out_img.rows * rowbytes);

    int filter_type = png_get_filter_type(dsc.png, dsc.info);
    int compression_type = png_get_compression_type(dsc.png, dsc.info);
    int interlace_type = png_get_interlace_type(dsc.png, dsc.info);
    BOOST_LOG_TRIVIAL(info) << boost::format("filter_type %1%, compression_type %2%, interlace_type %3%, rowbytes %4%")%filter_type %compression_type %interlace_type %rowbytes;

    auto readbuf = static_cast<png_bytep>(out_img.buf.data());
    for (size_t r = out_img.rows; r > 0; r--)
    {
        png_read_row(dsc.png, readbuf + (r - 1) * rowbytes, nullptr);
    }

    png_read_end(dsc.png, dsc.info);
    png_destroy_read_struct(&dsc.png, &dsc.info, NULL);

    return true;
}

bool decode_colored_png(const ReadBuf &in_buf, ImageColorscale &out_img)
{
    struct ReadBufStream stream{in_buf};

    return decode_colored_png(stream, out_img);
}


// 实用的函数，将打包的 RGB 图像存储到文件。主要用于调试目的。
// 基于 https://www.lemoda.net/c/write-png/
// png_color_type 为 PNG_COLOR_TYPE_RGB 或 PNG_COLOR_TYPE_GRAY
//FIXME 也许最好改用 tdefl_write_image_to_png_file_in_memory() ?
static bool write_rgb_or_gray_to_file(const char *file_name_utf8, size_t width, size_t height, int png_color_type, const uint8_t *data)
{
    bool         result       = false;

    // 由于 goto 语句而需要前向声明。
    png_structp  png_ptr      = nullptr;
    png_infop    info_ptr     = nullptr;
    png_byte   **row_pointers = nullptr;

    FILE        *fp = boost::nowide::fopen(file_name_utf8, "wb");
    if (! fp) {
        BOOST_LOG_TRIVIAL(error) << "write_png_file: File could not be opened for writing: " << file_name_utf8;
        goto fopen_failed;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (! png_ptr) {
        BOOST_LOG_TRIVIAL(error) << "write_png_file: png_create_write_struct() failed";
        goto png_create_write_struct_failed;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (! info_ptr) {
        BOOST_LOG_TRIVIAL(error) << "write_png_file: png_create_info_struct() failed";
        goto png_create_info_struct_failed;
    }

    // 设置错误处理。
    if (setjmp(png_jmpbuf(png_ptr))) {
        BOOST_LOG_TRIVIAL(error) << "write_png_file: setjmp() failed";
        goto png_failure;
    }

    // 设置图像属性。
    png_set_IHDR(png_ptr,
        info_ptr,
        png_uint_32(width),
        png_uint_32(height),
        8, // depth
        png_color_type,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    // 初始化 PNG 的行。
    row_pointers = reinterpret_cast<png_byte**>(::png_malloc(png_ptr, height * sizeof(png_byte*)));
    {
        int line_width = width;
        if (png_color_type == PNG_COLOR_TYPE_RGB)
            line_width *= 3;
        for (size_t y = 0; y < height; ++ y) {
            auto row = reinterpret_cast<png_byte*>(::png_malloc(png_ptr, line_width));
            row_pointers[y] = row;
            memcpy(row, data + line_width * y, line_width);
        }
    }

    // 将图像数据写入 "fp"。
    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, nullptr);

    for (size_t y = 0; y < height; ++ y)
        png_free(png_ptr, row_pointers[y]);
    png_free(png_ptr, row_pointers);

    result = true;

png_failure:
png_create_info_struct_failed:
    ::png_destroy_write_struct(&png_ptr, &info_ptr);
png_create_write_struct_failed:
    ::fclose(fp);
fopen_failed:
    return result;
}

bool write_rgb_to_file(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb)
{
    return write_rgb_or_gray_to_file(file_name_utf8, width, height, PNG_COLOR_TYPE_RGB, data_rgb);
}

bool write_rgb_to_file(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb)
{
    return write_rgb_to_file(file_name_utf8.c_str(), width, height, data_rgb);
}

bool write_rgb_to_file(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_rgb)
{
    assert(width * height * 3 == data_rgb.size());
    return write_rgb_to_file(file_name_utf8.c_str(), width, height, data_rgb.data());
}

bool write_gray_to_file(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_gray)
{
    return write_rgb_or_gray_to_file(file_name_utf8, width, height, PNG_COLOR_TYPE_GRAY, data_gray);
}

bool write_gray_to_file(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_gray)
{
    return write_gray_to_file(file_name_utf8.c_str(), width, height, data_gray);
}

bool write_gray_to_file(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_gray)
{
    assert(width * height == data_gray.size());
    return write_gray_to_file(file_name_utf8.c_str(), width, height, data_gray.data());
}

// 缩放变体主要用于调试目的，例如导出低分辨率距离场的图像。
// 通过复制行和列来实现缩放，没有任何平滑处理，以突出原始像素。
// png_color_type 为 PNG_COLOR_TYPE_RGB 或 PNG_COLOR_TYPE_GRAY
static bool write_rgb_or_gray_to_file_scaled(const char *file_name_utf8, size_t width, size_t height, int png_color_type, const uint8_t *data, size_t scale)
{
    if (scale <= 1)
        return write_rgb_or_gray_to_file(file_name_utf8, width, height, png_color_type, data);
    else {
        size_t pixel_bytes = png_color_type == PNG_COLOR_TYPE_RGB ? 3 : 1;
        size_t line_width  = width * pixel_bytes;
        std::vector<uint8_t> scaled(line_width * height * scale * scale);
        uint8_t *dst = scaled.data();
        for (size_t r = 0; r < height; ++ r) {
            for (size_t repr = 0; repr < scale; ++ repr) {
                const uint8_t *row = data + line_width * r;
                for (size_t c = 0; c < width; ++ c) {
                    for (size_t repc = 0; repc < scale; ++ repc)
                        for (size_t b = 0; b < pixel_bytes; ++ b)
                            *dst ++ = row[b];
                    row += pixel_bytes;
                }
            }
        }
        return write_rgb_or_gray_to_file(file_name_utf8, width * scale, height * scale, png_color_type, scaled.data());
    }
}

bool write_rgb_to_file_scaled(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb, size_t scale)
{
    return write_rgb_or_gray_to_file_scaled(file_name_utf8, width, height, PNG_COLOR_TYPE_RGB, data_rgb, scale);
}

bool write_rgb_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_rgb, size_t scale)
{
    return write_rgb_to_file_scaled(file_name_utf8.c_str(), width, height, data_rgb, scale);
}

bool write_rgb_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_rgb, size_t scale)
{
    assert(width * height * 3 == data_rgb.size());
    return write_rgb_to_file_scaled(file_name_utf8.c_str(), width, height, data_rgb.data(), scale);
}

bool write_gray_to_file_scaled(const char *file_name_utf8, size_t width, size_t height, const uint8_t *data_gray, size_t scale)
{
    return write_rgb_or_gray_to_file_scaled(file_name_utf8, width, height, PNG_COLOR_TYPE_GRAY, data_gray, scale);
}

bool write_gray_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const uint8_t *data_gray, size_t scale)
{
    return write_gray_to_file_scaled(file_name_utf8.c_str(), width, height, data_gray, scale);
}

bool write_gray_to_file_scaled(const std::string &file_name_utf8, size_t width, size_t height, const std::vector<uint8_t> &data_gray, size_t scale)
{
    assert(width * height == data_gray.size());
    return write_gray_to_file_scaled(file_name_utf8.c_str(), width, height, data_gray.data(), scale);
}

}} // namespace Slic3r::png
