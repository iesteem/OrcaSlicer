#ifndef slic3r_PresetBundle_hpp_
#define slic3r_PresetBundle_hpp_

#include "Preset.hpp"
#include "AppConfig.hpp"
#include "FilamentColorLibrary.hpp"
#include "enum_bitmask.hpp"
#include "MixedFilament.hpp"

#include <memory>
#include <unordered_map>
#include <array>
#include <vector>
#include <boost/filesystem/path.hpp>

#define DEFAULT_USER_FOLDER_NAME "default"
#define BUNDLE_STRUCTURE_JSON_NAME "bundle_structure.json"

#define VALIDATE_PRESETS_SUCCESS                0
#define VALIDATE_PRESETS_PRINTER_NOT_FOUND      1
#define VALIDATE_PRESETS_FILAMENTS_NOT_FOUND    2
#define VALIDATE_PRESETS_MODIFIED_GCODES        3


// 定义供应商类型的枚举类
enum class VendorType {
    Unknown = 0,
    Klipper,
    Marlin,
    Marlin_BBL
};

struct ConnectMachineInfo
{
    std::string filament_info {""};
    std::string filament_type {""};
    std::string nozzle_info {""};
    std::string color_info{""};
    std::vector<std::string> multiColors;
    Slic3r::FilamentColorMode colorMode { Slic3r::FilamentColorMode::Segment };
    int index {0};
};

namespace Slic3r {

// Print + Filament + Printer 预设的捆绑包。
class PresetBundle
{
public:
    PresetBundle();
    PresetBundle(const PresetBundle &rhs);
    PresetBundle& operator=(const PresetBundle &rhs);

    // 移除所有预设，但保留"-- default --"。
    // 可选择性地从用户配置文件目录中移除预设引用的所有文件。
    void            reset(bool delete_files);

    void            setup_directories();
    void            copy_files(const std::string& from);

    struct PresetPreferences {
        std::string printer_model_id;// 首选打印机型号的名称
        std::string printer_variant; // 首选打印机变体的名称
        std::string filament;        // 首选耗材预设的名称
        std::string sla_material;    // 首选SLA材料预设的名称
    };

    // 从 Slic3r::data_dir() / presets 加载所有类型（print、filament、printer）的ini文件。
    // 从 config.ini 加载选择（当前打印、当前耗材、当前打印机）
    // 如果存在，选择首选的预设
    PresetsConfigSubstitutions load_presets(AppConfig &config, ForwardCompatibilitySubstitutionRule rule,
                                            const PresetPreferences& preferred_selection = PresetPreferences());

    // 从 config.ini 加载选择（当前打印、当前耗材、当前打印机）
    // 在应用程序启动时仅执行一次。
    //BBS: change it to public
    void     load_selections(AppConfig &config, const PresetPreferences& preferred_selection = PresetPreferences());

    // BBS 加载用户预设
    PresetsConfigSubstitutions load_user_presets(std::string user, ForwardCompatibilitySubstitutionRule rule);
    PresetsConfigSubstitutions load_user_presets(AppConfig &config, std::map<std::string, std::map<std::string, std::string>>& my_presets, ForwardCompatibilitySubstitutionRule rule);
    PresetsConfigSubstitutions import_presets(std::vector<std::string> &files, std::function<int(std::string const &)> override_confirm, ForwardCompatibilitySubstitutionRule rule);
    bool                       import_json_presets(PresetsConfigSubstitutions &            substitutions,
                                                   std::string &                           file,
                                                   std::function<int(std::string const &)> override_confirm,
                                                   ForwardCompatibilitySubstitutionRule    rule,
                                                   int &                                   overwrite,
                                                   std::vector<std::string> &              result);
    void save_user_presets(AppConfig& config, std::vector<std::string>& need_to_delete_list);
    void remove_users_preset(AppConfig &config, std::map<std::string, std::map<std::string, std::string>> * my_presets = nullptr);
    void update_user_presets_directory(const std::string preset_folder);
    void remove_user_presets_directory(const std::string preset_folder);
    void update_system_preset_setting_ids(std::map<std::string, std::map<std::string, std::string>>& system_presets);

