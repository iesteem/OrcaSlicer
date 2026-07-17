#ifndef slic3r_Utils_hpp_
#define slic3r_Utils_hpp_

#include <iomanip>
#include <locale>
#include <utility>
#include <functional>
#include <type_traits>
#include <system_error>
#include <regex>

#include <boost/system/error_code.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/ptime.hpp"

#include <openssl/md5.h>

#include "libslic3r.h"

//define CLI errors

#define CLI_SUCCESS                 0
#define CLI_ENVIRONMENT_ERROR       -1
#define CLI_INVALID_PARAMS          -2
#define CLI_FILE_NOTFOUND           -3
#define CLI_FILELIST_INVALID_ORDER  -4
#define CLI_CONFIG_FILE_ERROR       -5
#define CLI_DATA_FILE_ERROR         -6
#define CLI_INVALID_PRINTER_TECH    -7
#define CLI_UNSUPPORTED_OPERATION   -8

#define CLI_COPY_OBJECTS_ERROR      -9
#define CLI_SCALE_TO_FIT_ERROR      -10
#define CLI_EXPORT_STL_ERROR        -11
#define CLI_EXPORT_OBJ_ERROR        -12
#define CLI_EXPORT_3MF_ERROR        -13
#define CLI_OUT_OF_MEMORY           -14
#define CLI_3MF_NOT_SUPPORT_MACHINE_CHANGE      -15
#define CLI_3MF_NEW_MACHINE_NOT_SUPPORTED       -16
#define CLI_PROCESS_NOT_COMPATIBLE     -17
#define CLI_INVALID_VALUES_IN_3MF      -18
#define CLI_POSTPROCESS_NOT_SUPPORTED  -19
#define CLI_PRINTABLE_SIZE_REDUCED     -20
#define CLI_OBJECT_ARRANGE_FAILED      -21
#define CLI_OBJECT_ORIENT_FAILED       -22
#define CLI_MODIFIED_PARAMS_TO_PRINTER -23
#define CLI_FILE_VERSION_NOT_SUPPORTED -24


#define CLI_NO_SUITABLE_OBJECTS     -50
#define CLI_VALIDATE_ERROR          -51
#define CLI_OBJECTS_PARTLY_INSIDE   -52
#define CLI_EXPORT_CACHE_DIRECTORY_CREATE_FAILED   -53
#define CLI_EXPORT_CACHE_WRITE_FAILED   -54
#define CLI_IMPORT_CACHE_NOT_FOUND      -55
#define CLI_IMPORT_CACHE_DATA_CAN_NOT_USE -56
#define CLI_IMPORT_CACHE_LOAD_FAILED      -57
#define CLI_SLICING_TIME_EXCEEDS_LIMIT      -58
#define CLI_TRIANGLE_COUNT_EXCEEDS_LIMIT    -59
#define CLI_NO_SUITABLE_OBJECTS_AFTER_SKIP  -60
#define CLI_FILAMENT_NOT_MATCH_BED_TYPE     -61
#define CLI_FILAMENTS_DIFFERENT_TEMP        -62
#define CLI_OBJECT_COLLISION_IN_SEQ_PRINT   -63
#define CLI_OBJECT_COLLISION_IN_LAYER_PRINT -64
#define CLI_SPIRAL_MODE_INVALID_PARAMS      -65

#define CLI_SLICING_ERROR                  -100
#define CLI_GCODE_PATH_CONFLICTS           -101


namespace boost { namespace filesystem { class directory_entry; }}

