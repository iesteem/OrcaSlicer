#ifndef slic3r_FilamentHotBedNozzleRules_hpp_
#define slic3r_FilamentHotBedNozzleRules_hpp_

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "PrintConfig.hpp"

namespace Slic3r {

class PresetBundle;

/// 当 filament_hot_bed_nozzles.json 将当前喷嘴+类型标记为耗材预设禁止时填充。
struct NozzleFilamentRuleMismatch {
    bool        has_mismatch{ false };
    std::string nozzle_diameter_mm;   // 显示数字，例如 "0.2"（无 "mm" 后缀）
    std::string nozzle_type_key;      // NozzleTypeEumnToStr: undefine, hardened_steel, stainless_steel, brass
    std::string filament_preset_name; // 解析后的预设显示名称（可能为空）
};

// JSON: %AppData%/.../system/Snapmaker/filament/filament_hot_bed_nozzles.json（优先），否则使用捆绑资源路径。
// 键：热床 ID（btPEI, btGESP），喷嘴 ID（"0.2mm" ...）带有支持/警告。 …) with support/warning.
// 喷嘴规则（键名以 "mm" 结尾），任选其一：
// 1) 单对象 { "type":"all"|材质, "forbidden":[...] }
// 2) 数组 [ { "type", "forbidden" }, ... ]
// 3) 按材质分键对象：键为 all / undefine / hardened_steel / stainless_steel / brass（可只写其中任意几种），
//    值为 forbidden 数组、{ "forbidden": [...] }、warning 数组、{ "warning": [...] }，或同时含 forbidden / warning 的对象；
//    "all" 表示任意喷嘴材质共用该条规则。warning 为提示级（不拦截切片），与热床 warning 语义类似。
class FilamentHotBedNozzleRules
{
public:
    static constexpr const char* kBedKey_PEI  = "btPEI";
    static constexpr const char* kBedKey_GESP = "btGESP";

    static FilamentHotBedNozzleRules& singleton();

    void load();
    // 如果尚未加载则加载 JSON 一次（Print::extruders 使用此方法；GUI::load 通过 load() 强制重新加载）。
    void ensure_loaded();
    bool is_loaded() const;

    // "支持"包括警告级别的材料（这些材料允许使用，但需要单独的警告界面）。
    bool is_bed_filament_tips(const std::string& bed_key, const std::string& filament_type) const;
    bool is_bed_filament_supported(const std::string& bed_key, const std::string& filament_type) const;
    bool is_bed_filament_warning(const std::string& bed_key, const std::string& filament_type) const;
    bool is_nozzle_filament_forbidden(const std::string& nozzle_key, const std::string& filament_preset_name,
                                      NozzleType nozzle_type = NozzleType::ntUndefine) const;
    /// 通过基础预设名称上的精确名称（不区分大小写）匹配规则条目到耗材预设
    ///（可选的 " @..." 后缀之前的文本）；不会阻止切片（参见 evaluate_nozzle_filament_mismatch）。
    bool is_nozzle_filament_warning(const std::string& nozzle_key, const std::string& filament_preset_name,
                                    NozzleType nozzle_type = NozzleType::ntUndefine) const;
    
    std::string evaluate_nozzle_filament_mismatch(const PrintConfig& cfg,
                                                  const std::vector<unsigned int>& used_filament_indices,
                                                  const PresetBundle* preset_bundle = nullptr) const;

    /// @return 如果发现了禁止的喷嘴+耗材组合，则返回 true（填充 @p out）。
    bool evaluate_nozzle_filament_mismatch_detail(const PrintConfig& cfg, const std::vector<unsigned int>& used_filament_indices,
                                                    const PresetBundle* preset_bundle, NozzleFilamentRuleMismatch& out) const;

    
    bool evaluate_graphic_effect_bed_filament_mismatch(const PrintConfig& cfg,
                                                       const std::vector<unsigned int>& used_filament_indices) const;

    bool evaluate_pei_bed_filament_mismatch_not_pla(const PrintConfig& cfg, const std::vector<unsigned int>& used_filament_indices) const;
    bool evaluate_pei_bed_filament_mismatch_tpu(const PrintConfig& cfg, const std::vector<unsigned int>& used_filament_indices) const;

    // JSON 喷嘴 forbidden 条带（供解析逻辑使用）
    struct NozzleForbiddenBand {
        bool                            applies_to_all_nozzle_types{ true };
        std::unordered_set<std::string> nozzle_types;
        std::unordered_set<std::string> forbidden_substrings;
        std::unordered_set<std::string> warning_substrings;
    };

private:
    FilamentHotBedNozzleRules() = default;

    mutable std::recursive_mutex m_mutex;
    bool m_loaded{ false };
    std::unordered_map<std::string, std::unordered_set<std::string>> m_bed_support_filament_types;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_bed_warning_filament_types;
    std::unordered_map<std::string, std::vector<NozzleForbiddenBand>> m_nozzle_forbidden_bands;
};

std::string bed_type_to_filament_rule_key(BedType bed_type);
std::string nozzle_diameter_to_filament_rule_key(double nozzle_diameter_mm);

} // namespace Slic3r

#endif