    //BBS: add API to get previous machine
    int validate_presets(const std::string &file_name, DynamicPrintConfig& config, std::set<std::string>& different_gcodes);

    //BBS: add function to generate differed preset for save
    //the pointer should be freed by the caller
    Preset* get_preset_differed_for_save(Preset& preset);
    int get_differed_values_to_update(Preset& preset, std::map<std::string, std::string>& key_values);

    //BBS: get vendor's current version
    Semver get_vendor_profile_version(std::string vendor_name);

    // Orca: 获取供应商类型
    VendorType get_current_vendor_type();
    // 供应商相关的便捷函数
    bool is_bbl_vendor() { return get_current_vendor_type() == VendorType::Marlin_BBL; }
    // 是否使用bbl网络进行打印上传
    bool use_bbl_network();
    // 是否使用bbl的设备标签页
    bool use_bbl_device_tab();

    bool backup_user_folder() const;

    //BBS: project embedded preset logic
    PresetsConfigSubstitutions load_project_embedded_presets(std::vector<Preset*> project_presets, ForwardCompatibilitySubstitutionRule substitution_rule);
    std::vector<Preset*> get_current_project_embedded_presets();
    void reset_project_embedded_presets();

    //BBS: find printer model
    std::string get_texture_for_printer_model(std::string model_name);
    std::string get_stl_model_for_printer_model(std::string model_name);
    std::string get_hotend_model_for_printer_model(std::string model_name);

    // 将选择（当前打印、当前耗材、当前打印机）导出到 config.ini
    void            export_selections(AppConfig &config);

    // BBS
    void            set_num_filaments(unsigned int n, std::string new_col = "");
    void            set_num_filaments(unsigned int n, std::vector<std::string> new_colors);
    void            update_num_filaments(unsigned int to_del_filament_id);
    unsigned int sync_ams_list(unsigned int & unknowns);
    //BBS: check whether this is the only edited filament
    bool is_the_only_edited_filament(unsigned int filament_index);

    // Orca: 更新选中的耗材和打印
    void           update_selections(AppConfig &config);
    void set_calibrate_printer(std::string name);

    void set_is_validation_mode(bool mode) { validation_mode = mode; }
    void set_vendor_to_validate(std::string vendor) { vendor_to_validate = vendor; }

    std::set<std::string> get_printer_names_by_printer_type_and_nozzle(const std::string &printer_type, std::string nozzle_diameter_str);
    bool                  check_filament_temp_equation_by_printer_type_and_nozzle_for_mas_tray(const std::string &printer_type,
                                                                                               std::string &      nozzle_diameter_str,
                                                                                               std::string &      setting_id,
                                                                                               std::string &      tag_uid,
                                                                                               std::string &      nozzle_temp_min,
                                                                                               std::string &      nozzle_temp_max,
                                                                                               std::string &      preset_setting_id);

    Preset *                    get_similar_printer_preset(std::string printer_model, std::string printer_variant);
    
    PresetCollection            prints;
    PresetCollection            sla_prints;
    PresetCollection            filaments;
    PresetCollection            sla_materials;
	PresetCollection& 			materials(PrinterTechnology pt)       { return pt == ptFFF ? this->filaments : this->sla_materials; }
	const PresetCollection& 	materials(PrinterTechnology pt) const { return pt == ptFFF ? this->filaments : this->sla_materials; }
    PrinterPresetCollection     printers;
    PhysicalPrinterCollection   physical_printers;
    // 多挤出机或多材料打印的耗材预设名称。
    // extruders.size() 应与 printers.get_edited_preset().config.nozzle_diameter.size() 相同
    std::vector<std::string>    filament_presets;
    // BBS: AMS 相关
    std::map<int, DynamicPrintConfig> filament_ams_list;
    std::vector<std::vector<std::string>> ams_multi_color_filment;