namespace Slic3r {

extern void set_logging_level(unsigned int level);
extern unsigned int level_string_to_boost(std::string level);
extern std::string  get_string_logging_level(unsigned level);
extern unsigned get_logging_level();
extern void trace(unsigned int level, const char *message);
// 格式化分配的内存，用逗号分隔千位。
extern std::string format_memsize_MB(size_t n);
// 返回要添加到boost::log输出以告知当前进程内存分配情况的字符串。
// 如果日志级别>= info (3)或ignore_loglevel==true，则字符串非空。
// 后者用于从SysInfoDialog获取内存信息。
extern std::string log_memory_info(bool ignore_loglevel = false);
extern void disable_multi_threading();
// 返回物理内存（RAM）的大小，以字节为单位。
extern size_t total_physical_memory();

// 设置GUI资源文件的路径。
void set_var_dir(const std::string &path);
// 返回GUI资源文件的完整路径。
const std::string& var_dir();
// 返回文件名的完整资源路径。
std::string var(const std::string &file_name);

// 设置各种静态定义数据的路径（例如初始配置包）。
void set_resources_dir(const std::string &path);
// 返回资源目录的完整路径。
const std::string& resources_dir();

//BBS: add temp dir
void set_temporary_dir(const std::string &path);
const std::string& temporary_dir();

//BBS: convert 0.1.3.4 version format to 00.01.03.04 format, like AA.BB.CC.DD
inline std::string convert_to_full_version(std::string short_version)
{
    std::string result = "";
    std::vector<std::string> items;
    boost::split(items, short_version, boost::is_any_of("."));
    if (items.size() == 4) {
        for (int i = 0; i < 4; i++) {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << items[i];
            result += ss.str();
            if (i != 4 - 1)
                result += ".";
        }
        return result;
    }
    return result;
}
template<typename DataType>
inline DataType round_divide(DataType dividend, DataType divisor) //!< Return dividend divided by divisor rounded to the nearest integer
{
    return (dividend + divisor / 2) / divisor;
}

// 设置GUI本地化文件的路径。
void set_local_dir(const std::string &path);
// 返回本地化目录的完整路径。
const std::string& localization_dir();

// 设置形状库文件的路径。
void set_sys_shapes_dir(const std::string &path);
// 返回系统形状库目录的完整路径。
const std::string& sys_shapes_dir();

// 返回自定义形状库目录的完整路径。
std::string custom_shapes_dir();

// 设置形状库文件的路径。
void set_custom_gcodes_dir(const std::string &path);
// 返回系统形状库目录的完整路径。
const std::string& custom_gcodes_dir();

// 设置预设文件的路径。
void set_data_dir(const std::string &path);
// 返回GUI资源文件的完整路径。
const std::string& data_dir();

// BBL: true: succeed create or dir exists; false: fail to create
bool makedir(const std::string path);

// 为调试目的格式化输出路径。
// 在首次调用函数时将输出路径前缀写入控制台，
// 以便用户知道在哪里查找调试输出。
std::string debug_out_path(const char *name, ...);
// 较小的级别意味着较少的日志。level=5表示保存所有日志。
void set_log_path_and_level(const std::string& file, unsigned int level);

/*
 * TODO : This interface may have truncation issues.
 */
void flush_logs();

// 用于在本地Windows 8位代码页中编码的字符串的特殊类型。
// 此类型仅在Perl绑定中需要，用于向Perl传递该字符串是原始的，不是UTF-8编码的。
typedef std::string local_encoded_string;

// 返回下一个utf8序列的长度。=字符串中一起组成一个utf-8字符的字节数。
// 从pos开始。ASCII字符返回1。如果pos在序列中间也有效。
extern size_t get_utf8_sequence_length(const std::string& text, size_t pos = 0);
extern size_t get_utf8_sequence_length(const char *seq, size_t size);

// 将UTF-8编码的字符串转换为本地编码。
// 在Windows上，UTF-8字符串被转换为本地8位代码页。
// 在OSX和Linux上，此函数不进行转换，返回源字符串的副本。
extern local_encoded_string encode_path(const char *src);
extern std::string decode_path(const char *src);
extern std::string normalize_utf8_nfc(const char *src);

// 安全地重命名文件，即使目标文件已存在。
// 在Windows上，文件资源管理器（或防病毒软件等）经常在短时间内锁定文件，
// 因此文件可能无法移动。在遇到可恢复错误时重试。
extern std::error_code rename_file(const std::string &from, const std::string &to);

enum CopyFileResult {
	SUCCESS = 0,
	FAIL_COPY_FILE,
	FAIL_FILES_DIFFERENT,
	FAIL_RENAMING,
	FAIL_CHECK_ORIGIN_NOT_OPENED,
	FAIL_CHECK_TARGET_NOT_OPENED
};
// 复制文件，调整访问属性，使目标文件可写。
CopyFileResult copy_file_inner(const std::string &from, const std::string &to, std::string& error_message);
// Copy file to a temp file first, then rename it to the final file name.
// If with_check is true, then the content of the copied file is compared to the content
// of the source file before renaming.
// Additional error info is passed in error message.
extern CopyFileResult copy_file(const std::string &from, const std::string &to, std::string& error_message, const bool with_check = false);

// Compares two files if identical.
extern CopyFileResult check_copy(const std::string& origin, const std::string& copy);

// Ignore system and hidden files, which may be created by the DropBox synchronisation process.
// https://github.com/prusa3d/PrusaSlicer/issues/1298
extern bool is_plain_file(const boost::filesystem::directory_entry &path);
extern bool is_ini_file(const boost::filesystem::directory_entry &path);
extern bool is_idx_file(const boost::filesystem::directory_entry &path);
extern bool is_gcode_file(const std::string &path);
extern bool is_img_file(const std::string& path);
extern bool is_gallery_file(const boost::filesystem::directory_entry& path, char const* type);
extern bool is_gallery_file(const std::string& path, char const* type);
extern bool is_shapes_dir(const std::string& dir);
//BBS: add json support
extern bool is_json_file(const std::string& path);

// Orca: custom protocal support utils
inline bool is_orca_open(const std::string& url)
{
    return boost::starts_with(url, "Snapmaker_Orca://open") || boost::starts_with(url, "snapmaker-orca://open");
}
inline bool is_prusaslicer_open(const std::string& url) { return boost::starts_with(url, "prusaslicer://open"); }
inline bool is_bambustudio_open(const std::string& url) { return boost::starts_with(url, "bambustudio://open") || boost::starts_with(url, "bambustudioopen://"); }
inline bool is_cura_open(const std::string& url) { return boost::starts_with(url, "cura://open"); }
inline bool is_supported_open_protocol(const std::string& url) { return is_orca_open(url) || is_prusaslicer_open(url) || is_bambustudio_open(url) || is_cura_open(url); }
inline bool is_printables_link(const std::string& url) {
    const std::regex url_regex("(http|https)://printables.com", std::regex_constants::icase);
    return std::regex_match(url, url_regex);
}
inline bool is_makerworld_link(const std::string& url) {
    const std::regex url_regex("(http|https)://makerworld.com", std::regex_constants::icase);
    return std::regex_match(url, url_regex);
}
inline bool is_thingiverse_link(const std::string& url) {
    const std::regex url_regex("(http|https)://www.thingiverse.com", std::regex_constants::icase);
    return std::regex_match(url, url_regex);
}

// sanitize a string to be used as a filename
inline std::string sanitize_filename(const std::string &filename){
    const std::regex special_chars("[/\\\\:*?\"<>|]");
    return std::regex_replace(filename, special_chars, "_");
}
// File path / name / extension splitting utilities, working with UTF-8,
// to be published to Perl.
namespace PerlUtils {
    // Get a file name including the extension.
    extern std::string path_to_filename(const char *src);
    // Get a file name without the extension.
    extern std::string path_to_stem(const char *src);
    // Get just the extension.
    extern std::string path_to_extension(const char *src);
    // Get a directory without the trailing slash.
    extern std::string path_to_parent_path(const char *src);
};

std::string string_printf(const char *format, ...);

// Standard "generated by Slic3r version xxx timestamp xxx" header string,
// to be placed at the top of Slic3r generated files.
std::string header_slic3r_generated();

// Standard "generated by PrusaGCodeViewer version xxx timestamp xxx" header string,
// to be placed at the top of Slic3r generated files.
std::string header_gcodeviewer_generated();

// getpid platform wrapper
extern unsigned get_current_pid();
// BBS: backup & restore
std::string get_process_name(int pid);

// Compute the next highest power of 2 of 32-bit v
// http://graphics.stanford.edu/~seander/bithacks.html
inline uint16_t next_highest_power_of_2(uint16_t v)
{
    if (v != 0)
        -- v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    return ++ v;
}
inline uint32_t next_highest_power_of_2(uint32_t v)
{
    if (v != 0)
        -- v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return ++ v;
}
inline uint64_t next_highest_power_of_2(uint64_t v)
{
    if (v != 0)
        -- v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return ++ v;
}

// On some implementations (such as some versions of clang), the size_t is a type of its own, so we need to overload for size_t.
// Typically, though, the size_t type aliases to uint64_t / uint32_t.
// We distinguish that here and provide implementation for size_t if and only if it is a distinct type
template<class T> size_t next_highest_power_of_2(T v,
    typename std::enable_if<std::is_same<T, size_t>::value, T>::type = 0,     // T is size_t
    typename std::enable_if<!std::is_same<T, uint64_t>::value, T>::type = 0,  // T is not uint64_t
    typename std::enable_if<!std::is_same<T, uint32_t>::value, T>::type = 0,  // T is not uint32_t
    typename std::enable_if<sizeof(T) == 8, T>::type = 0)                     // T is 64 bits
{
    return next_highest_power_of_2(uint64_t(v));
}
template<class T> size_t next_highest_power_of_2(T v,
    typename std::enable_if<std::is_same<T, size_t>::value, T>::type = 0,     // T is size_t
    typename std::enable_if<!std::is_same<T, uint64_t>::value, T>::type = 0,  // T is not uint64_t
    typename std::enable_if<!std::is_same<T, uint32_t>::value, T>::type = 0,  // T is not uint32_t
    typename std::enable_if<sizeof(T) == 4, T>::type = 0)                     // T is 32 bits
{
    return next_highest_power_of_2(uint32_t(v));
}

template <class VectorType> void reserve_power_of_2(VectorType &vector, size_t n) {
    vector.reserve(next_highest_power_of_2(n));
}

template<class VectorType> void reserve_more(VectorType &vector, size_t n)
{
    vector.reserve(vector.size() + n);
}

template<class VectorType> void reserve_more_power_of_2(VectorType &vector, size_t n)
{
    vector.reserve(next_highest_power_of_2(vector.size() + n));
}

template<typename INDEX_TYPE>
inline INDEX_TYPE prev_idx_modulo(INDEX_TYPE idx, const INDEX_TYPE count)
{
	if (idx == 0)
		idx = count;
	return -- idx;
}

template<typename INDEX_TYPE>
inline INDEX_TYPE next_idx_modulo(INDEX_TYPE idx, const INDEX_TYPE count)
{
	if (++ idx == count)
		idx = 0;
	return idx;
}


// Return dividend divided by divisor rounded to the nearest integer
template<typename INDEX_TYPE>
inline INDEX_TYPE round_up_divide(const INDEX_TYPE dividend, const INDEX_TYPE divisor)
{
    return (dividend + divisor - 1) / divisor;
}

template<typename CONTAINER_TYPE>
inline typename CONTAINER_TYPE::size_type prev_idx_modulo(typename CONTAINER_TYPE::size_type idx, const CONTAINER_TYPE &container)
{
	return prev_idx_modulo(idx, container.size());
}

template<typename CONTAINER_TYPE>
inline typename CONTAINER_TYPE::size_type next_idx_modulo(typename CONTAINER_TYPE::size_type idx, const CONTAINER_TYPE &container)
{
	return next_idx_modulo(idx, container.size());
}

template<typename CONTAINER_TYPE>
inline const typename CONTAINER_TYPE::value_type& prev_value_modulo(typename CONTAINER_TYPE::size_type idx, const CONTAINER_TYPE &container)
{
	return container[prev_idx_modulo(idx, container.size())];
}

template<typename CONTAINER_TYPE>
inline typename CONTAINER_TYPE::value_type& prev_value_modulo(typename CONTAINER_TYPE::size_type idx, CONTAINER_TYPE &container)
{
	return container[prev_idx_modulo(idx, container.size())];
}

template<typename CONTAINER_TYPE>
inline const typename CONTAINER_TYPE::value_type& next_value_modulo(typename CONTAINER_TYPE::size_type idx, const CONTAINER_TYPE &container)
{
	return container[next_idx_modulo(idx, container.size())];
}

template<typename CONTAINER_TYPE>
inline typename CONTAINER_TYPE::value_type& next_value_modulo(typename CONTAINER_TYPE::size_type idx, CONTAINER_TYPE &container)
{
	return container[next_idx_modulo(idx, container.size())];
}

extern std::string xml_escape(std::string text, bool is_marked = false);
extern std::string xml_escape_double_quotes_attribute_value(std::string text);
extern std::string xml_unescape(std::string text);


#if defined __GNUC__ && __GNUC__ < 5 && !defined __clang__
// Older GCCs don't have std::is_trivially_copyable
// cf. https://gcc.gnu.org/onlinedocs/gcc-4.9.4/libstdc++/manual/manual/status.html#status.iso.2011
// #warning "GCC version < 5, faking std::is_trivially_copyable"
template<typename T> struct IsTriviallyCopyable { static constexpr bool value = true; };
#else
template<typename T> struct IsTriviallyCopyable : public std::is_trivially_copyable<T> {};
#endif

// A very lightweight ROII wrapper around C FILE.
// The old C file API is much faster than C++ streams, thus they are recommended for processing large / huge files.
struct FilePtr {
    FilePtr(FILE *f) : f(f) {}
    ~FilePtr() { this->close(); }
    void close() {
        if (this->f) {
            ::fclose(this->f);
            this->f = nullptr;
        }
    }
    FILE* f = nullptr;
};

class ScopeGuard
{
public:
    typedef std::function<void()> Closure;
private:
//    bool committed;
    Closure closure;

public:
    ScopeGuard() {}
    ScopeGuard(Closure closure) : closure(std::move(closure)) {}
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard &&other) : closure(std::move(other.closure)) {}

