#ifndef libslic3r_Timer_hpp_
#define libslic3r_Timer_hpp_

#include <string>
#include <chrono>

namespace Slic3r {

/// <summary>
/// 此类的实例用于测量代码块的时间消耗，
/// 直到实例销毁并将结果写入调试输出
/// </summary>
class Timer
{
    std::string m_name;
    std::chrono::steady_clock::time_point m_start;
public:
    /// <summary>
    /// 描述计时器的名称
    /// </summary>
    /// <param name="name">在控制台日志中描述计时器
    Timer(const std::string& name);

    /// <summary>
    /// 描述计时器的名称
    /// </summary>
    ~Timer();
};

namespace Timing {

    // 来自Catch2单元测试库的计时代码
    static inline uint64_t nanoseconds_since_epoch() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    // 来自Catch2单元测试库的计时代码
    class Timer {
    public:
        void start() {
            m_nanoseconds = nanoseconds_since_epoch();
        }
        uint64_t elapsed_nanoseconds() const {
            return nanoseconds_since_epoch() - m_nanoseconds;
        }
        uint64_t elapsed_microseconds() const {
            return elapsed_nanoseconds() / 1000;
        }
        unsigned int elapsed_milliseconds() const {
            return static_cast<unsigned int>(elapsed_microseconds()/1000);
        }
        double elapsed_seconds() const {
            return elapsed_microseconds() / 1000000.0;
        }
    private:
        uint64_t m_nanoseconds = 0;
    };

    // 如果此计时对象的生命周期超过限制，则发出Boost::log错误。
    class TimeLimitAlarm {
    public:
        TimeLimitAlarm(uint64_t time_limit_nanoseconds, std::string_view limit_exceeded_message) :
            m_time_limit_nanoseconds(time_limit_nanoseconds), m_limit_exceeded_message(limit_exceeded_message) { 
            m_timer.start();
        }
        ~TimeLimitAlarm() {
            auto elapsed = m_timer.elapsed_nanoseconds();
            if (elapsed > m_time_limit_nanoseconds)
                this->report_time_exceeded();
        }
        static TimeLimitAlarm new_nanos(uint64_t time_limit_nanoseconds, std::string_view limit_exceeded_message) {
            return TimeLimitAlarm(time_limit_nanoseconds, limit_exceeded_message);
        }
        static TimeLimitAlarm new_milis(uint64_t time_limit_milis, std::string_view limit_exceeded_message) {
            return TimeLimitAlarm(uint64_t(time_limit_milis) * 1000000l, limit_exceeded_message);
        }
        static TimeLimitAlarm new_seconds(uint64_t time_limit_seconds, std::string_view limit_exceeded_message) {
            return TimeLimitAlarm(uint64_t(time_limit_seconds) * 1000000000l, limit_exceeded_message);
        }
    private:
        void report_time_exceeded() const;

        Timer               m_timer;
        uint64_t            m_time_limit_nanoseconds;
        std::string_view    m_limit_exceeded_message;
    };

} // namespace Catch

} // namespace Slic3r

#endif // libslic3r_Timer_hpp_