    // 用于基于层的颜色混合的混合（虚拟）耗材。
    MixedFilamentManager        mixed_filaments;

    // Snapmaker 相关
    std::map<int, std::pair<std::string, std::string>> machine_filaments;
    std::vector<ConnectMachineInfo>                    m_connect_machine_info_list;

    // 校准相关
    Preset const * calibrate_printer = nullptr;
    std::set<Preset const *> calibrate_filaments;

    // 项目配置值与打印/耗材/打印机预设分开保存，
    // 它们被序列化/反序列化到/从.amf、.3mf、.config、.gcode，
    // 并被切片核心使用。
    DynamicPrintConfig          project_config;

    // 加载的系统配置文件每个都有一个条目，
    // 系统配置文件将指向PresetBundle::vendors拥有的VendorProfile实例。
    VendorMap                   vendors;

    // Orca: 用于Orca耗材库
    std::map<std::string, DynamicPrintConfig> m_config_maps;
    std::map<std::string, std::string> m_filament_id_maps;

        struct ObsoletePresets
    {
        std::vector<std::string> prints;
        std::vector<std::string> sla_prints;
        std::vector<std::string> filaments;
        std::vector<std::string> sla_materials;
        std::vector<std::string> printers;
    };
    ObsoletePresets             obsolete_presets;

    bool                        has_defauls_only() const
        { return prints.has_defaults_only() && filaments.has_defaults_only() && printers.has_defaults_only(); }

    DynamicPrintConfig          full_config() const;
    // full_config() 移除了一些"无用"的配置。
    DynamicPrintConfig          full_config_secure() const;

    // 加载用户配置并将其存储到用户配置文件中。
    // 此方法由配置向导调用。
    void                        load_config_from_wizard(const std::string &name, DynamicPrintConfig config, Semver file_version, bool is_custom_defined = false)
        { this->load_config_file_config(name, false, std::move(config), file_version, true, is_custom_defined); }

    // 加载来自包含配置的模型文件（如3MF等）的配置。
    // 此方法由Plater调用。
    void                        load_config_model(const std::string &name, DynamicPrintConfig config, Semver file_version = Semver())
        { this->load_config_file_config(name, true, std::move(config), file_version); }

    // 加载包含打印、耗材和打印机预设的外部配置文件。
    // 除了配置文件，也可以加载包含全套参数的G-code。
    // 将来配置也可能从AMF文件中读取。
    // 如果文件加载成功，其打印/耗材/打印机配置文件将被激活。
    ConfigSubstitutions         load_config_file(const std::string &path, ForwardCompatibilitySubstitutionRule compatibility_rule);

    // 加载配置包文件到预设中，并将加载的预设存储到本地配置目录的单独文件中。
    // 将设置加载到提供的设置实例中。
    // 激活存储在配置包中的预设。
    // 返回成功加载的预设数量。
    enum LoadConfigBundleAttribute {
        // 保存已加载的配置文件。
        SaveImported,
        // 在加载前删除所有旧的配置文件。
        ResetUserProfile,
        // 加载系统配置包。
        LoadSystem,
        LoadVendorOnly,
        LoadFilamentOnly,
    };
    using LoadConfigBundleAttributes = enum_bitmask<LoadConfigBundleAttribute>;
    // 根据标志加载配置包。
    // 加载系统配置文件时不执行任何配置替换，否则执行并报告替换。
    /*std::pair<PresetsConfigSubstitutions, size_t> load_configbundle(
        const std::string &path, LoadConfigBundleAttributes flags, ForwardCompatibilitySubstitutionRule compatibility_rule);*/
    //Orca: 从json加载配置包，传递基础包以支持跨供应商继承
    std::pair<PresetsConfigSubstitutions, size_t> load_vendor_configs_from_json(
        const std::string &path, const std::string &vendor_name, LoadConfigBundleAttributes flags, ForwardCompatibilitySubstitutionRule compatibility_rule, const PresetBundle* base_bundle = nullptr);