    ~ScopeGuard()
    {
        if (closure) { closure(); }
    }

    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard &&other)
    {
        closure = std::move(other.closure);
        return *this;
    }

    void reset() { closure = Closure(); }
};

// Shorten the dhms time by removing the seconds, rounding the dhm to full minutes
// and removing spaces.
inline std::string short_time(const std::string &time)
{
    // Parse the dhms time format.
    int days = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    float f_seconds = 0.0;
    if (time.find('d') != std::string::npos)
        ::sscanf(time.c_str(), "%dd %dh %dm %ds", &days, &hours, &minutes, &seconds);
    else if (time.find('h') != std::string::npos)
        ::sscanf(time.c_str(), "%dh %dm %ds", &hours, &minutes, &seconds);
    else if (time.find('m') != std::string::npos)
        ::sscanf(time.c_str(), "%dm %ds", &minutes, &seconds);
    else if (time.find('s') != std::string::npos) {
        ::sscanf(time.c_str(), "%fs", &f_seconds);
        seconds = int(f_seconds);
    }
    // Round to full minutes.
    if (days + hours > 0 && seconds >= 30) {
        if (++minutes == 60) {
            minutes = 0;
            if (++hours == 24) {
                hours = 0;
                ++days;
            }
        }
    }
    // Format the dhm time.
    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd%dh%dm", days, hours, minutes);
    else if (hours > 0)
        ::sprintf(buffer, "%dh%dm", hours, minutes);
    else if (minutes > 0)
        ::sprintf(buffer, "%dm%ds", minutes, (int)seconds);
    else if (seconds >= 1)
        ::sprintf(buffer, "%ds", (int)seconds);
    else if (f_seconds > 0 && f_seconds < 1)
        ::sprintf(buffer, "<1s");
    else if (seconds == 0)
        ::sprintf(buffer, "0s");
    return buffer;
}

