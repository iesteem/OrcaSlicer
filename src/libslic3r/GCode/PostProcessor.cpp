#include "PostProcessor.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/format.hpp"
#include "libslic3r/I18N.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/cstdlib.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/nowide/fstream.hpp>

// BBS
#include <iostream>
#include <fstream>

#ifdef WIN32

// 标准Windows包含。
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

// https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
// 此例程将给定的参数追加到命令行，使得CommandLineToArgvW返回不变的参数字符串。
// 命令行中的参数应该用空格分隔；此函数不添加这些空格。
// Argument    - 提供要编码的参数。
// CommandLine - 提供我们要追加编码参数字符串的命令行。
static void quote_argv_winapi(const std::wstring &argument, std::wstring &commmand_line_out)
{
    // 除非确实需要，否则不加引号---希望避免程序无法正确解析引号的问题。
    if (argument.empty() == false && argument.find_first_of(L" \t\n\v\"") == argument.npos)
        commmand_line_out.append(argument);
    else {
        commmand_line_out.push_back(L'"');
        for (auto it = argument.begin(); ; ++ it) {
            unsigned number_backslashes = 0;
            while (it != argument.end() && *it == L'\\') {
                ++ it;
                ++ number_backslashes;
            }
            if (it == argument.end()) {
                // 转义所有反斜杠，但让我们要添加的终止双引号被解释为元字符。
                commmand_line_out.append(number_backslashes * 2, L'\\');
                break;
            } else if (*it == L'"') {
                // 转义所有反斜杠和后面的双引号。
                commmand_line_out.append(number_backslashes * 2 + 1, L'\\');
                commmand_line_out.push_back(*it);
            } else {
                // 反斜杠在这里并不特殊。
                commmand_line_out.append(number_backslashes, L'\\');
                commmand_line_out.push_back(*it);
            }
        }
        commmand_line_out.push_back(L'"');
    }
}

static DWORD execute_process_winapi(const std::wstring &command_line)
{
    // 提取当前环境以传递给子进程。
    std::wstring envstr;
    {
        wchar_t *env = GetEnvironmentStrings();
        assert(env != nullptr);
        const wchar_t* var = env;
        size_t totallen = 0;
        size_t len;
        while ((len = wcslen(var)) > 0) {
            totallen += len + 1;
            var += len + 1;
        }
        envstr = std::wstring(env, totallen);
        FreeEnvironmentStrings(env);
    }

    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb             = sizeof(STARTUPINFO);
#if 0
    startup_info.dwFlags     = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_HIDE;
#endif
    PROCESS_INFORMATION process_info;
    if (! ::CreateProcessW(
            nullptr /* lpApplicationName */, (LPWSTR)command_line.c_str(), nullptr /* lpProcessAttributes */, nullptr /* lpThreadAttributes */, false /* bInheritHandles */,
                CREATE_UNICODE_ENVIRONMENT /* | CREATE_NEW_CONSOLE */ /* dwCreationFlags */, (LPVOID)envstr.c_str(), nullptr /* lpCurrentDirectory */, &startup_info, &process_info))
        throw Slic3r::RuntimeError(std::string("启动脚本失败 ") + boost::nowide::narrow(command_line) + ", Win32错误: " + std::to_string(int(::GetLastError())));
    ::WaitForSingleObject(process_info.hProcess, INFINITE);
    ULONG rc = 0;
    ::GetExitCodeProcess(process_info.hProcess, &rc);
    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
    return rc;
}

