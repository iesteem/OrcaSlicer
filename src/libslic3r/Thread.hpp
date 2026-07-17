#ifndef GUI_THREAD_HPP
#define GUI_THREAD_HPP

#include <utility>
#include <string>
#include <thread>
#include <boost/thread.hpp>

namespace Slic3r {

// 设置/获取线程名称。
// 如果不支持该API则返回false。
//
// 建议在生成子线程之前命名主线程，因为在Windows 10上使用动态链接
// 来初始化Get/SetThreadDescription函数，这不是线程安全的。
//
// pthread_setname_np最多支持15个字符的线程名！（第16个字符是空终止符）
//
// 使用线程作为参数的方法在OSX上不支持。
// 仅在较新的Windows 10上支持命名线程。

bool set_thread_name(std::thread &thread, const char *thread_name);
inline bool set_thread_name(std::thread &thread, const std::string &thread_name) { return set_thread_name(thread, thread_name.c_str()); }
bool set_thread_name(boost::thread &thread, const char *thread_name);
inline bool set_thread_name(boost::thread &thread, const std::string &thread_name) { return set_thread_name(thread, thread_name.c_str()); }
bool set_current_thread_name(const char *thread_name);
inline bool set_current_thread_name(const std::string &thread_name) { return set_current_thread_name(thread_name.c_str()); }

// 在应用程序启动时调用，将当前线程ID保存为主（UI）线程ID。
void save_main_thread_id();
// 检索缓存的主（UI）线程ID。
boost::thread::id get_main_thread_id();
// 检查主（UI）线程是否处于活动状态。
bool is_main_thread_active();

// 如果不支持则返回nullopt。
// OSX不支持。
// 仅在较新的Windows 10上支持命名线程。
std::optional<std::string> get_current_thread_name();

// 在TBB线程首次启动之前的某个地方调用，以便
// 为它们提供调试器中可识别的名称。
// 同时它将工作线程的区域设置为"C"，以便G-code生成器生成"."作为小数分隔符。
void name_tbb_thread_pool_threads_set_locale();

template<class Fn>
inline boost::thread create_thread(boost::thread::attributes &attrs, Fn &&fn)
{
    // 复制线程池中TBB工作线程的栈分配大小：在64位系统上分配4MB，在32位系统上分配2MB
    // 默认设置。
    
    attrs.set_stack_size((sizeof(void*) == 4) ? (2048 * 1024) : (4096 * 1024));
    return boost::thread{attrs, std::forward<Fn>(fn)};
}

template<class Fn> inline boost::thread create_thread(Fn &&fn)
{
    boost::thread::attributes attrs;
    return create_thread(attrs, std::forward<Fn>(fn));    
}

}

#endif // GUI_THREAD_HPP
