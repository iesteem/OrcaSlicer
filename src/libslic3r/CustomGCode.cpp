#include "CustomGCode.hpp"
#include "Config.hpp"
#include "GCode.hpp"
#include "GCodeWriter.hpp"

namespace Slic3r {

namespace CustomGCode {

//BBS: 无用的配置和函数
#if 0
// 如果加载的配置包含"colorprint_heights"选项（如果从旧版Slicer导入），
// 并且 CustomGCode::Info.gcodes 为空（没有以新格式提供的颜色打印数据），
// 则 CustomGCode::Info.gcodes 应根据此选项进行更新。
extern void update_custom_gcode_per_print_z_from_config(Info& info, DynamicPrintConfig* config)
{
	auto *colorprint_heights = config->option<ConfigOptionFloats>("colorprint_heights");
    if (colorprint_heights == nullptr)
        return;
    if (info.gcodes.empty() && ! colorprint_heights->values.empty()) {
		// 仅当没有新格式的等效数据时，才转换旧颜色打印高度。
        const std::vector<std::string>& colors = ColorPrintColors::get();
        const auto& colorprint_values = colorprint_heights->values;
        info.gcodes.clear();
        info.gcodes.reserve(colorprint_values.size());
        int i = 0;
        for (auto val : colorprint_values)
            info.gcodes.emplace_back(Item{ val, ColorChange, 1, colors[(++i)%7] });

        info.mode = SingleExtruder;
	}

	// The "colorprint_heights" config value has been deprecated. At this point of time it has been converted
	// to a new format and therefore it shall be erased.
    config->erase("colorprint_heights");
}
#endif

// 如果按打印高度自定义Gcode的信息是从旧版Slicer导入的，模式将是未定义的。
// 因此，我们应根据项中的代码值更新 CustomGCode::Info.mode。
extern void check_mode_for_custom_gcode_per_print_z(Info& info)
{
    if (info.mode != Undef)
        return;

    bool is_single_extruder = true;
    for (auto item : info.gcodes) 
    {
        if (item.type == ToolChange) {
            info.mode = MultiAsSingle;
            return;
        }
        if (item.type == ColorChange && item.extruder > 1)
            is_single_extruder = false;
    }

    info.mode = is_single_extruder ? SingleExtruder : MultiExtruder;
}

// 返回按打印Z排序的<print_z, 1-based 耗材ID>对
// 来自 custom_gcode_per_print_z。
std::vector<std::pair<double, unsigned int>> custom_tool_changes(const Info& custom_gcode_per_print_z, size_t num_filaments)
{
    std::vector<std::pair<double, unsigned int>> custom_tool_changes;
    for (const Item& custom_gcode : custom_gcode_per_print_z.gcodes)
        if (custom_gcode.type == ToolChange) {
            // 如果可用的耗材槽发生变化，对于超出当前物理+混合范围的过期ID，回退到耗材1。
            assert(custom_gcode.extruder >= 0);
            custom_tool_changes.emplace_back(custom_gcode.print_z, static_cast<unsigned int>(size_t(custom_gcode.extruder) > num_filaments ? 1 : custom_gcode.extruder));
        }
    return custom_tool_changes;
}

} // namespace CustomGCode

} // namespace Slic3r
