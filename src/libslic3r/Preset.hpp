#ifndef slic3r_Preset_hpp_
#define slic3r_Preset_hpp_

#include <deque>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include "PrintConfig.hpp"
#include "Semver.hpp"
#include "ProjectTask.hpp"

//BBS: change system directories
#define PRESET_WEB_DIR         "web"
#define PRESET_SYSTEM_DIR      "system"
#define PRESET_USER_DIR        "user"
#define PRESET_FILAMENT_NAME    "filament"
#define PRESET_PRINT_NAME     "process"
#define PRESET_PRINTER_NAME     "machine"
#define PRESET_SLA_PRINT_NAME  "sla_print"
#define PRESET_SLA_MATERIALS_NAME "sla_materials"
#define PRESET_PROFILES_DIR "profiles"
#define PRESET_PROFILES_TEMOLATE_DIR "profiles_template"
#define PRESET_TEMPLATE_DIR "Template"
#define PRESET_CUSTOM_VENDOR "Custom"

//BBS: iot preset type strings
#define PRESET_IOT_PRINTER_TYPE     "printer"
#define PRESET_IOT_FILAMENT_TYPE    "filament"
#define PRESET_IOT_PRINT_TYPE       "print"


//BBS: add json support
#define BBL_JSON_KEY_MIN_VERSION    "min_version"
#define BBL_JSON_KEY_VERSION        "version"
#define BBL_JSON_KEY_IS_CUSTOM      "is_custom_defined"
#define BBL_JSON_KEY_URL            "url"
#define BBL_JSON_KEY_NAME           "name"
#define BBL_JSON_KEY_DESCRIPTION    "description"
#define BBL_JSON_KEY_FORCE_UPDATE   "force_update"
#define BBL_JSON_KEY_MACHINE_MODEL_LIST     "machine_model_list"
#define BBL_JSON_KEY_PROCESS_LIST   "process_list"
#define BBL_JSON_KEY_SUB_PATH       "sub_path"
#define BBL_JSON_KEY_FILAMENT_LIST  "filament_list"
#define BBL_JSON_KEY_MACHINE_LIST   "machine_list"
#define BBL_JSON_KEY_TYPE           "type"
#define BBL_JSON_KEY_FROM           "from"
#define BBL_JSON_KEY_SETTING_ID     "setting_id"
#define BBL_JSON_KEY_BASE_ID        "base_id"
#define BBL_JSON_KEY_USER_ID        "user_id"
#define BBL_JSON_KEY_FILAMENT_ID    "filament_id"
#define BBL_JSON_KEY_UPDATE_TIME    "updated_time"
#define BBL_JSON_KEY_INHERITS       "inherits"
#define BBL_JSON_KEY_INSTANTIATION  "instantiation"
#define BBL_JSON_KEY_NOZZLE_DIAMETER            "nozzle_diameter"
#define BBL_JSON_KEY_PRINTER_TECH                 "machine_tech"
#define BBL_JSON_KEY_FAMILY                     "family"
#define BBL_JSON_KEY_BED_MODEL                  "bed_model"
#define BBL_JSON_KEY_BED_TEXTURE                "bed_texture"
#define BBL_JSON_KEY_HOTEND_MODEL               "hotend_model"
#define BBL_JSON_KEY_DEFAULT_MATERIALS          "default_materials"
#define BBL_JSON_KEY_MODEL_ID                   "model_id"

// Orca 扩展
#define ORCA_JSON_KEY_RENAMED_FROM              "renamed_from"


namespace Slic3r {

class AppConfig;
class PresetBundle;

enum ConfigFileType
{
    CONFIG_FILE_TYPE_UNKNOWN,
    CONFIG_FILE_TYPE_APP_CONFIG,
    CONFIG_FILE_TYPE_CONFIG,
    CONFIG_FILE_TYPE_CONFIG_BUNDLE,
};

//BBS: add a function to load the version from xxx.json
extern Semver get_version_from_json(std::string file_path);

extern Semver get_min_version_from_json(std::string file_path);

//BBS: add a function to load the key-values from xxx.json
extern int get_values_from_json(std::string file_path, std::vector<std::string>& keys, std::map<std::string, std::string>& key_values);

extern ConfigFileType guess_config_file_type(const boost::property_tree::ptree &tree);

class VendorProfile
{
public:
    std::string                     name;
    std::string                     id;
    Semver                          config_version;
    std::string                     config_update_url;
    std::string                     changelog_url;

    struct PrinterVariant {
        PrinterVariant() {}
        PrinterVariant(const std::string &name) : name(name) {}
        std::string                 name;
    };

    struct PrinterModel {
        PrinterModel() {}
        std::string                 id;
        std::string                 name;
        //BBS: this is internal id for the printer. Currently only used for searching in database
        std::string                 model_id;
        PrinterTechnology           technology;
        std::string                 family;
        std::vector<PrinterVariant> variants;
        std::vector<std::string>	default_materials;
        // 供应商和打印机型号特定的打印热床模型和纹理。
        std::string 			 	bed_model;
        std::string 				bed_texture;
        std::string                 hotend_model;

        PrinterVariant*       variant(const std::string &name) {
            for (auto &v : this->variants)
                if (v.name == name)
                    return &v;
            return nullptr;
        }

        const PrinterVariant* variant(const std::string &name) const { return const_cast<PrinterModel*>(this)->variant(name); }
    };
    std::vector<PrinterModel>          models;

    std::set<std::string>              default_filaments;
    std::set<std::string>              default_sla_materials;

    VendorProfile() {}
    VendorProfile(std::string id) : id(std::move(id)) {}

    bool 		valid() const { return ! name.empty() && ! id.empty() && config_version.valid(); }

    // 从ini文件加载VendorProfile。
    // 如果`load_all`为false，仅加载包含基本信息（名称、版本、URL）的头部。
    static VendorProfile from_ini(const boost::filesystem::path &path, bool load_all=true);
    static VendorProfile from_ini(const boost::property_tree::ptree &tree, const boost::filesystem::path &path, bool load_all=true);

    size_t      num_variants() const { size_t n = 0; for (auto &model : models) n += model.variants.size(); return n; }
    std::vector<std::string> families() const;