    // Export a config bundle file containing all the presets and the names of the active presets.
    //void                        export_configbundle(const std::string &path, bool export_system_settings = false, bool export_physical_printers = false);
    //BBS: add a function to export current configbundle as default
    //void export_current_configbundle(const std::string &path);
    //BBS: add a function to export system presets for cloud-slicer
    //void export_system_configs(const std::string &path);
    std::vector<std::string> export_current_configs(const std::string &path, std::function<int(std::string const &)> override_confirm,
        bool include_modify, bool export_system_settings = false);

    // 启用/禁用 "- default -" 预设。
    void                        set_default_suppressed(bool default_suppressed);

    // 设置耗材预设名称。由于名称可能来自UI选择框，
    // 可选的"(modified)"后缀将从耗材名称中移除。
    void                        set_filament_preset(size_t idx, const std::string &name);

    // 从活动打印机预设中读取挤出机数量，更新filament_presets的大小和内容。
    void                        update_multi_material_filament_presets(size_t to_delete_filament_id = size_t(-1),
                                                                       size_t old_num_filaments = size_t(-1));
    // 在混合行启用/删除更改后重建旧->新虚拟耗材映射，当物理耗材数量本身未更改时。
    void                        update_mixed_filament_id_remap(const std::vector<MixedFilament> &old_mixed,
                                                               size_t old_num_filaments,
                                                               size_t new_num_filaments,
                                                               size_t deleted_mixed_idx = size_t(-1));
    // 在最新的耗材数量更改期间生成的映射。
    // 索引是旧的基于1的耗材ID，值是新的基于1的耗材ID（0 = 已移除）。
    const std::vector<unsigned int>& last_filament_id_remap() const { return m_last_filament_id_remap; }
    
    // 为混合耗材合并操作构建自定义重映射
    // 在将混合耗材合并到另一个耗材（物理或混合）时使用
    void build_merge_filament_remap(size_t from_id, size_t to_id, size_t total_filaments)
    {
        m_last_filament_id_remap.assign(total_filaments + 1, 0);
        for (size_t i = 0; i <= total_filaments; ++i) {
            if (i == from_id + 1) {
                // 当 from_id < to_id 时，源移除后目标也向下移动1，
                // 因此其新的基于1的ID是 `to_id`（而不是 to_id+1）。
                if (from_id < to_id)
                    m_last_filament_id_remap[i] = (unsigned int)(to_id);
                else
                    m_last_filament_id_remap[i] = (unsigned int)(to_id + 1);
            } else if (i > from_id + 1) {
                m_last_filament_id_remap[i] = (unsigned int)(i - 1);  // 删除后ID向下移动
            } else {
                m_last_filament_id_remap[i] = (unsigned int)i;  // 保持不变
            }
        }
    }
    
