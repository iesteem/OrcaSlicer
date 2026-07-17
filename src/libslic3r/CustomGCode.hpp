#ifndef slic3r_CustomGCode_hpp_
#define slic3r_CustomGCode_hpp_

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r {

class DynamicPrintConfig;

namespace CustomGCode {

enum Type
{
    ColorChange,
    PausePrint,
    ToolChange,
    Template,
    Custom,
    Unknown,
};

struct Item
{
    bool operator<(const Item& rhs) const { return this->print_z < rhs.print_z; }
    bool operator==(const Item& rhs) const
    {
        return (rhs.print_z   == this->print_z    ) &&
               (rhs.type      == this->type       ) &&
               (rhs.extruder  == this->extruder   ) &&
               (rhs.color     == this->color      ) &&
               (rhs.extra     == this->extra      );
    }
    bool operator!=(const Item& rhs) const { return ! (*this == rhs); }
    
    double      print_z;
    Type        type;
    int         extruder;   // ColorChangeCode 和 ToolChangeCode 的参考值
                            // "gcode" == ColorChangeCode   => M600 将应用于 "extruder" 挤出机
                            // "gcode" == ToolChangeCode    => 整个打印工具将切换到 "extruder" 挤出机
    std::string color;      // 如果 gcode 等于 PausePrintCode，
                            // 此字段用于保存在打印机显示屏上显示的短消息
    std::string extra;      // 此字段用于额外数据，例如：
                            // - Type::Custom 的 G-code 文本
                            // - Type::PausePrint 的消息文本
    void from_json(const nlohmann::json& j) {
        std::string type_str;
        j.at("type").get_to(type_str);
        std::map<std::string,Type> str2type = { {"ColorChange", ColorChange },
            {"PausePrint",PausePrint},
            {"ToolChange",ToolChange},
            {"Template",Template},
            {"Custom",Custom},
            {"Unknown",Unknown} };
        type = Unknown;
        if (str2type.find(type_str) != str2type.end())
            type = str2type[type_str];
        j.at("print_z").get_to(print_z);
        j.at("color").get_to(color);
        j.at("extruder").get_to(extruder);
        if(j.contains("extra"))
            j.at("extra").get_to(extra);
    }
};

enum Mode
{
    Undef,
    SingleExtruder,   // 选择了单挤出机打印机预设
    MultiAsSingle,    // 选择了多挤出机打印机预设，但
                      // 此模式仅适用于单挤出机打印
                      //（所有 ModelObject 和 ModelVolume 分配相同的挤出机）。
    MultiExtruder     // 选择了多挤出机打印机预设
};

// custom_code_per_height 模式的字符串模拟
static constexpr char SingleExtruderMode[] = "SingleExtruder";
static constexpr char MultiAsSingleMode [] = "MultiAsSingle";
static constexpr char MultiExtruderMode [] = "MultiExtruder";

struct Info
{
    Mode mode = Undef;
    std::vector<Item> gcodes;

    bool operator==(const Info& rhs) const
    {
        return  (rhs.mode   == this->mode   ) &&
                (rhs.gcodes == this->gcodes );
    }
    bool operator!=(const Info& rhs) const { return !(*this == rhs); }

    void from_json(const nlohmann::json& j) {
        std::string mode_str;
        if (j.contains("mode"))
            j.at("mode").get_to(mode_str);
        if (mode_str == "SingleExtruder") mode = SingleExtruder;
        else if (mode_str == "MultiAsSingle") mode = MultiAsSingle;
        else if (mode_str == "MultiExtruder") mode = MultiExtruder;
        else mode = Undef;

        auto j_gcodes = j.at("gcodes");
        gcodes.reserve(j_gcodes.size());
        for (auto& jj : j_gcodes) {
            Item item;
            item.from_json(jj);
            gcodes.push_back(item);
        }
    }
};

// 如果加载的配置具有 "colorprint_heights" 选项（如果是从旧版 Slicer 导入的），
// 并且 CustomGCode::Info.gcodes 为空（没有新格式的颜色打印数据），
// 那么 CustomGCode::Info.gcodes 应根据此选项进行更新。
//BBS
//extern void update_custom_gcode_per_print_z_from_config(Info& info, DynamicPrintConfig* config);

// 如果每个打印Z的自定义Gcode信息是从旧版Slicer导入的，模式将是未定义的。
// 因此，我们应该根据项目的代码值更新 CustomGCode::Info.mode。
extern void check_mode_for_custom_gcode_per_print_z(Info& info);

// 返回按 print_z 递增排序的 <print_z, 基于1的耗材ID> 对
// 来自 custom_gcode_per_print_z。耗材数量可能包括混合虚拟
// 耗材以及物理耗材。
std::vector<std::pair<double, unsigned int>> custom_tool_changes(const Info& custom_gcode_per_print_z, size_t num_filaments);

} // namespace CustomGCode

} // namespace Slic3r



#endif /* slic3r_CustomGCode_hpp_ */