    bool        operator< (const VendorProfile &rhs) const { return this->id <  rhs.id; }
    bool        operator==(const VendorProfile &rhs) const { return this->id == rhs.id; }
};

class Preset;

// 用于保存配置文件及其供应商定义的辅助结构，供应商定义可能已从父系统预设中提取。
// 父预设只能通过PresetCollection访问，因此为了允许在PresetCollection外部定义各种is_compatible_with方法，
// 在需要时由PresetCollection::get_preset_with_vendor_profile()返回此复合结构。
struct PresetWithVendorProfile {
	PresetWithVendorProfile(const Preset &preset, const VendorProfile *vendor) : preset(preset), vendor(vendor) {}
	const Preset 		&preset;
	const VendorProfile *vendor;
};

// 注意：此处使用map而不是unordered_map很重要，
// 因为我们需要迭代器不被失效，
// 因为Preset和ConfigWizard持有指向VendorProfiles的指针。
// XXX: 也许set就足够了（参见Wizard中的更改）
typedef std::map<std::string, VendorProfile> VendorMap;

class Preset
{
public:
    enum Type
    {
        TYPE_INVALID,
        TYPE_PRINT,
        TYPE_SLA_PRINT,
        TYPE_FILAMENT,
        TYPE_SLA_MATERIAL,
        TYPE_PRINTER,
        TYPE_COUNT,
        // 此类型用于支持物理打印机的PresetConfigSubstitutions，但它不属于Preset类，
        // 而是使用PhysicalPrinter类。
        TYPE_PHYSICAL_PRINTER,
        // BBS: plate config
        TYPE_PLATE,
        // BBS: model config
        TYPE_MODEL,
    };

    Type                type        = TYPE_INVALID;

    // 预设表示一组"默认"属性，
    // 从PrintConfig的默认值中提取（其定义请参见PrintConfigDef）。
    bool                is_default = false;
    // 外部预设指向已加载但未导入到Slic3r默认配置位置的配置。
    bool                is_external = false;
    // 系统预设是只读的。
    bool                is_system   = false;
    // 如果预设与AppConfig中启用的打印机型号/变体关联，或者没有打印机型号/变体关联，则预设可见。
    // 此外，仅当"default"预设是列表中唯一的预设时，它才可见。
    bool                is_visible  = true;
    // 此预设是否已被修改？
    bool                is_dirty    = false;
    // 此预设是否与当前活动打印机兼容？
    bool                is_compatible = true;

    //BBS: add type for project-embedded
    bool                is_project_embedded = false;
    ConfigSubstitutions *loading_substitutions{nullptr};
    bool                is_user() const { return ! this->is_default && ! this->is_system && ! this->is_project_embedded; }
    //bool                is_user() const { return ! this->is_default && ! this->is_system; }

    // 预设名称，通常从文件名派生。
    std::string         name;
    // 预设的文件名。可以是打印/耗材/打印机预设，
    // 或捆绑了打印+耗材+打印机预设的配置文件（此时is_external和可能的is_system将为true），
    // 也可以是G-code（同样，is_external将为true）。
    std::string         file;
    // 如果这是系统配置文件，则应有供应商数据可在UI中显示。
    const VendorProfile *vendor      = nullptr;

    // 此配置文件是否已加载？
    bool                loaded      = false;

    // 配置数据，从文件加载或从默认值设置。
    DynamicPrintConfig  config;

    // 预设的别名
    std::string         alias;
    // 配置文件名称列表，此配置文件在某个时间点从此列表中重命名。
    // 此列表用于在从.gcode、.3mf、.amf加载时按名称匹配配置文件，
    // 以及将用户配置文件的"inherits"字段与更新后的系统配置文件匹配。
    std::vector<std::string> renamed_from;

    // Orca: 维护从此预设中排除的打印机型号列表，设计用于在Orca耗材库中未定义compatible_printer的耗材
    // （因此默认对所有打印机型号可见）。但是，我们可能在供应商配置文件中为特定打印机型号定义了专门的耗材，
    // 在这种情况下，我们希望对这些打印机型号隐藏此通用预设。
    std::set<std::string> m_excluded_from;

    // Orca: 标志，指示此预设是否来自Orca耗材库
    bool m_from_orca_filament_lib = false;

    //BBS
    Semver              version;         // 预设版本
    std::string         ini_str;         // 预设的ini字符串
    std::string         setting_id;      // 云数据库中的设置ID
    std::string         filament_id;      // 云数据库中的设置ID
    std::string         user_id;         // 预设用户ID
    std::string         base_id;         // 预设的基础ID
    std::string         sync_info;       // 枚举: "delete", "create", "update", ""
    std::string         custom_defined;  // 枚举: "1", "0", ""
    std::string         description;     // 描述
    long long           updated_time{0};    //last updated time
    std::map<std::string, std::string> key_values;

    static std::string  get_type_string(Preset::Type type);
    // 获取物联网的字符串类型
    static std::string  get_iot_type_string(Preset::Type type);
    static Preset::Type get_type_from_string(std::string type_str);
    void                load_info(const std::string& file);
    void                save_info(std::string file = "");
    void                remove_files();

    //BBS: add logic for only difference save
    //if parent_config is null, save all keys, otherwise, only save difference
    void                save(DynamicPrintConfig* parent_config);
    void                reload(Preset const & parent);

    // 返回此预设的标签，由名称和"(modified)"后缀组成（如果此预设被修改过）。
    std::string         label(bool no_alias) const;

    // 如果提供的配置与活动配置不同，则设置is_dirty标志。
    void                set_dirty(const DynamicPrintConfig &config) { this->is_dirty = ! this->config.diff(config).empty(); }
    void                set_dirty(bool dirty = true) { this->is_dirty = dirty; }
    void                reset_dirty() { this->is_dirty = false; }

    // 返回此预设所继承的预设名称。
    static std::string& inherits(DynamicPrintConfig &cfg) { return cfg.option<ConfigOptionString>("inherits", true)->value; }
    std::string&        inherits() { return Preset::inherits(this->config); }
    const std::string&  inherits() const { return Preset::inherits(const_cast<Preset*>(this)->config); }

    // 返回 "compatible_prints_condition"。
    static std::string& compatible_prints_condition(DynamicPrintConfig &cfg) { return cfg.option<ConfigOptionString>("compatible_prints_condition", true)->value; }
    std::string&        compatible_prints_condition() {
		assert(this->type == TYPE_FILAMENT || this->type == TYPE_SLA_MATERIAL);
        return Preset::compatible_prints_condition(this->config);
    }
    const std::string&  compatible_prints_condition() const { return const_cast<Preset*>(this)->compatible_prints_condition(); }