// 运行脚本。如果是perl脚本，通过捆绑的perl解释器运行。
// 如果是批处理文件，通过cmd.exe运行。
// 否则直接运行。
static int run_script(const std::string &script, const std::string &gcode, std::string &/*std_err*/)
{
    // 解包用户提供的参数列表。
    int     nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(boost::nowide::widen(script).c_str(), &nArgs);
    if (szArglist == nullptr || nArgs <= 0) {
        // CommandLineToArgvW失败。可能命令行转义无效？
        throw Slic3r::RuntimeError(std::string("后处理脚本 ") + script + " 在文件 " + gcode + " 上失败。CommandLineToArgvW()拒绝解析命令行路径。");
    }

    std::wstring command_line;
    std::wstring command = szArglist[0];
    if (! boost::filesystem::exists(boost::filesystem::path(command)))
        throw Slic3r::RuntimeError(std::string("配置的后处理脚本不存在: ") + boost::nowide::narrow(command));
    if (boost::iends_with(command, L".pl")) {
        // 这是一个perl脚本。通过perl解释器运行。
        // 当前进程可能是slic3r.exe或slic3r-console.exe。
        // 查找进程的路径：
        wchar_t wpath_exe[_MAX_PATH + 1];
        ::GetModuleFileNameW(nullptr, wpath_exe, _MAX_PATH);
        boost::filesystem::path path_exe(wpath_exe);
        boost::filesystem::path path_perl = path_exe.parent_path() / "perl" / "perl.exe";
        if (! boost::filesystem::exists(path_perl)) {
            LocalFree(szArglist);
            throw Slic3r::RuntimeError(std::string("Perl解释器 ") + path_perl.string() + " 不存在。");
        }
        // 用当前的perl解释器替换它。
        quote_argv_winapi(boost::nowide::widen(path_perl.string()), command_line);
        command_line += L" ";
    } else if (boost::iends_with(command, ".bat")) {
        // 通过命令行解释器运行批处理文件。
        command_line = L"cmd.exe /C ";
    }

    for (int i = 0; i < nArgs; ++ i) {
        quote_argv_winapi(szArglist[i], command_line);
        command_line += L" ";
    }
    LocalFree(szArglist);
    quote_argv_winapi(boost::nowide::widen(gcode), command_line);
    return (int)execute_process_winapi(command_line);
}

#else
    // POSIX

#include <cstdlib>   // getenv()
#include <sstream>
#include <boost/process.hpp>

namespace process = boost::process;

static int run_script(const std::string &script, const std::string &gcode, std::string &std_err)
{
    // 尝试获取用户的默认shell
    const char *shell = ::getenv("SHELL");
    if (shell == nullptr) { shell = "/bin/sh"; }

    // 引用和转义gcode路径参数
    std::string command { script };
    command.append(" '");
    for (char c : gcode) {
        if (c == '\'') { command.append("'\\''"); }
        else { command.push_back(c); }
    }
    command.push_back('\'');

    BOOST_LOG_TRIVIAL(debug) << boost::format("执行脚本, shell: %1%, 命令: %2%") % shell % command;

    process::ipstream istd_err;
    process::child child(shell, "-c", command, process::std_err > istd_err);

    std_err.clear();
    std::string line;

    while (child.running() && std::getline(istd_err, line)) {
        std_err.append(line);
        std_err.push_back('\n');
    }

    child.wait();
    return child.exit_code();
}

#endif

