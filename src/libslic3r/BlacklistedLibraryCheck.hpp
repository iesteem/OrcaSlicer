#ifndef slic3r_BlacklistedLibraryCheck_hpp_
#define slic3r_BlacklistedLibraryCheck_hpp_

#ifdef  WIN32
#include <windows.h>
#include <vector>
#include <string>
#endif //WIN32

namespace Slic3r {

#ifdef  WIN32
class BlacklistedLibraryCheck
{
public:
    static BlacklistedLibraryCheck& get_instance()
    {
        static BlacklistedLibraryCheck instance; 
                              
        return instance;
    }
private:
    BlacklistedLibraryCheck() = default;

    std::vector<std::wstring> m_found;
public:
    BlacklistedLibraryCheck(BlacklistedLibraryCheck const&) = delete;
    void operator=(BlacklistedLibraryCheck const&) = delete;
    // 返回所有找到的黑名单 dll
    bool get_blacklisted(std::vector<std::wstring>& names);
    std::wstring get_blacklisted_string();
    // 如果枚举找到黑名单 dll 则返回 true
    bool perform_check();

    // UTF-8 encoded path
    static bool is_blacklisted(const std::string &dllpath);
    static bool is_blacklisted(const std::wstring &dllpath);
private:
    static const std::vector<std::wstring> blacklist;
};

#endif //WIN32

} // namespace Slic3r

#endif //slic3r_BlacklistedLibraryCheck_hpp_