    // 为物理到混合耗材合并操作构建自定义重映射
    // 这考虑了物理耗材被删除时的虚拟ID更改
    // 并且考虑了依赖于被删除物理耗材的混合耗材
    // num_physical: 删除前的物理耗材数量
    void build_merge_filament_remap(size_t from_id, size_t to_id, size_t total_filaments, size_t num_physical)
    {
        m_last_filament_id_remap.assign(total_filaments + 1, 0);
        
        // 首先，识别哪些混合耗材将被删除（那些依赖于from_id的）
        std::set<size_t> deleted_mixed_indices;
        unsigned int from_1based = (unsigned int)(from_id + 1);

        std::vector<size_t> dependent = mixed_filaments.mixed_filaments_using_physical(from_1based);
        size_t visible = 0;
        const auto& mfs_ref = mixed_filaments.mixed_filaments();
        for (size_t k = 0; k < mfs_ref.size(); ++k) {
            if (!mfs_ref[k].enabled || mfs_ref[k].deleted) continue;
            if (std::find(dependent.begin(), dependent.end(), k) != dependent.end())
                deleted_mixed_indices.insert(num_physical + visible);
            ++visible;
        }
        
        for (size_t i = 0; i <= total_filaments; ++i) {
            if (i == from_id + 1) {
                // 源物理耗材映射到目标混合耗材
                // 删除后目标的虚拟ID: new_num_physical + adjusted_mixed_idx
                size_t target_old_virtual_id = to_id;  // 基于0
                size_t target_old_mixed_idx = target_old_virtual_id - num_physical;
                size_t new_num_physical = num_physical - 1;
                // 减去出现在目标之前的已删除混合耗材
                size_t deleted_before_target = 0;
                for (size_t vid : deleted_mixed_indices)
                    if (vid < target_old_virtual_id) ++deleted_before_target;
                size_t target_new_virtual_id = new_num_physical + target_old_mixed_idx - deleted_before_target;
                m_last_filament_id_remap[i] = (unsigned int)(target_new_virtual_id + 1);  // 转换为基于1
            } else if (i > from_id + 1 && i <= num_physical) {
                // 后续的物理耗材向下移动1
                m_last_filament_id_remap[i] = (unsigned int)(i - 1);
            } else if (i > num_physical) {
                // Mixed filament indices will change (because num_physical decreases)
                size_t old_virtual_id = i - 1;
                size_t old_mixed_idx = old_virtual_id - num_physical;
                
                // 检查此混合耗材是否将被删除
                if (deleted_mixed_indices.find(old_virtual_id) != deleted_mixed_indices.end()) {
                    // 此混合耗材将被删除，映射到0（无效）
                    m_last_filament_id_remap[i] = 0;
                } else {
                    // 此混合耗材将被保留，计算其新虚拟ID。
                    // 减去出现在此之前的已删除混合耗材。
                    size_t new_num_physical = num_physical - 1;
                    size_t deleted_before = 0;
                    for (size_t vid : deleted_mixed_indices)
                        if (vid < old_virtual_id) ++deleted_before;
                    size_t new_virtual_id = new_num_physical + old_mixed_idx - deleted_before;
                    m_last_filament_id_remap[i] = (unsigned int)(new_virtual_id + 1);  // 转换为基于1
                }
            } else {
                // 源之前的物理耗材保持不变
                m_last_filament_id_remap[i] = (unsigned int)i;
            }
        }
    }
    
    // 直接设置自定义重映射表
    void set_filament_id_remap(const std::vector<unsigned int>& remap)
    {
        m_last_filament_id_remap = remap;
    }
    
    std::vector<unsigned int> consume_last_filament_id_remap()
    {
        std::vector<unsigned int> out = std::move(m_last_filament_id_remap);
        m_last_filament_id_remap.clear();
        return out;
    }

    // 根据打印和耗材预设是否标记为与当前选中的打印机（以及耗材预设情况下的打印）兼容来更新其is_compatible标志。
    // 还更新每个预设的is_visible标志。
    // 如果 select_other_if_incompatible 为true，则当前打印或耗材预设不兼容时切换到某个兼容的预设。
    void                        update_compatible(PresetSelectCompatibleType select_other_print_if_incompatible, PresetSelectCompatibleType select_other_filament_if_incompatible);
    void                        update_compatible(PresetSelectCompatibleType select_other_if_incompatible) { this->update_compatible(select_other_if_incompatible, select_other_if_incompatible); }

    // 基于用户配置设置打印机供应商、打印机型号和打印机变体的is_visible标志。
    // 如果缺少"vendor"部分，则启用特定供应商的所有型号和变体。
    // 当型号已启用时，还将新发布的喷嘴变体合并到应用配置中（无需向导）。
    void                        load_installed_printers(AppConfig &config);

    // 如果用户已启用打印机型号（应用配置中的任何喷嘴变体），则启用供应商配置文件中当前定义的所有变体。
    // 从 load_installed_printers 调用；为罕见的直接使用而暴露。
    void                        install_missing_variants_for_enabled_models(AppConfig &config);