// Returns the given time is seconds in format DDd HHh MMm SSs
inline std::string get_time_dhms(float time_in_secs)
{
    int days = (int)(time_in_secs / 86400.0f);
    time_in_secs -= (float)days * 86400.0f;
    int hours = (int)(time_in_secs / 3600.0f);
    time_in_secs -= (float)hours * 3600.0f;
    int minutes = (int)(time_in_secs / 60.0f);
    time_in_secs -= (float)minutes * 60.0f;

    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd %dh %dm %ds", days, hours, minutes, (int)time_in_secs);
    else if (hours > 0)
        ::sprintf(buffer, "%dh %dm %ds", hours, minutes, (int)time_in_secs);
    else if (minutes > 0)
        ::sprintf(buffer, "%dm %ds", minutes, (int)time_in_secs);
    else if (time_in_secs > 1)
        ::sprintf(buffer, "%ds", (int)time_in_secs);
    else
        ::sprintf(buffer, "%fs", time_in_secs);

    return buffer;
}

inline std::string get_bbl_time_dhms(float time_in_secs)
{
    int days = (int)(time_in_secs / 86400.0f);
    time_in_secs -= (float)days * 86400.0f;
    int hours = (int)(time_in_secs / 3600.0f);
    time_in_secs -= (float)hours * 3600.0f;
    int minutes = (int)(time_in_secs / 60.0f);
    time_in_secs -= (float)minutes * 60.0f;

    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd%dh%dm%ds", days, hours, minutes, (int)time_in_secs);
    else if (hours > 0)
        ::sprintf(buffer, "%dh%dm%ds", hours, minutes, (int)time_in_secs);
    else if (minutes > 0)
        ::sprintf(buffer, "%dm%ds", minutes, (int)time_in_secs);
    else
        ::sprintf(buffer, "%ds", (int)time_in_secs);

    return buffer;
}