    // 返回 "compatible_printers_condition"。
    static std::string& compatible_printers_condition(DynamicPrintConfig &cfg) { return cfg.option<ConfigOptionString>("compatible_printers_condition", true)->value; }
    std::string&        compatible_printers_condition() {
		assert(this->type == TYPE_PRINT || this->type == TYPE_SLA_PRINT || this->type == TYPE_FILAMENT || this->type == TYPE_SLA_MATERIAL);
        return Preset::compatible_printers_condition(this->config);
    }
    const std::string&  compatible_printers_condition() const { return const_cast<Preset*>(this)->compatible_printers_condition(); }

    // 返回打印机技术，如果未设置打印机技术则返回ptFFF。
    static PrinterTechnology printer_technology(const DynamicPrintConfig &cfg) {
        auto *opt = cfg.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
        // 导入旧配置文件时可能触发以下断言，
        // 但将其保留在此处以捕获不应查询"printer_technology"键的情况更安全。
//        assert(opt != nullptr);
        return (opt == nullptr) ? ptFFF : opt->value;
    }
    PrinterTechnology   printer_technology() const { return Preset::printer_technology(this->config); }
    // 此调用返回引用，它可能向DynamicPrintConfig添加新条目。
    PrinterTechnology&  printer_technology_ref() { return this->config.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology", true)->value; }

    // 根据应用配置设置is_visible
    void                set_visible_from_appconfig(const AppConfig &app_config);

    // 调整挤出机特定字段的大小，使用第一个挤出机的内容初始化它们。
    void                set_num_extruders(unsigned int n) { this->config.set_num_extruders(n); }

    // 按预设名称进行字典序排序。预设名称在单个PresetCollection中应是唯一的。
    bool                operator<(const Preset &other) const { return this->name < other.name; }

    // 专门用于支撑G和支撑W
    std::string get_filament_type(std::string &display_filament_type);
    std::string get_printer_type(PresetBundle *preset_bundle); // 获取编辑后的预设类型
    std::string get_current_printer_type(PresetBundle *preset_bundle); // 获取当前预设类型

    bool has_lidar(PresetBundle *preset_bundle);
    bool is_custom_defined();

    BedType get_default_bed_type(PresetBundle *preset_bundle);
    bool has_cali_lines(PresetBundle* preset_bundle);


    static double convert_pellet_flow_to_filament_diameter(double pellet_flow_coefficient)
    {
        return sqrt(4 / (PI * pellet_flow_coefficient)); 
    }

    static double convert_filament_diameter_to_pellet_flow(double filament_diameter)
    {
        return 4 / (pow(filament_diameter, 2) * PI); 
    }

    static const std::vector<std::string>&  print_options();
    static const std::vector<std::string>&  filament_options();
    // 打印机选项包含喷嘴选项。
    static const std::vector<std::string>&  printer_options();
    // 打印机选项中的喷嘴选项。
    static const std::vector<std::string>&  nozzle_options();
    // 打印机机器限制，包含在printer_options()中。
    static const std::vector<std::string>&  machine_limits_options();

    static const std::vector<std::string>&  sla_printer_options();
    static const std::vector<std::string>&  sla_material_options();
    static const std::vector<std::string>&  sla_print_options();

	static void                             update_suffix_modified(const std::string& new_suffix_modified);
    static const std::string&               suffix_modified();
    static std::string                      remove_suffix_modified(const std::string& name);
    static void                             normalize(DynamicPrintConfig &config);
    // 报告被错误放置到错误组中的配置字段，将它们从配置中移除。
    static std::string                      remove_invalid_keys(DynamicPrintConfig &config, const DynamicPrintConfig &default_config);

    // BBS: 将构造函数移到public
    Preset(Type type, const std::string &name, bool is_default = false) : type(type), is_default(is_default), name(name) {}

protected:
    Preset() = default;

    friend class        PresetCollection;
    friend class        PresetBundle;
};

bool is_compatible_with_print  (const PresetWithVendorProfile &preset, const PresetWithVendorProfile &active_print, const PresetWithVendorProfile &active_printer);
bool is_compatible_with_printer(const PresetWithVendorProfile &preset, const PresetWithVendorProfile &active_printer, const DynamicPrintConfig *extra_config);
bool is_compatible_with_printer(const PresetWithVendorProfile &preset, const PresetWithVendorProfile &active_printer);

enum class PresetSelectCompatibleType {
	// 如果新选择的配置文件不兼容，则从不选择兼容的预设。
	Never,
	// 仅当活动配置文件以前是兼容的但不再兼容时，才选择兼容的预设。
	OnlyIfWasCompatible,
	// 如果活动配置文件不再兼容，则始终选择兼容的预设。
	Always
};

// 在解析单个配置文件期间执行的替换。
struct PresetConfigSubstitutions {
    // 用户可读的预设名称。
    std::string                             preset_name;
    // 预设类型（Print / Filament / Printer ...）
    Preset::Type                            preset_type;
    enum class Source {
        UserFile,
        ConfigBundle,
        //BBS: add cloud and project type
        UserCloud,
        ProjectFile,
    };
    Source                                  preset_source;
    // Source of the preset. It may be empty in case of a ConfigBundle being loaded.
    std::string                             preset_file;
    // What config value has been substituted with what.
    ConfigSubstitutions                     substitutions;
};

// Substitutions having been performed during parsing a set of configuration files, for example when starting up
// PrusaSlicer and reading the user Print / Filament / Printer profiles.
using PresetsConfigSubstitutions = std::vector<PresetConfigSubstitutions>;

// 相同类型预设的集合（Print、Filament或Printer类型之一）。
class PresetCollection
{
public:
    // 用"- default -"预设初始化PresetCollection。
    PresetCollection(Preset::Type type, const std::vector<std::string> &keys, const Slic3r::StaticPrintConfig &defaults, const std::string &default_name = "Default Setting");

    typedef std::deque<Preset>::iterator Iterator;
    typedef std::deque<Preset>::const_iterator ConstIterator;
    typedef std::function<void(Preset* preset, std::string sync_info)> SyncFunc;
    //BBS get m_presets begin
    Iterator        lbegin() { return m_presets.begin(); }
    //BBS: validate_preset
    bool            validate_preset(const std::string &name, std::string &inherit);

    Iterator        begin() { return m_presets.begin() + m_num_default_presets; }
    ConstIterator   begin() const { return m_presets.cbegin() + m_num_default_presets; }
    ConstIterator   cbegin() const { return m_presets.cbegin() + m_num_default_presets; }
    Iterator        end() { return m_presets.end(); }
    ConstIterator   end() const { return m_presets.cend(); }
    ConstIterator   cend() const { return m_presets.cend(); }

