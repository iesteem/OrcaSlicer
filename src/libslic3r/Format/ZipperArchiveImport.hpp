#ifndef ZIPPERARCHIVEIMPORT_HPP
#define ZIPPERARCHIVEIMPORT_HPP

#include <vector>
#include <string>
#include <cstdint>

#include <boost/property_tree/ptree.hpp>

#include "libslic3r/PrintConfig.hpp"

namespace Slic3r {

// zipper存档中任意文件的缓冲区。
struct EntryBuffer
{
    std::vector<uint8_t> buf;
    std::string          fname;
};

// 保存从zipper存档读取的数据的结构体。
struct ZipperArchive
{
    boost::property_tree::ptree profile, config;
    std::vector<EntryBuffer>    entries;
};

// 存档中包含元数据的文件名。
const constexpr char *CONFIG_FNAME  = "config.ini";
const constexpr char *PROFILE_FNAME = "prusaslicer.ini";

// 读取使用Zipper类写入的存档。
// includes参数是一组文件名字符串，条目必须包含这些子串才能被包含在ZipperArchive中。
// excludes参数可以包含文件名不得包含的子串。
// 存档中的每个文件都会被读入ZipperArchive::entries，
// 但CONFIG_FNAME和PROFILE_FNAME文件除外，它们会被读入
// ZipperArchive::config和ZipperArchive::profile结构体中。
ZipperArchive read_zipper_archive(const std::string &zipfname,
                                  const std::vector<std::string> &includes,
                                  const std::vector<std::string> &excludes);

// 从存档中提取打印配置文件到'out'中。
// 返回一个具有正确参数的配置文件，用于模型重建，
// 即使在存档的元数据中未完全找到所需参数。
// 如果存档的元数据完全损坏，inout参数应作为一个可用的后备配置文件。
std::pair<DynamicPrintConfig, ConfigSubstitutions> extract_profile(
    const ZipperArchive &arch, DynamicPrintConfig &inout);

} // namespace Slic3r

#endif // ZIPPERARCHIVEIMPORT_HPP