inline std::string get_timezone_utc_hm(long second)
{
    bool pos = true;
    if (second < 0) {
        pos = false;
        second = -second;
    }

    int hours = (int)(second / 3600.0f);
    second -= (float)hours * 3600.0f;
    int minutes = (int)(second / 60.0f);
    second -= (float)minutes * 60.0f;

    char buffer[64];
    ::sprintf(buffer, "UTC%s%02d:%02d", pos ? "+" : "-", hours, minutes);
    return buffer;
}

inline std::string get_time_dhm(float time_in_secs)
{
    int days = (int)(time_in_secs / 86400.0f);
    time_in_secs -= (float)days * 86400.0f;
    int hours = (int)(time_in_secs / 3600.0f);
    time_in_secs -= (float)hours * 3600.0f;
    int minutes = (int)(time_in_secs / 60.0f);

    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd %dh %dm", days, hours, minutes);
    else if (hours > 0)
        ::sprintf(buffer, "%dh %dm", hours, minutes);
    else if (minutes > 0)
        ::sprintf(buffer, "%dm", minutes);
    else
        ::sprintf(buffer, "%dm", 0);

    return buffer;
}

inline std::string get_time_hms(float time_in_secs)
{
    int hours = (int)(time_in_secs / 3600.0f);
    time_in_secs -= (float)hours * 3600.0f;
    int minutes = (int)(time_in_secs / 60.0f);
    time_in_secs -= (float)minutes * 60.0f;
    int secs = (int)time_in_secs;

    char buffer[64];
    ::sprintf(buffer, "%02d:%02d:%02d", hours, minutes, secs);
    return buffer;
}