    //BBS
    Iterator        erase(Iterator it) { return m_presets.erase(it); }
    SyncFunc        sync_func{ nullptr };
    void            set_sync_func(SyncFunc func) { sync_func = func; }
    //BBS: mutex
    void            lock() { m_mutex.lock(); }
    void            unlock() { m_mutex.unlock(); }

    void            reset(bool delete_files);

    Preset::Type    type() const { return m_type; }
    // 在屏幕上和错误消息中使用的名称。未本地化。
    std::string     name() const;
    // 在配置包中用作节名称，以及预设的文件夹名称。
    std::string     section_name() const;
    const std::deque<Preset>& operator()() const { return m_presets; }

    // 在集合开头添加默认预设，递增m_default_preset计数器。
    void            add_default_preset(const std::vector<std::string> &keys, const Slic3r::StaticPrintConfig &defaults, const std::string &preset_name);

    // 从提供的目录路径加载特定类型的ini文件。
    void            load_presets(const std::string &dir_path, const std::string &subdir, PresetsConfigSubstitutions& substitutions, ForwardCompatibilitySubstitutionRule rule);

    //BBS: update user presets directory
    void            update_user_presets_directory(const std::string& dir_path, const std::string& type);
    void            save_user_presets(const std::string& dir_path, const std::string& type, std::vector<std::string>& need_to_delete_list);
    bool            load_user_preset(std::string name, std::map<std::string, std::string> preset_values, PresetsConfigSubstitutions& substitutions, ForwardCompatibilitySubstitutionRule rule);
    void            update_after_user_presets_loaded();
    //BBS: get user presets
    int  get_user_presets(PresetBundle *preset_bundle, std::vector<Preset> &result_presets);
    void set_sync_info_and_save(std::string name, std::string setting_id, std::string syncinfo, long long update_time);
    bool need_sync(std::string name, std::string setting_id, long long update_time);

    //BBS: add function to generate differed preset for save
    //the pointer should be freed by the caller
    Preset* get_preset_differed_for_save(Preset& preset);
    //BBS:get the differencen values to update
    int get_differed_values_to_update(Preset& preset, std::map<std::string, std::string>& key_values);

    //BBS: add project embedded presets logic
    void load_project_embedded_presets(std::vector<Preset*>& project_presets, const std::string& type, PresetsConfigSubstitutions& substitutions, ForwardCompatibilitySubstitutionRule rule);
    std::vector<Preset*> get_project_embedded_presets();
    bool reset_project_embedded_presets();

    // 从已解析的配置文件加载预设，将其插入到排序的预设序列中，
    // 并选择它，丢失之前的修改。
    Preset&         load_preset(const std::string &path, const std::string &name, const DynamicPrintConfig &config, bool select = true, Semver file_version = Semver(), bool is_custom_defined = false);
    Preset&         load_preset(const std::string &path, const std::string &name, DynamicPrintConfig &&config, bool select = true, Semver file_version = Semver(), bool is_custom_defined = false);

    bool clone_presets(std::vector<Preset const *> const &presets, std::vector<std::string> &failures, std::function<void(Preset &, Preset::Type &)> modifier, bool force_rewritten = false);
    bool clone_presets_for_printer(
        std::vector<Preset const *> const &templates, std::vector<std::string> &failures, std::string const &printer, std::function <std::string(std::string)> create_filament_id, bool force_rewritten = false);
    bool clone_presets_for_filament(Preset const *const &     preset,
                                    std::vector<std::string> &failures,
                                    std::string const &       filament_name,
                                    std::string const &       filament_id,
                                    const DynamicConfig &     dynamic_config,
                                    const std::string &       compatible_printers,
                                    bool                      force_rewritten = false);

    std::map<std::string, std::vector<Preset const *>> get_filament_presets() const;

    // 返回加载的预设，如果选择了现有预设并从配置修改则返回true。
    // 在这种情况下，为多材料打印机加载的后续耗材不应被修改，
    // 而是应创建外部预设。
    enum class LoadAndSelect {
        // 从不选择
        Never,
        // 总是选择
        Always,
        // 仅当配置文件被修改时才选择。
        OnlyIfModified,
    };
    std::pair<Preset*, bool> load_external_preset(
        // 配置文件源文件的路径（G-code、AMF或3MF文件、配置文件）
        const std::string           &path,
        // 配置文件的名称，从源文件名派生。
        const std::string           &name,
        // 配置文件的原始名称，从加载的配置中提取。如果名称未存储则为空。
        const std::string           &original_name,
        // 用于初始化预设的配置。
        const DynamicPrintConfig    &config,
        // 不同的设置列表
        const std::set<std::string> &different_settings_list,
        // 加载后是否选择预设？
        LoadAndSelect                select = LoadAndSelect::Always,
        const Semver                file_version = Semver(),
        const std::string           filament_id = std::string());

    // 以新名称保存预设。如果名称与旧名称不同，
    // 则新预设将存储到预设列表中。
    // 所有预设都被标记为未修改，新预设被激活。
    //BBS: add project embedded preset logic
    void            save_current_preset(const std::string &new_name, bool detach = false, bool save_to_project = false, Preset* _curr_preset = nullptr, const Preset* _current_printer = nullptr);

    // 删除当前预设，激活第一个可见预设。
    // 如果预设成功删除则返回true。
    bool            delete_current_preset();
    // 删除当前预设，激活第一个可见预设。
    // 如果预设成功删除则返回true。
    bool            delete_preset(const std::string& name);

    // 启用/禁用 "- default -" 预设。
    void            set_default_suppressed(bool default_suppressed);
    bool            is_default_suppressed() const { return m_default_suppressed; }