    const std::string&          get_preset_name_by_alias(const Preset::Type& preset_type, const std::string& alias) const;

    const int                   get_required_hrc_by_filament_type(const std::string& filament_type) const;
    // 将提供的类型的当前预设以新名称保存。如果名称与旧名称不同，
    // 未选中的选项将恢复为初始值
    //BBS: add project embedded preset logic
    void                        save_changes_for_preset(const std::string& new_name, Preset::Type type, const std::vector<std::string>& unselected_options, bool save_to_project = false);

    std::pair<PresetsConfigSubstitutions, std::string> load_system_models_from_json(ForwardCompatibilitySubstitutionRule compatibility_rule);
    std::pair<PresetsConfigSubstitutions, std::string> load_system_filaments_json(ForwardCompatibilitySubstitutionRule compatibility_rule);
    VendorProfile                                      get_custom_vendor_models() const;

    // SM_FEATURE: add Snapmaker machine as default
    static const char *SM_BUNDLE;
    static const char* SM_DEFAULT_PRINTER_MODEL;
    static const char* SM_DEFAULT_PRINTER_VARIANT;
    static const char* SM_DEFAULT_FILAMENT;
    static const char *ORCA_FILAMENT_LIBRARY;


    static std::array<Preset::Type, 3>  types_list(PrinterTechnology pt) {
        if (pt == ptFFF)
            return  { Preset::TYPE_PRINTER, Preset::TYPE_PRINT, Preset::TYPE_FILAMENT };
        return      { Preset::TYPE_PRINTER, Preset::TYPE_SLA_PRINT, Preset::TYPE_SLA_MATERIAL };
    }

    // Orca: 仅用于验证
    bool has_errors() const;

private:
    //std::pair<PresetsConfigSubstitutions, std::string> load_system_presets(ForwardCompatibilitySubstitutionRule compatibility_rule);
    //BBS: add json related logic
    std::pair<PresetsConfigSubstitutions, std::string> load_system_presets_from_json(ForwardCompatibilitySubstitutionRule compatibility_rule);
    // 将一个供应商的预设与另一个供应商的预设合并，报告重复项。
    std::vector<std::string>    merge_presets(PresetBundle &&other);
    void                        build_filament_id_remap(const std::vector<MixedFilament> &old_mixed,
                                                        size_t old_num_filaments,
                                                        size_t new_num_filaments,
                                                        bool deleting_filament,
                                                        unsigned int deleted_1based,
                                                        size_t deleted_mixed_idx = size_t(-1));
    // 更新系统配置文件的renamed_from和别名映射。
    void 						update_system_maps();

    // 设置耗材和sla材料的is_visible标志，
    // 当没有安装耗材/材料时，基于已启用的打印机应用默认值。
    void                        load_installed_filaments(AppConfig &config);
    void                        load_installed_sla_materials(AppConfig &config);

    // 从配置中加载打印、耗材和打印机预设。如果是外部配置，则从外部路径提取名称。
    // 并且外部配置仅被引用，不存储到用户配置文件目录。
    // 如果不是外部配置，则配置将存储到用户配置文件目录。
    void                        load_config_file_config(const std::string &name_or_path, bool is_external, DynamicPrintConfig &&config, Semver file_version = Semver(), bool selected = false, bool is_custom_defined = false);
    /*ConfigSubstitutions         load_config_file_config_bundle(
        const std::string &path, const boost::property_tree::ptree &tree, ForwardCompatibilitySubstitutionRule compatibility_rule);*/

    DynamicPrintConfig          full_fff_config() const;
    DynamicPrintConfig          full_sla_config() const;

    // Orca: 仅用于验证
    bool validation_mode = false;
    std::string vendor_to_validate = "";
    int m_errors = 0;
    std::vector<unsigned int> m_last_filament_id_remap;

};

ENABLE_ENUM_BITMASK_OPERATORS(PresetBundle::LoadConfigBundleAttribute)

} // namespace Slic3r

#endif /* slic3r_PresetBundle_hpp_ */
