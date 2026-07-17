#ifndef slic3r_Utils_Time_hpp_
#define slic3r_Utils_Time_hpp_

#include <string>
#include <ctime>

namespace Slic3r {
namespace Utils {

// 应为线程安全。
time_t get_current_time_utc();

enum class TimeZone { local, utc };
enum class TimeFormat { gcode, iso8601Z };

// time_t到字符串的函数...

std::string time2str(const time_t &t, TimeZone zone, TimeFormat fmt);

inline std::string time2str(TimeZone zone, TimeFormat fmt)
{
    return time2str(get_current_time_utc(), zone, fmt);
}

inline std::string utc_timestamp(time_t t)
{
    return time2str(t, TimeZone::utc, TimeFormat::gcode);
}

inline std::string utc_timestamp()
{
    return utc_timestamp(get_current_time_utc());
}

inline std::string local_timestamp(TimeFormat fmt = TimeFormat::gcode) {
     return time2str(get_current_time_utc(), TimeZone::local, fmt);
}

// 字符串到time_t的函数。如果解析输入失败，则返回time_t(-1)。
time_t str2time(const std::string &str, TimeZone zone, TimeFormat fmt);


// /////////////////////////////////////////////////////////////////////////////
// 用于在UTC time_t和ISO8601时间格式之间转换的实用程序，
// 用于将时间戳放入文件和目录名称中。
// 出错时返回(time_t)-1。

// 使用这些函数在所有平台上安全地与ISO8601格式进行相互转换

inline std::string iso_utc_timestamp(time_t t)
{
    return time2str(t, TimeZone::utc, TimeFormat::iso8601Z);
}

inline std::string iso_utc_timestamp()
{
    return iso_utc_timestamp(get_current_time_utc());
}

inline time_t parse_iso_utc_timestamp(const std::string &str)
{
    return str2time(str, TimeZone::utc, TimeFormat::iso8601Z);
}

// /////////////////////////////////////////////////////////////////////////////

} // namespace Utils
} // namespace Slic3r

#endif /* slic3r_Utils_Time_hpp_ */