    // 选择一个预设。如果提供了无效索引，则选择第一个可见预设。
    Preset&         select_preset(size_t idx);
    // 返回选中的预设，不应用用户修改。
    Preset&         get_selected_preset() {
        //BBS fix crash when m_idx_selected == -1, give a default value
        if ((m_idx_selected < 0) || (m_idx_selected >= m_presets.size())) {
            select_preset(first_visible_idx());
        }
        return m_presets[m_idx_selected];
    }
    const Preset&   get_selected_preset() const { return m_presets[m_idx_selected]; }
    size_t          get_selected_idx()    const { return m_idx_selected; }
    // 返回选中预设的名称，如果未选择预设则返回空字符串。
    std::string     get_selected_preset_name() const {
        if (m_idx_selected == size_t(-1) || m_idx_selected >= m_presets.size())
            return std::string();
        return this->get_selected_preset().name;
    }
    // 对于当前编辑的预设，如果存在则返回父预设。
    // 如果没有父预设，则返回nullptr。
    // 父预设可以是系统预设或用户预设，这将在UI中反映。
    const Preset*   get_selected_preset_parent() const;
	// 获取子预设的父预设，基于子预设的"inherits"字段，
	// 在m_presets和m_map_system_profile_renamed中搜索"inherits"配置文件名称。
	const Preset*	get_preset_parent(const Preset& child) const;
	const Preset*	get_preset_base(const Preset& child) const;
	// 返回包含用户修改的选中预设。
    Preset&         get_edited_preset()         { return m_edited_preset; }
    const Preset&   get_edited_preset() const   { return m_edited_preset; }

    const Preset& get_selected_preset_base() const { return *get_preset_base(m_presets[m_idx_selected]); }

    // 返回最后保存的预设。
//  const Preset&   get_saved_preset() const { return m_saved_preset; }

    // 返回定义了供应商的第一个父配置文件的供应商，如果不存在则返回null。
    PresetWithVendorProfile get_preset_with_vendor_profile(const Preset &preset) const;
    PresetWithVendorProfile get_edited_preset_with_vendor_profile() const { return this->get_preset_with_vendor_profile(this->get_edited_preset()); }

    const std::string& 		get_preset_name_by_alias(const std::string& alias) const;
	const std::string*		get_preset_name_renamed(const std::string &old_name) const;
    bool                    is_alias_exist(const std::string &alias, Preset* preset = nullptr);
    void                    set_printer_hold_alias(const std::string &alias, Preset &preset);

	// 用于从Tab更新预设选择
	const std::deque<Preset>&	get_presets() const	{ return m_presets; }
    size_t                      get_idx_selected()	{ return m_idx_selected; }
	static const std::string&	get_suffix_modified();

    // 返回可能带有修改的预设。
	Preset&			default_preset(size_t idx = 0)		 { assert(idx < m_num_default_presets); return m_presets[idx]; }
	const Preset&   default_preset(size_t idx = 0) const { assert(idx < m_num_default_presets); return m_presets[idx]; }
	virtual const Preset& default_preset_for(const DynamicPrintConfig & /* config */) const { return this->default_preset(); }
    // 按索引返回预设。如果预设处于活动状态，则返回临时副本。
    Preset&         preset(size_t idx, bool real = false) {
        if (real) return m_presets[idx];
        return (idx == m_idx_selected) ? m_edited_preset : m_presets[idx];
    }
    const Preset&   preset(size_t idx) const    { return const_cast<PresetCollection*>(this)->preset(idx); }
    void            discard_current_changes() {
        m_presets[m_idx_selected].reset_dirty();
        m_edited_preset = m_presets[m_idx_selected];
//        update_saved_preset_from_current_preset();
    }

    // 按名称返回预设。如果预设处于活动状态，则返回临时副本。
    // 如果按名称未找到预设，则返回null。
    // 如果设置 real = true 则返回真实指针
    Preset* find_preset(const std::string& name, bool first_visible_if_not_found = false, bool real = false, bool only_from_library = false);
    const Preset* find_preset(const std::string& name, bool first_visible_if_not_found = false) const
    {
        return const_cast<PresetCollection*>(this)->find_preset(name, first_visible_if_not_found);
    }
    // Orca: 查找预设，如果未找到，继续在重命名历史中搜索。此函数仅应在查找自定义预设的系统（父）预设时使用。
    Preset* find_preset2(const std::string& name, bool auto_match = true);

    std::vector<std::string> diameters_of_selected_printer();
    // 当前编辑预设的printer_model所附的所有喷嘴变体（忽略is_visible）。
    std::vector<std::string> diameters_for_same_printer_model();

    const Preset* find_preset2(const std::string& name, bool auto_match = true) const
    {
        return const_cast<PresetCollection*>(this)->find_preset2(name, auto_match);
    }
    size_t first_visible_idx() const;
    // 返回第一个兼容预设的索引。当然至少'- default -'预设应该是兼容的。
    // 如果某个首选替代项兼容，则选择它。
    template<typename PreferedCondition> size_t first_compatible_idx(PreferedCondition prefered_condition) const
    {
        size_t i             = m_default_suppressed ? m_num_default_presets : 0;
        size_t n             = this->m_presets.size();
        size_t i_compatible  = n;
        int    match_quality = -1;
        for (; i < n; ++i)
            // 由于我们使用来自向导的耗材选择，因此也需要控制预设的可见性
            if (m_presets[i].is_compatible && m_presets[i].is_visible) {
                int this_match_quality = prefered_condition(m_presets[i]);
                if (this_match_quality > match_quality) {
                    if (match_quality == std::numeric_limits<int>::max())
                        // 将找不到更好的匹配。
                        return i;
                    // 将具有最高匹配质量的第一个兼容配置文件存储到i_compatible中。
                    i_compatible  = i;
                    match_quality = this_match_quality;
                }
            }
        return (i_compatible == n) ?
                    // 未找到兼容的预设，返回默认预设。
                    0 :
                    // 找到兼容的预设。
                    i_compatible;
}
    // 返回第一个兼容预设的索引。当然至少'- default -'预设应该是兼容的。
    size_t          first_compatible_idx() const { return this->first_compatible_idx([](const Preset&) -> int { return 0; }); }

    // 返回第一个可见预设的索引。当然至少'- default -'预设应该是可见的。
    // 返回第一个可见预设。当然至少'- default -'预设应该是可见的。
    Preset&         first_visible()             { return this->preset(this->first_visible_idx()); }
    const Preset&   first_visible() const       { return this->preset(this->first_visible_idx()); }
    Preset&         first_compatible()          { return this->preset(this->first_compatible_idx()); }
    template<typename PreferedCondition>
    Preset&         first_compatible(PreferedCondition prefered_condition) { return this->preset(this->first_compatible_idx(prefered_condition)); }
    const Preset&   first_compatible() const    { return this->preset(this->first_compatible_idx()); }

    // 返回预设数量，包括"- default -"预设。
    size_t          size() const                { return m_presets.size(); }
    bool            has_defaults_only() const   { return m_presets.size() <= m_num_default_presets; }

