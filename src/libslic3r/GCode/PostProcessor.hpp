#ifndef slic3r_GCode_PostProcessor_hpp_
#define slic3r_GCode_PostProcessor_hpp_

#include <string>

#include "../libslic3r.h"
#include "../PrintConfig.hpp"

namespace Slic3r {

// 如果定义了后处理脚本，则运行该脚本。
// 如果后处理脚本被执行，则返回true。
// 如果没有定义后处理脚本，则返回false。
// 出错时抛出异常。
// host是"File"、"PrusaLink"、"Repetier"、"SL1Host"、"OctoPrint"、"FlashAir"、"Duet"、"AstroBox"...
// 如果make_copy，则通过添加".pp"后缀为src_path创建临时文件，并更新src_path。
// 在这种情况下，调用者负责删除创建的临时文件。
// output_name是G-code在SD卡上或上传到PrusaLink或OctoPrint时的最终名称。
// 如果上传到PrusaLink或OctoPrint，则文件首先在目标主机上重命名为output_name。
// 后处理脚本可能会更改output_name。
extern bool run_post_process_scripts(std::string &src_path, bool make_copy, const std::string &host, std::string &output_name, const DynamicPrintConfig &config);

inline bool run_post_process_scripts(std::string &src_path, const DynamicPrintConfig &config)
{
    std::string src_path_name = src_path;
    return run_post_process_scripts(src_path, false, "File", src_path_name, config);
}

// BBS
extern void gcode_add_line_number(const std::string &path, const DynamicPrintConfig &config);

} // namespace Slic3r

#endif /* slic3r_GCode_PostProcessor_hpp_ */