namespace Slic3r {

//! 用于标记本地化字符串的宏，
//! 返回相同的字符串
#define L(s) (s)
#define _(s) Slic3r::I18N::translate(s)

// BBS
void gcode_add_line_number(const std::string& path, const DynamicPrintConfig& config)
{
    const ConfigOptionBool* opt = config.opt<ConfigOptionBool>("gcode_add_line_number");
    if (!opt->getBool())
        return;

    auto gcode_file = boost::filesystem::path(path);
    if (!boost::filesystem::exists(gcode_file))
        return;

    std::fstream fs;
    std::string new_gcode;
    fs.open(gcode_file.c_str(), std::fstream::in | std::fstream::out);

    size_t line_number = 1;
    std::string gcode_line;
    while (std::getline(fs, gcode_line)) {
        char num_str[128];
        memset(num_str, 0, sizeof(num_str));
        snprintf(num_str, sizeof(num_str), "%zd", line_number);
        new_gcode += std::string("N") + num_str + " " + gcode_line + "\n";
        line_number++;
    }

    fs.clear();
    fs.seekp(0, std::ios_base::beg);
    fs.write(new_gcode.c_str(), new_gcode.length());
    fs.close();
}

// 如果定义了后处理脚本，则运行该脚本。
// 如果后处理脚本被执行，则返回true。
// 如果没有定义后处理脚本，则返回false。
// 出错时抛出异常。
// host是"File"、"PrusaLink"、"Repetier"、"SL1Host"、"OctoPrint"、"FlashAir"、"Duet"、"AstroBox"...
// 对于"File"目标，通过添加".pp"后缀为src_path创建临时文件，并更新src_path。
// 在这种情况下，调用者负责删除创建的临时文件。
// output_name是G-code在SD卡上或上传到PrusaLink或OctoPrint时的最终名称。
// 如果上传到PrusaLink或OctoPrint，则文件首先在目标主机上重命名为output_name。
// 后处理脚本可能会更改output_name。
bool run_post_process_scripts(std::string &src_path, bool make_copy, const std::string &host, std::string &output_name, const DynamicPrintConfig &config)
{
    const auto *post_process = config.opt<ConfigOptionStrings>("post_process");
    if (// 可能以SLA模式运行
        post_process == nullptr ||
        // 没有后处理脚本
        post_process->values.empty())
        return false;

    std::string path;
    if (make_copy) {
        // 不要在输入文件上运行后处理脚本，它将被G-code查看器内存映射。
        // 制作一个副本。
        path = src_path + ".pp";
        // 首先删除旧文件（如果存在）。
        try {
            if (boost::filesystem::exists(path))
                boost::filesystem::remove(path);
        } catch (const std::exception &err) {
            BOOST_LOG_TRIVIAL(error) << Slic3r::format("在运行后处理脚本之前删除旧临时文件 %1% 失败: %2%", path, err.what());
        }
        // 其次制作副本。
        std::string error_message;
        if (copy_file(src_path, path, error_message, false) != SUCCESS)
            throw Slic3r::RuntimeError(Slic3r::format("在运行后处理脚本之前制作G-code文件 %1% 的临时副本失败: %2%", src_path, error_message));
    } else {
        // 不制作运行后处理脚本之前的G-code副本。
        path = src_path;
    }

    auto delete_copy = [&path, &src_path, make_copy]() {
        if (make_copy)
            try {
                if (boost::filesystem::exists(path))
                    boost::filesystem::remove(path);
            } catch (const std::exception &err) {
                BOOST_LOG_TRIVIAL(error) << Slic3r::format("删除G-code文件 %2% 的临时副本 %1% 失败: %3%", path, src_path, err.what());
            }
    };

    auto gcode_file = boost::filesystem::path(path);
    if (! boost::filesystem::exists(gcode_file))
        throw Slic3r::RuntimeError(std::string("后处理器找不到导出的gcode文件"));

    // 将打印配置存储到环境变量中。
    config.setenv_();
    // 让后处理脚本知道目标主机（"File"、"PrusaLink"、"Repetier"、"SL1Host"、"OctoPrint"、"FlashAir"、"Duet"、"AstroBox"...）
    boost::nowide::setenv("SLIC3R_PP_HOST", host.c_str(), 1);
    // 让后处理脚本知道最终文件名。对于"File"主机，它是目标文件名的完整路径及其位置，例如指向SD卡。
    // 对于"PrusaLink"或"OctoPrint"，它是文件名可选地带目标主机上的目录。
    boost::nowide::setenv("SLIC3R_PP_OUTPUT_NAME", output_name.c_str(), 1);

    // 后处理脚本可能创建并填充一个可选文件的路径，该文件包含单行的output_name替换。
    std::string path_output_name = path + ".output_name";
    auto remove_output_name_file = [&path_output_name, &src_path]() {
        try {
            if (boost::filesystem::exists(path_output_name))
                boost::filesystem::remove(path_output_name);
        } catch (const std::exception &err) {
            BOOST_LOG_TRIVIAL(error) << Slic3r::format("删除携带G-code文件 %2% 的最终名称/路径的文件 %1% 失败: %3%", path_output_name, src_path, err.what());
        }
    };
    // 删除上次运行可能遗留的path_output_name。
    remove_output_name_file();

    try {
        for (const std::string &scripts : post_process->values) {
            std::vector<std::string> lines;
            boost::split(lines, scripts, boost::is_any_of("\r\n"));
            for (std::string script : lines) {
                // 忽略空的后处理脚本行。
                boost::trim(script);
                if (script.empty())
                    continue;
                BOOST_LOG_TRIVIAL(info) << "在文件 " << path << " 上执行脚本 " << script;
                std::string std_err;
                const int result = run_script(script, gcode_file.string(), std_err);
                if (result != 0) {
                    const std::string msg = std_err.empty() ? (boost::format("后处理脚本 %1% 在文件 %2% 上失败。\n错误代码: %3%") % script % path % result).str()
                        : (boost::format("后处理脚本 %1% 在文件 %2% 上失败。\n错误代码: %3%\n输出:\n%4%") % script % path % result % std_err).str();
                    BOOST_LOG_TRIVIAL(error) << msg;
                    delete_copy();
                    throw Slic3r::RuntimeError(msg);
                }
                if (! boost::filesystem::exists(gcode_file)) {
                    const std::string msg = (boost::format(_(L(
                        "后处理脚本 %1% 失败。\n\n"
                        "后处理脚本应原地更改G-code文件 %2%，但该G-code文件已被删除并可能以新名称保存。\n"
                        "请调整后处理脚本以原地更改G-code，并查阅手册了解如何可选地重命名后处理后的G-code文件。\n")))
                        % script % path).str();
                    BOOST_LOG_TRIVIAL(error) << msg;
                    throw Slic3r::RuntimeError(msg);
                }
            }
        }
        if (boost::filesystem::exists(path_output_name)) {
            try {
                // 从path_output_name读取单行，应包含后处理G-code的新输出名称。
                boost::nowide::fstream f;
                f.open(path_output_name, std::ios::in);
                std::string new_output_name;
                std::getline(f, new_output_name);
                f.close();

                if (host == "File") {
                    namespace fs = boost::filesystem;
                    fs::path op(new_output_name);
                    if (op.is_relative() && op.has_filename() && op.parent_path().empty()) {
                        // 这只是文件名吗？使其成为绝对路径。
                        auto outpath = fs::path(output_name).parent_path();
                        outpath /= op.string();
                        new_output_name = outpath.string();
                    }
                    else {
                        if (! op.is_absolute() || ! op.has_filename())
                            throw Slic3r::RuntimeError("无法从输出名称文件解析所需的新路径");
                    }
                    if (! fs::exists(fs::path(new_output_name).parent_path()))
                        throw Slic3r::RuntimeError(Slic3r::format("输出目录不存在: %1%",
                                                                  fs::path(new_output_name).parent_path().string()));
                }

                BOOST_LOG_TRIVIAL(trace) << "后处理脚本将文件名从 " << output_name << " 更改为 " << new_output_name;
                output_name = new_output_name;
            } catch (const std::exception &err) {
                throw Slic3r::RuntimeError(Slic3r::format("run_post_process_scripts: 读取文件 %1% 失败，"
                                                          "该文件携带G-code文件的最终名称/路径: %2%",
                                                          path_output_name, err.what()));
            }
            remove_output_name_file();
        }
    } catch (...) {
        remove_output_name_file();
        delete_copy();
        throw;
    }

    src_path = std::move(path);
    return true;
}

} // namespace Slic3r