    // 对于Print/Filament预设，禁用那些与打印机不兼容的预设。
    template<typename PreferedCondition>
    void            update_compatible(const PresetWithVendorProfile &active_printer, const PresetWithVendorProfile *active_print, PresetSelectCompatibleType select_other_if_incompatible, PreferedCondition prefered_condition)
    {
        if (this->update_compatible_internal(active_printer, active_print, select_other_if_incompatible) == (size_t)-1)
            // 查找其他兼容的预设，或"-- default --"预设。
            this->select_preset(this->first_compatible_idx(prefered_condition));
    }
    void            update_compatible(const PresetWithVendorProfile &active_printer, const PresetWithVendorProfile *active_print, PresetSelectCompatibleType select_other_if_incompatible)
        { this->update_compatible(active_printer, active_print, select_other_if_incompatible, [](const Preset&) -> int { return 0; }); }

    size_t          num_visible() const { return std::count_if(m_presets.begin(), m_presets.end(), [](const Preset &preset){return preset.is_visible;}); }

    // 比较get_selected_preset()与get_edited_preset()配置的内容，如果不同则返回true。
    bool                        current_is_dirty() const
        { return is_dirty(&this->get_edited_preset(), &this->get_selected_preset()); }
    // 比较get_selected_preset()与get_edited_preset()配置的内容，返回差异的键列表。
    std::vector<std::string>    current_dirty_options(const bool deep_compare = false) const
        { return dirty_options(&this->get_edited_preset(), &this->get_selected_preset(), deep_compare); }
    // 比较get_selected_preset()与get_edited_preset()配置的内容，返回差异的键列表。
    std::vector<std::string>    current_different_from_parent_options(const bool deep_compare = false) const
        { return dirty_options(&this->get_edited_preset(), this->get_selected_preset_parent(), deep_compare); }

    // 比较get_saved_preset()与get_edited_preset()配置的内容，如果不同则返回true。
    bool                        saved_is_dirty() const
        { return is_dirty(&this->get_edited_preset(), &m_saved_preset); }
    // Compare the content of get_saved_preset() with get_edited_preset() configs, return the list of keys where they differ.
//    std::vector<std::string>    saved_dirty_options() const
//        { return dirty_options(&this->get_edited_preset(), &this->get_saved_preset(), /* deep_compare */ false); }
    // 将编辑后的预设复制到已保存的预设中。
    void                        update_saved_preset_from_current_preset() { m_saved_preset = m_edited_preset; }

    // 返回系统预设名称的排序列表。
    // 用于在导入用户配置包时验证"inherits"标志。
    // 返回所有系统预设的名称，包括这些预设的旧名称。
    std::vector<std::string>    system_preset_names() const;

    // 更新当前预设的dirty标志
    // 如果dirty标志更改则返回true。
    bool            update_dirty();

    // 按名称选择配置文件。如果选择更改则返回true。
    // 不使用force时，仅当索引更改时更新选择。
    // 使用force时，如果新索引与旧索引相同，则恢复更改。
    bool            select_preset_by_name(const std::string &name, bool force);
    bool is_base_preset(const Preset &preset) const { return preset.is_system || (preset.is_user() && preset.inherits().empty()); }

    // 从配置文件名称生成文件路径。如果缺少".ini"后缀则添加。
    std::string     path_from_name(const std::string &new_name, bool detach = false) const;
    std::string     path_for_preset(const Preset & preset) const;

    size_t num_default_presets() { return m_num_default_presets; }

protected:
    PresetCollection() = default;
    // 复制构造函数和复制运算符不应在PresetBundle外部使用，
    // 因为Profile::vendor指向存储在父PresetBundle中的VendorProfile实例！
    PresetCollection(const PresetCollection &other) = default;
    //BBS: add operator= logic insteadof default
    PresetCollection& operator=(const PresetCollection &other);
    // 使用上述默认运算符复制集合后，调用此函数
    // 以调整Profile::vendor指针。
    void            update_vendor_ptrs_after_copy(const VendorMap &vendors);

    // 选择一个预设（如果存在）。如果不存在，则选择无效索引(-1)。
    // 这是一个临时状态，应由下一步立即修复。
    bool            select_preset_by_name_strict(const std::string &name);

    // 将一个供应商的预设与另一个供应商的预设合并，报告重复项。
    std::vector<std::string> merge_presets(PresetCollection &&other, const VendorMap &new_vendors);

    // 从加载的系统配置文件更新m_map_alias_to_profile_name。
	void 			update_map_alias_to_profile_name();

    // 从加载的系统配置文件更新m_map_system_profile_renamed。
    void 			update_map_system_profile_renamed();

    // Orca: 更新已加载系统配置文件的m_excluded_from。
    void 			update_library_profile_excluded_from();


    void            set_custom_preset_alias(Preset &preset);

private:
    // 在排序的预设列表中查找预设位置。
    // "-- default --" 预设始终是第一个，因此需要不同处理。
    // 如果预设不存在，则返回一个迭代器，指示在何处插入同名预设。
    std::deque<Preset>::iterator find_preset_internal(const std::string &name, bool from_orca_lib_only = false)
    {
        auto it = Slic3r::lower_bound_by_predicate(m_presets.begin() + m_num_default_presets, m_presets.end(), [&name](const auto& l) { return l.name < name;  });
        if (it == m_presets.end() || it->name != name) {
            // 在排序的非默认预设列表中未找到预设。尝试默认值。
            for (size_t i = 0; i < m_num_default_presets; ++ i)
                if (m_presets[i].name == name && (!from_orca_lib_only || m_presets[i].m_from_orca_filament_lib)) {
                    it = m_presets.begin() + i;
                    break;
                }
        }
        return it;
    }
    std::deque<Preset>::const_iterator find_preset_internal(const std::string &name) const
        { return const_cast<PresetCollection*>(this)->find_preset_internal(name); }
    std::deque<Preset>::iterator 	   find_preset_renamed(const std::string &name) {
    	auto it_renamed = m_map_system_profile_renamed.find(name);
    	auto it = (it_renamed == m_map_system_profile_renamed.end()) ? m_presets.end() : this->find_preset_internal(it_renamed->second);
    	assert((it_renamed == m_map_system_profile_renamed.end()) || (it != m_presets.end() && it->name == it_renamed->second));
    	return it;
    }
    std::deque<Preset>::const_iterator find_preset_renamed(const std::string &name) const
        { return const_cast<PresetCollection*>(this)->find_preset_renamed(name); }