inline std::string get_bbl_monitor_time_dhm(float time_in_secs)
{
    int days = (int)(time_in_secs / 86400.0f);
    time_in_secs -= (float)days * 86400.0f;
    int hours = (int)(time_in_secs / 3600.0f);
    time_in_secs -= (float)hours * 3600.0f;
    int minutes = (int)(time_in_secs / 60.0f);

    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd%dh%dm", days, hours, minutes);
    else if (hours > 0)
        ::sprintf(buffer, "%dh%dm", hours, minutes);
    else if (minutes >= 0)
        ::sprintf(buffer, "%dm", minutes);
    else {
        return "";
    }

    return buffer;
}

inline std::string get_bbl_monitor_end_time_dhm(float time_in_secs)
{
    if (time_in_secs == 0.0f)
        return {};

    std::stringstream stream;
    boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
    auto endTime = now + boost::posix_time::seconds(static_cast<int>(time_in_secs));
    auto facet = new boost::posix_time::time_facet("%H:%M");//%Y-%m-%d %H:%M:%S
    stream.imbue(std::locale(std::locale::classic(), facet));
    stream << endTime;

    return stream.str();
}

inline std::string get_bbl_remain_time_dhms(float time_in_secs)
{
    int days = (int) (time_in_secs / 86400.0f);
    time_in_secs -= (float) days * 86400.0f;
    int hours = (int) (time_in_secs / 3600.0f);
    time_in_secs -= (float) hours * 3600.0f;
    int minutes = (int) (time_in_secs / 60.0f);
    time_in_secs -= (float) minutes * 60.0f;

    char buffer[64];
    if (days > 0)
        ::sprintf(buffer, "%dd%dh%dm%ds", days, hours, minutes, (int) time_in_secs);
    else if (hours > 0)
        ::sprintf(buffer, "%dh%dm%ds", hours, minutes, (int) time_in_secs);
    else if (minutes > 0)
        ::sprintf(buffer, "%dm%ds", minutes, (int) time_in_secs);
    else
        ::sprintf(buffer, "%ds", (int) time_in_secs);

    return buffer;
}