    size_t update_compatible_internal(const PresetWithVendorProfile &active_printer, const PresetWithVendorProfile *active_print, PresetSelectCompatibleType unselect_if_incompatible);
public:
    static bool                     is_dirty(const Preset *edited, const Preset *reference);
    static std::vector<std::string> dirty_options(const Preset *edited, const Preset *reference, const bool deep_compare = false);
    //BBS: add function for dirty_options_without_option_list
    static std::vector<std::string> dirty_options_without_option_list(const Preset *edited, const Preset *reference, const std::set<std::string>& option_ignore_list, const bool deep_compare = false);
private:
    // 此PresetCollection的类型：TYPE_PRINT、TYPE_FILAMENT或TYPE_PRINTER。
    Preset::Type            m_type;
    // 预设列表，以"- default -"预设开始。
    // 使用deque强制容器为每个条目分配一个对象，
    // 以便预设的地址在容器调整大小时不会改变。
    std::deque<Preset>      m_presets;
    // 系统配置文件可能有别名。映射到完整的配置文件名称。
    std::map<std::string, std::vector<std::string>> m_map_alias_to_profile_name;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_printer_hold_alias;
    // 从旧系统配置文件名称到当前系统配置文件名称的映射。
    std::map<std::string, std::string> m_map_system_profile_renamed;
    // 最初，此预设包含选中预设的副本。之后，此副本可能由用户修改。
    Preset                  m_edited_preset;
    // 包含最后保存的选中预设的副本。
    Preset                  m_saved_preset;

    // 选中的预设。
    size_t                  m_idx_selected;
    // "- default -" 预设是否被抑制？
    bool                    m_default_suppressed  = true;
    size_t                  m_num_default_presets = 0;

    // 存储配置文件的目录路径。
    std::string             m_dir_path;

    // to access select_preset_by_name_strict() and the default & copy constructors.
    friend class PresetBundle;

    //BBS: mutex
    std::mutex          m_mutex;

    // Orca: 仅用于验证
    int m_errors = 0;
};

// 打印机支持FFF和SLA技术，使用不同的配置值集，
// 因此此PresetCollection需要处理两个默认值。
class PrinterPresetCollection : public PresetCollection
{
public:
    PrinterPresetCollection(Preset::Type type, const std::vector<std::string> &keys, const Slic3r::StaticPrintConfig &defaults, const std::string &default_name = "Default Printer") :
		PresetCollection(type, keys, defaults, default_name) {}

    const Preset&   default_preset_for(const DynamicPrintConfig &config) const override;

    const Preset*   find_system_preset_by_model_and_variant(const std::string &model_id, const std::string &variant) const;
    const Preset*   find_custom_preset_by_model_and_variant(const std::string &model_id, const std::string &variant) const;

    bool            only_default_printers() const;
private:
    PrinterPresetCollection() = default;
    PrinterPresetCollection(const PrinterPresetCollection &other) = default;
    PrinterPresetCollection& operator=(const PrinterPresetCollection &other) = default;

    friend class PresetBundle;
};

namespace PresetUtils {
	// 系统配置文件的PrinterModel，此预设从中派生，如果未从系统配置文件派生则为null。
	const VendorProfile::PrinterModel* system_printer_model(const Preset &preset);
    std::string system_printer_bed_model(const Preset& preset);
    std::string system_printer_bed_texture(const Preset& preset);
    std::string system_printer_hotend_model(const Preset& preset);
} // namespace PresetUtils


//////////////////////////////////////////////////////////////////////

class PhysicalPrinter
{
public:
    PhysicalPrinter(const std::string& name, const DynamicPrintConfig &default_config);
    PhysicalPrinter(const std::string& name, const DynamicPrintConfig &default_config, const Preset& preset);
    void set_name(const std::string &name);

    // 物理打印机名称，通常从文件名派生。
    std::string         name;
    // 物理打印机的文件名。
    std::string         file;
    // 配置数据，从文件加载或从默认值设置。
    DynamicPrintConfig  config;
    // 与此物理打印机一起使用的预设集
    std::set<std::string> preset_names;

    // 此配置文件是否已加载？
    bool                loaded = false;

    static std::string  separator();
    static const std::vector<std::string>&  printer_options();
    static const std::vector<std::string>&  print_host_options();
    static std::vector<std::string>         presets_with_print_host_information(const PrinterPresetCollection& printer_presets);
    static bool has_print_host_information(const DynamicPrintConfig& config);

    const std::set<std::string>&            get_preset_names() const;

    void                update_preset_names_in_config();

    //BBS: change to json format
    //void                save() { this->config.save(this->file); }
    void                save(DynamicPrintConfig* parent_config) { this->config.save_to_json(this->file, std::string("Physical_Printer"), std::string("User"), std::string(SLIC3R_VERSION)); }
    void                save(const std::string& file_name_from, const std::string& file_name_to);

    void                update_from_preset(const Preset& preset);
    void                update_from_config(const DynamicPrintConfig &new_config);

    // 将预设添加到preset_names
    // 如果集合中已存在此名称的预设，则返回false
    bool                add_preset(const std::string& preset_name);
    bool                delete_preset(const std::string& preset_name);
    void                reset_presets();

    // 返回打印机技术，如果未设置打印机技术则返回ptFFF。
    static PrinterTechnology printer_technology(const DynamicPrintConfig& cfg) {
        auto* opt = cfg.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
        // 导入旧配置文件时可能触发以下断言，
        // 但将其保留在此处以捕获不应查询"printer_technology"键的情况更安全。
        return (opt == nullptr) ? ptFFF : opt->value;
    }
    PrinterTechnology   printer_technology() const { return printer_technology(this->config); }

    // 按预设名称进行字典序排序。预设名称在单个PresetCollection中应是唯一的。
    bool                operator<(const PhysicalPrinter& other) const { return this->name < other.name; }

    // 获取包含预设名称的完整打印机名称
    std::string         get_full_name(std::string preset_name) const;

    // 从包含预设名称的完整名称中获取打印机名称
    static std::string  get_short_name(std::string full_name);

    // 从包含打印机名称的完整名称中获取预设名称
    static std::string  get_preset_name(std::string full_name);

protected:
    friend class        PhysicalPrinterCollection;
};


// ---------------------------------
// ***  PhysicalPrinterCollection  ***
// ---------------------------------

// 物理打印机的集合
class PhysicalPrinterCollection
{
public:
    PhysicalPrinterCollection(const std::vector<std::string>& keys);

    typedef std::deque<PhysicalPrinter>::iterator Iterator;
    typedef std::deque<PhysicalPrinter>::const_iterator ConstIterator;
    Iterator        begin() { return m_printers.begin(); }
    ConstIterator   begin() const { return m_printers.cbegin(); }
    ConstIterator   cbegin() const { return m_printers.cbegin(); }
    Iterator        end() { return m_printers.end(); }
    ConstIterator   end() const { return m_printers.cend(); }
    ConstIterator   cend() const { return m_printers.cend(); }

    bool            empty() const {return m_printers.empty(); }

    void            reset(bool delete_files) {};

    const std::deque<PhysicalPrinter>& operator()() const { return m_printers; }

    // 从提供的目录路径加载特定类型的ini文件。
    void            load_printers(const std::string& dir_path, const std::string& subdir, PresetsConfigSubstitutions& substitutions, ForwardCompatibilitySubstitutionRule rule);
    void            load_printers_from_presets(PrinterPresetCollection &printer_presets);
    // 从加载的配置中加载打印机
    void            load_printer(const std::string& path, const std::string& name, DynamicPrintConfig&& config, bool select, bool save=false);

    // 以新名称保存打印机。如果名称与旧名称不同，
    // 新打印机将存储到打印机列表中。
    // 新打印机被激活。
    void            save_printer(PhysicalPrinter& printer, const std::string& renamed_from = "");

    // 删除当前预设，激活第一个可见预设。
    // 如果预设成功删除则返回true。
    bool            delete_printer(const std::string& name);
    // 删除选中的预设
    // 如果预设成功删除则返回true。
    bool            delete_selected_printer();
    // 从所有打印机中删除preset_name预设：
    // 如果是打印机的最后一个预设且first_check == false，则删除此打印机
    // 如果所有预设成功删除则返回true。
    bool            delete_preset_from_printers(const std::string& preset_name);

    // 获取拥有多个预设且"preset_names"预设是其中之一的所有打印机列表
    std::vector<std::string> get_printers_with_preset( const std::string &preset_name);
    // 获取仅包含"preset_names"预设的打印机列表
    std::vector<std::string> get_printers_with_only_preset( const std::string &preset_name);

    // 返回选中的预设，不应用用户修改。
    PhysicalPrinter&        get_selected_printer() { return m_printers[m_idx_selected]; }
    const PhysicalPrinter&  get_selected_printer() const { return m_printers[m_idx_selected]; }

    size_t                  get_selected_idx()    const { return m_idx_selected; }
    // 返回选中预设的名称，如果未选择预设则返回空字符串。
    std::string             get_selected_printer_name() const { return (m_idx_selected == size_t(-1)) ? std::string() : this->get_selected_printer().name; }
    // 返回选中打印机的配置，如果未选中打印机则返回nullptr。
    DynamicPrintConfig*     get_selected_printer_config() { return (m_idx_selected == size_t(-1)) ? nullptr : &(this->get_selected_printer().config); }
    // 返回选中打印机的配置，如果未选中打印机则返回nullptr。
    PrinterTechnology       get_selected_printer_technology() { return (m_idx_selected == size_t(-1)) ? PrinterTechnology::ptAny : this->get_selected_printer().printer_technology(); }

    // 每个物理打印机可以有多个相关预设，
    // 因此，使用以下函数获取列表中选中的确切名称：
    // 返回选中打印机的完整名称，如果未选中预设则返回空字符串。
    std::string     get_selected_full_printer_name() const;
    // 返回选中预设的打印机型号，如果未选中预设则返回空字符串。
    std::string     get_selected_printer_preset_name() const { return (m_idx_selected == size_t(-1)) ? std::string() : m_selected_preset; }

    // 按完整打印机名称选择打印机，包含打印机名称、分隔符和选中预设名称
    // 如果full_name不包含选中预设的名称，则为该打印机选择列表中的第一个预设
    void select_printer(const std::string& full_name);
    void select_printer(const PhysicalPrinter& printer);
    void select_printer(const std::string& printer_name, const std::string& preset_name);
    bool has_selection() const;
    void unselect_printer() ;
    bool is_selected(ConstIterator it, const std::string &preset_name) const;

    // 按索引返回打印机。如果打印机处于活动状态，则返回临时副本。
    PhysicalPrinter& printer(size_t idx) { return m_printers[idx]; }
    const PhysicalPrinter& printer(size_t idx) const { return const_cast<PhysicalPrinterCollection*>(this)->printer(idx); }

    // 按名称返回预设。如果预设处于活动状态，则返回临时副本。
    // 如果按名称未找到预设，则返回null。
    // 支持大小写（不）敏感搜索
    PhysicalPrinter* find_printer(const std::string& name, bool case_sensitive_search = true);
    const PhysicalPrinter* find_printer(const std::string& name, bool case_sensitive_search = true) const
    {
        return const_cast<PhysicalPrinterCollection*>(this)->find_printer(name, case_sensitive_search);
    }

    // 从配置文件名称生成文件路径。如果缺少".ini"后缀则添加。
    std::string     path_from_name(const std::string& new_name) const;

    const DynamicPrintConfig& default_config() const { return m_default_config; }

private:
    friend class PresetBundle;
    PhysicalPrinterCollection() = default;
    PhysicalPrinterCollection& operator=(const PhysicalPrinterCollection& other) = default;

    // 在排序的打印机列表中查找物理打印机位置。
    // 打印机名称应唯一且不区分大小写
    // 当需要不区分大小写的搜索时，使用case_sensitive_search = false调用此函数
    std::deque<PhysicalPrinter>::iterator find_printer_internal(const std::string& name, bool case_sensitive_search = true);
    std::deque<PhysicalPrinter>::const_iterator find_printer_internal(const std::string& name, bool case_sensitive_search = true) const
    {
        return const_cast<PhysicalPrinterCollection*>(this)->find_printer_internal(name);
    }

    // 打印机列表
    // 使用deque强制容器为每个条目分配一个对象，
    // 以便预设的地址在容器调整大小时不会改变。
    std::deque<PhysicalPrinter> m_printers;

    // 包含PhysicalPrinter::printer_options()所有键/值对的物理打印机默认配置。
    DynamicPrintConfig          m_default_config;

    // 选中的打印机。
    size_t                      m_idx_selected = size_t(-1);
    // 当前为此打印机选择的预设名称
    std::string                 m_selected_preset;

    // 存储配置文件的目录路径。
    std::string                 m_dir_path;
};


} // namespace Slic3r

#endif /* slic3r_Preset_hpp_ */