bool bbl_calc_md5(std::string &filename, std::string &md5_out);

inline std::string filter_characters(const std::string& str, const std::string& filterChars)
{
    std::string filteredStr = str;

    auto removeFunc = [&filterChars](char ch) {
        return filterChars.find(ch) != std::string::npos;
    };

    filteredStr.erase(std::remove_if(filteredStr.begin(), filteredStr.end(), removeFunc), filteredStr.end());

    return filteredStr;
}

bool copy_directory_recursively(const boost::filesystem::path &source, const boost::filesystem::path &target, std::function<bool(const std::string)> filter = nullptr);

// Copy source tree to target + ".new", run validate_staging, then rename into place (target + ".old" rollback on failure).
bool atomic_replace_directory(
    const boost::filesystem::path &source,
    const boost::filesystem::path &target,
    std::function<bool(const std::string)> filter,
    std::function<bool(const boost::filesystem::path &staging)> validate_staging);

// Orca: Since 1.7.9 Boost deprecated save_string_file and load_string_file, copy and modified from boost 1.7.8
void save_string_file(const boost::filesystem::path& p, const std::string& str);
void load_string_file(const boost::filesystem::path& p, std::string& str);

bool check_layer_id_pattern(const std::string& pattern, int layer_id);

} // namespace Slic3r

#if WIN32
    #define SLIC3R_STDVEC_MEMSIZE(NAME, TYPE) NAME.capacity() * ((sizeof(TYPE) + __alignof(TYPE) - 1) / __alignof(TYPE)) * __alignof(TYPE)
    //FIXME this is an inprecise hack. Add the hash table size and possibly some estimate of the linked list at each of the used bin.
    #define SLIC3R_STDUNORDEREDSET_MEMSIZE(NAME, TYPE) NAME.size() * ((sizeof(TYPE) + __alignof(TYPE) - 1) / __alignof(TYPE)) * __alignof(TYPE)
#else
    #define SLIC3R_STDVEC_MEMSIZE(NAME, TYPE) NAME.capacity() * ((sizeof(TYPE) + alignof(TYPE) - 1) / alignof(TYPE)) * alignof(TYPE)
    //FIXME this is an inprecise hack. Add the hash table size and possibly some estimate of the linked list at each of the used bin.
    #define SLIC3R_STDUNORDEREDSET_MEMSIZE(NAME, TYPE) NAME.size() * ((sizeof(TYPE) + alignof(TYPE) - 1) / alignof(TYPE)) * alignof(TYPE)
#endif

#endif // slic3r_Utils_hpp_
