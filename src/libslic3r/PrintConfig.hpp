// Slic3r 的配置存储。
//
// 配置存储分为静态和动态两种。
// DynamicPrintConfig 主要用于用户界面，而 StaticPrintConfig 用于切片和 G-code 生成。
//
// 从 StaticPrintConfig 派生的类形成以下层次结构。
//
// FullPrintConfig
//    PrintObjectConfig
//    PrintRegionConfig
//    PrintConfig
//        GCodeConfig
//

#ifndef slic3r_PrintConfig_hpp_
#define slic3r_PrintConfig_hpp_

#include "libslic3r.h"
#include "Config.hpp"
#include "Polygon.hpp"
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/tuple/to_seq.hpp>

namespace Slic3r {

enum GCodeFlavor : unsigned char {
    gcfMarlinLegacy, gcfKlipper, gcfRepRapFirmware, gcfMarlinFirmware, gcfRepRapSprinter, gcfRepetier, gcfTeacup, gcfMakerWare, gcfSailfish, gcfMach3, gcfMachinekit,
    gcfSmoothie, gcfNoExtrusion
};


enum class FuzzySkinType {
    None,
    External,
    All,
    AllWalls,
};

enum class FuzzySkinMode {
    Displacement,
    Extrusion,
    Combined,
};

enum class NoiseType {
    Classic,
    Perlin,
    Billow,
    RidgedMulti,
    Voronoi,
};

enum PrintHostType {
    htPrusaLink, htPrusaConnect, htOctoPrint, htDuet, htFlashAir, htAstroBox, htRepetier, htMKS, htESP3D, htCrealityPrint, htObico, htFlashforge, htSimplyPrint, htElegooLink, htMoonRaker_mqtt, htMoonRaker, 
};

enum AuthorizationType {
    atKeyPassword, atUserPassword
};

enum InfillPattern : int {
    ipMonotonic, ipMonotonicLine,
    ipRectilinear, ipAlignedRectilinear, ipZigZag, ipCrossZag, ipLockedZag,
    ipLine, ipGrid,
    ipTriangles, ipStars,
    ipCubic, ipAdaptiveCubic, ipQuarterCubic, ipSupportCubic, ipLightning,
    ipHoneycomb, ip3DHoneycomb, ipLateralHoneycomb, ipLateralLattice,
    ipCrossHatch, ipTpmsD, ipTpmsFK, ipGyroid,
    ipConcentric, ipHilbertCurve, ipArchimedeanChords, ipOctagramSpiral,
    ipSupportBase, ipConcentricInternal,
    ipCount,
};

enum class IroningType {
    NoIroning,
    TopSurfaces,
    TopmostOnly,
    AllSolid,
    Count,
};

//BBS
enum class WallInfillOrder {
    InnerOuterInfill,
    OuterInnerInfill,
    InfillInnerOuter,
    InfillOuterInner,
    InnerOuterInnerInfill,
    Count,
};

// BBS
enum class WallSequence {
    InnerOuter,
    OuterInner,
    InnerOuterInner,
    Count,
};

// Orca
enum class WallDirection
{
    Auto,
    CounterClockwise,
    Clockwise,
    Count,
};

//BBS
enum class PrintSequence {
    ByLayer,
    ByObject,
    ByDefault,
    Count,
};

enum class PrintOrder
{
    Default,
    AsObjectList,
    Count,
};

enum class SlicingMode
{
    // Regular, applying ClipperLib::pftNonZero rule when creating ExPolygons.
    Regular,
    // Compatible with 3DLabPrint models, applying ClipperLib::pftEvenOdd rule when creating ExPolygons.
    EvenOdd,
    // Orienting all contours CCW, thus closing all holes.
    CloseHoles,
};

enum SupportMaterialPattern {
    smpDefault,
    smpRectilinear, smpRectilinearGrid, smpHoneycomb,
    smpLightning,
    smpNone,
};

enum SupportMaterialStyle {
    smsDefault, smsGrid, smsSnug, smsTreeSlim, smsTreeStrong, smsTreeHybrid, smsTreeOrganic,
};

enum LongRectrationLevel
{
    Disabled=0,
    EnableMachine,
    EnableFilament
};

enum SupportMaterialInterfacePattern {
    smipAuto, smipRectilinear, smipConcentric, smipRectilinearInterlaced, smipGrid
};

// BBS
enum SupportType {
    stNormalAuto, stTreeAuto, stNormal, stTree
};
inline bool is_tree(SupportType stype)
{
    return std::set<SupportType>{stTreeAuto, stTree}.count(stype) != 0;
};
inline bool is_tree_slim(SupportType type, SupportMaterialStyle style)
{
    return is_tree(type) && style==smsTreeSlim;
};
inline bool is_auto(SupportType stype)
{
    return std::set<SupportType>{stNormalAuto, stTreeAuto}.count(stype) != 0;
};

enum SeamPosition {
    spNearest, spAligned, spAlignedBack, spRear, spRandom
};

// Orca
enum class SeamScarfType {
    None,
    External,
    All,
};

// Orca
enum EnsureVerticalShellThickness {
    evstNone,
    evstCriticalOnly,
    evstModerate,
    evstAll,
};

//Orca
enum InternalBridgeFilter {
    ibfDisabled, ibfLimited, ibfNofilter
};

//Orca
enum EnableExtraBridgeLayer {
    eblDisabled, eblExternalBridgeOnly, eblInternalBridgeOnly, eblApplyToAll
};

//Orca
enum GapFillTarget {
     gftEverywhere, gftTopBottom, gftNowhere
 };


enum LiftType {
    NormalLift,
    SpiralLift,
    LazyLift
};

enum SLAMaterial {
    slamTough,
    slamFlex,
    slamCasting,
    slamDental,
    slamHeatResistant,
};

enum SLADisplayOrientation {
    sladoLandscape,
    sladoPortrait
};

enum SLAPillarConnectionMode {
    slapcmZigZag,
    slapcmCross,
    slapcmDynamic
};

enum BrimType {
    btAutoBrim,  // BBS
    btEar, // Orca
    btPainted,  // BBS
    btOuterOnly,
    btInnerOnly,
    btOuterAndInner,
    btNoBrim,
};

enum TimelapseType : int {
    tlTraditional = 0,
    tlSmooth
};

enum SkirtType {
    stCombined, stPerObject
};

enum DraftShield {
    dsDisabled, dsEnabled
};

enum class PerimeterGeneratorType
{
    // 经典周长生成器，使用 Clipper 偏移和恒定挤出宽度。
    Classic,
    // 基于论文 "A framework for adaptive width control of dense contour-parallel toolpaths in fused deposition modeling" 的可变挤出宽度周长生成器，从 Cura 移植。
    Arachne
};

// BBS
enum OverhangFanThreshold {
    Overhang_threshold_none = 0,
    Overhang_threshold_1_4,
    Overhang_threshold_2_4,
    Overhang_threshold_3_4,
    Overhang_threshold_4_4,
    Overhang_threshold_bridge
};

// BBS
enum BedType {
    btDefault = 0,
    btPC,
    btEP,
    btPEI,
    btPTE,
    btPCT,
    btGESP,
    btSuperTack,
    btCount
};

// BBS
enum LayerSeq {
    flsAuto, 
    flsCustomize
};

// BBS
enum NozzleType {
    ntUndefine = 0,
    ntHardenedSteel,
    ntStainlessSteel,
    ntBrass,
    ntCount
};

static std::unordered_map<NozzleType, std::string>NozzleTypeEumnToStr = {
    {NozzleType::ntUndefine,        "undefine"},
    {NozzleType::ntHardenedSteel,   "hardened_steel"},
    {NozzleType::ntStainlessSteel,  "stainless_steel"},
    {NozzleType::ntBrass,           "brass"}
};

static std::unordered_map<std::string, NozzleType>NozzleTypeStrToEumn = {
    {"undefine", NozzleType::ntUndefine},
    {"hardened_steel", NozzleType::ntHardenedSteel},
    {"stainless_steel", NozzleType::ntStainlessSteel},
    {"brass", NozzleType::ntBrass}
};

// BBS
enum PrinterStructure {
    psUndefine=0,
    psCoreXY,
    psI3,
    psHbot,
    psDelta
};

// BBS
enum ZHopType {
    zhtAuto = 0,
    zhtNormal,
    zhtSlope,
    zhtSpiral,
    zhtCount
};

enum FilamentMapMode {
    fmmAutoForFlush,
    fmmAutoForMatch,
    fmmManual,
    fmmDefault
};

enum NozzleVolumeType {
    nvtNormal = 0,
    nvtBigTraffic,
    nvtMaxNozzleVolumeType = nvtBigTraffic
};

enum RetractLiftEnforceType {
    rletAllSurfaces = 0,
    rletTopOnly,
    rletBottomOnly,
    rletTopAndBottom
};

enum class GCodeThumbnailsFormat {
    PNG, JPG, QOI, BTT_TFT, ColPic
};

enum CounterboreHoleBridgingOption {
    chbNone, chbBridges, chbFilled
};

 enum WipeTowerWallType {
     wtwRectangle = 0,
     wtwCone,
     wtwRib
 };

static std::string bed_type_to_gcode_string(const BedType type)
{
    std::string type_str;

    switch (type) {
    case btSuperTack:
        type_str = "supertack_plate";
        break;
    case btPC:
        type_str = "cool_plate";
        break;
    case btPCT:
        type_str = "textured_cool_plate";
        break;
    case btEP:
        type_str = "eng_plate";
        break;
    case btPEI:
        type_str = "hot_plate";
        break;
    case btPTE:
        type_str = "textured_plate";
        break;
    default:
        type_str = "unknown";
        break;
    }

    return type_str;
}

static std::string get_bed_temp_key(const BedType type)
{
    if (type == btSuperTack)
        return "supertack_plate_temp";

    if (type == btPC)
        return "cool_plate_temp";

    if (type == btPCT)
        return "textured_cool_plate_temp";

    if (type == btEP)
        return "eng_plate_temp";

    if (type == btPEI)
        return "hot_plate_temp";

    if (type == btPTE)
        return "textured_plate_temp";

    if (type == btGESP)
        return "graphic_effect_plate_temp";

    return "";
}

static std::string get_bed_temp_1st_layer_key(const BedType type)
{
    if (type == btSuperTack)
        return "supertack_plate_temp_initial_layer";

    if (type == btPC)
        return "cool_plate_temp_initial_layer";

    if (type == btPCT)
        return "textured_cool_plate_temp_initial_layer";

    if (type == btEP)
        return "eng_plate_temp_initial_layer";

    if (type == btPEI)
        return "hot_plate_temp_initial_layer";

    if (type == btPTE)
        return "textured_plate_temp_initial_layer";

    if (type == btGESP)
        return "graphic_effect_plate_temp_initial_layer";

    return "";
}

#define CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(NAME) \
    template<> const t_config_enum_names& ConfigOptionEnum<NAME>::get_enum_names(); \
    template<> const t_config_enum_values& ConfigOptionEnum<NAME>::get_enum_values();

CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(PrinterTechnology)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(GCodeFlavor)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(FuzzySkinType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(FuzzySkinMode)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(NoiseType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(InfillPattern)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(IroningType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SlicingMode)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SupportMaterialPattern)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SupportMaterialStyle)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SupportMaterialInterfacePattern)
// BBS
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SupportType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SeamPosition)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SeamScarfType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SLADisplayOrientation)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SLAPillarConnectionMode)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(BrimType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(TimelapseType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(BedType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(SkirtType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(DraftShield)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(ForwardCompatibilitySubstitutionRule)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(GCodeThumbnailsFormat)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(CounterboreHoleBridgingOption)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(PrintHostType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(AuthorizationType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(WipeTowerWallType)
CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS(PerimeterGeneratorType)

#undef CONFIG_OPTION_ENUM_DECLARE_STATIC_MAPS

class DynamicPrintConfig;

// 定义 Slic3r 的每一个配置选项，包括 GUI 对话框的属性。
// 不存储实际值，但定义默认值。
class PrintConfigDef : public ConfigDef
{
public:
    PrintConfigDef();

    static void handle_legacy(t_config_option_key &opt_key, std::string &value);
    static void handle_legacy_composite(DynamicPrintConfig &config);

    // Array options growing with the number of extruders
    const std::vector<std::string>& extruder_option_keys() const { return m_extruder_option_keys; }
    // Options defining the extruder retract properties. These keys are sorted lexicographically.
    // The extruder retract keys could be overidden by the same values defined at the Filament level
    // (then the key is further prefixed with the "filament_" prefix).
    const std::vector<std::string>& extruder_retract_keys() const { return m_extruder_retract_keys; }

    // BBS
    const std::vector<std::string>& filament_option_keys() const { return m_filament_option_keys; }
    const std::vector<std::string>& filament_retract_keys() const { return m_filament_retract_keys; }

private:
    void init_common_params();
    void init_fff_params();
    void init_extruder_option_keys();
    void init_sla_params();

    std::vector<std::string>    m_extruder_option_keys;
    std::vector<std::string>    m_extruder_retract_keys;

    // BBS
    void init_filament_option_keys();

    std::vector<std::string>    m_filament_option_keys;
    std::vector<std::string>    m_filament_retract_keys;
};

// Slic3r 配置选项的唯一全局定义。
// 此定义为常量。
extern const PrintConfigDef print_config_def;

class StaticPrintConfig;

// 基于打印机技术的最小物体排列距离。
double min_object_distance(const ConfigBase &cfg);

// Slic3r 动态配置，用于覆盖每个物体、每个修改体积或每个打印材料的配置。
// 动态配置也用于存储用户对打印全局参数的修改，
// 因此修改后的配置值可以与活动配置进行差异比较，
// 以使相应的切片或 G-code 生成处理步骤失效。
// 此对象映射到 Perl 作为 Slic3r::Config。
class DynamicPrintConfig : public DynamicConfig
{
public:
    DynamicPrintConfig() {}
    DynamicPrintConfig(const DynamicPrintConfig &rhs) : DynamicConfig(rhs) {}
    DynamicPrintConfig(DynamicPrintConfig &&rhs) noexcept : DynamicConfig(std::move(rhs)) {}
    explicit DynamicPrintConfig(const StaticPrintConfig &rhs);
    explicit DynamicPrintConfig(const ConfigBase &rhs) : DynamicConfig(rhs) {}

    DynamicPrintConfig& operator=(const DynamicPrintConfig &rhs) { DynamicConfig::operator=(rhs); return *this; }
    DynamicPrintConfig& operator=(DynamicPrintConfig &&rhs) noexcept { DynamicConfig::operator=(std::move(rhs)); return *this; }

    static DynamicPrintConfig  full_print_config();
    static DynamicPrintConfig* new_from_defaults_keys(const std::vector<std::string> &keys);

    // 覆盖 ConfigBase::def()。静态配置定义。存储在此 ConfigBase 中的任何值都应有其定义。
    const ConfigDef*    def() const override { return &print_config_def; }

    void                normalize_fdm(int used_filaments = 0);
    void                normalize_fdm_1();
    // 返回已更改的参数集
    t_config_option_keys normalize_fdm_2(int num_objects, int used_filaments = 0);

    void                set_num_extruders(unsigned int num_extruders);

    // BBS
    void                set_num_filaments(unsigned int num_filaments);

    //BBS
    // 验证 PrintConfig。成功时返回空字符串，否则返回错误信息。
    std::map<std::string, std::string>         validate(bool under_cli = false);

    // 验证 opt_key 是否已废弃或重命名。
    // opt_key 和 value 都可能被 handle_legacy() 修改。
    // 如果 opt_key 在此版本的 Slic3r 中不再有效，handle_legacy() 会清空 opt_key。
    // handle_legacy() 由 set_deserialize() 内部调用。
    void                handle_legacy(t_config_option_key &opt_key, std::string &value) const override
        { PrintConfigDef::handle_legacy(opt_key, value); }

    // 在整体加载配置后调用。
    // 执行复合转换，例如将多个键合并为一个键。
    // 对于单个选项的转换，调用上面的 handle_legacy() 方法。
    void                handle_legacy_composite() override
        { PrintConfigDef::handle_legacy_composite(*this); }

    //BBS 特殊情况 Support G/ Support W
    std::string get_filament_type(std::string &displayed_filament_type, int id = 0);

    bool is_custom_defined();
};

void handle_legacy_sla(DynamicPrintConfig &config);

class StaticPrintConfig : public StaticConfig
{
public:
    StaticPrintConfig() {}

    // 覆盖 ConfigBase::def()。静态配置定义。存储在此 ConfigBase 中的任何值都应有其定义。
    const ConfigDef*    def() const override { return &print_config_def; }
    // 对缓存键列表的引用。
    virtual const t_config_option_keys& keys_ref() const = 0;

protected:
    // 验证 opt_key 是否已废弃或重命名。
    // opt_key 和 value 都可能被 handle_legacy() 修改。
    // 如果 opt_key 在此版本的 Slic3r 中不再有效，handle_legacy() 会清空 opt_key。
    // handle_legacy() 由 set_deserialize() 内部调用。
    void                handle_legacy(t_config_option_key &opt_key, std::string &value) const override
        { PrintConfigDef::handle_legacy(opt_key, value); }

    // 用于维护静态选项动态映射的内部类。
    class StaticCacheBase
    {
    public:
        // 在 StaticCache 设置期间调用。
        // 将一个 ConfigOption 添加到 m_map_name_to_offset。
        template<typename T>
        void                opt_add(const std::string &name, const char *base_ptr, const T &opt)
        {
            assert(m_map_name_to_offset.find(name) == m_map_name_to_offset.end());
            m_map_name_to_offset[name] = (const char*)&opt - base_ptr;
        }

    protected:
        std::map<std::string, ptrdiff_t>    m_map_name_to_offset;
    };

    // 由拥有选项的最顶层类的类型参数化。
    template<typename T>
    class StaticCache : public StaticCacheBase
    {
    public:
        // 使用 0 调用 m_defaults 的构造函数强制 m_defaults 不运行初始化。
        StaticCache() : m_defaults(nullptr) {}
        ~StaticCache() { delete m_defaults; m_defaults = nullptr; }

        bool                initialized() const { return ! m_keys.empty(); }

        ConfigOption*       optptr(const std::string &name, T *owner) const
        {
            const auto it = m_map_name_to_offset.find(name);
            return (it == m_map_name_to_offset.end()) ? nullptr : reinterpret_cast<ConfigOption*>((char*)owner + it->second);
        }

        const ConfigOption* optptr(const std::string &name, const T *owner) const
        {
            const auto it = m_map_name_to_offset.find(name);
            return (it == m_map_name_to_offset.end()) ? nullptr : reinterpret_cast<const ConfigOption*>((const char*)owner + it->second);
        }

        const std::vector<std::string>& keys()      const { return m_keys; }
        const T&                        defaults()  const { return *m_defaults; }

        // 在 StaticCache 设置期间调用。
        // 从 m_map_name_to_offset 收集选项键，
        // 将默认值分配给 m_defaults。
        void                finalize(T *defaults, const ConfigDef *defs)
        {
            assert(defs != nullptr);
            m_defaults = defaults;
            m_keys.clear();
            m_keys.reserve(m_map_name_to_offset.size());
            for (const auto &kvp : defs->options) {
                // 通过相对于 (char*)m_defaults 的偏移量根据选项名称 kvp.first 查找选项。
                ConfigOption *opt = this->optptr(kvp.first, m_defaults);
                if (opt == nullptr)
                    // 此选项未由类型 T 的 ConfigBase 定义。
                    continue;
                m_keys.emplace_back(kvp.first);
                const ConfigOptionDef *def = defs->get(kvp.first);
                assert(def != nullptr);
                if (def->default_value)
                    opt->set(def->default_value.get());
            }
        }

    private:
        T                                  *m_defaults;
        std::vector<std::string>            m_keys;
    };
};

#define STATIC_PRINT_CONFIG_CACHE_BASE(CLASS_NAME) \
public: \
    /* 覆盖 ConfigBase::optptr()。根据给定名称查找并/或创建一个 ConfigOption 实例。 */ \
    const ConfigOption*      optptr(const t_config_option_key &opt_key) const override \
        { return s_cache_##CLASS_NAME.optptr(opt_key, this); } \
    /* 覆盖 ConfigBase::optptr()。根据给定名称查找并/或创建一个 ConfigOption 实例。 */ \
    ConfigOption*            optptr(const t_config_option_key &opt_key, bool create = false) override \
        { return s_cache_##CLASS_NAME.optptr(opt_key, this); } \
    /* 覆盖 ConfigBase::keys()。收集此配置存储维护的所有配置值的名称。 */ \
    t_config_option_keys     keys() const override { return s_cache_##CLASS_NAME.keys(); } \
    const t_config_option_keys& keys_ref() const override { return s_cache_##CLASS_NAME.keys(); } \
    static const CLASS_NAME& defaults() { assert(s_cache_##CLASS_NAME.initialized()); return s_cache_##CLASS_NAME.defaults(); } \
private: \
    friend int print_config_static_initializer(); \
    static void initialize_cache() \
    { \
        assert(! s_cache_##CLASS_NAME.initialized()); \
        if (! s_cache_##CLASS_NAME.initialized()) { \
            CLASS_NAME *inst = new CLASS_NAME(1); \
            inst->initialize(s_cache_##CLASS_NAME, (const char*)inst); \
            s_cache_##CLASS_NAME.finalize(inst, inst->def()); \
        } \
    } \
    /* 缓存对象，包含键/选项映射、选项键列表以及使用默认值初始化的此静态配置的副本。 */ \
    static StaticPrintConfig::StaticCache<CLASS_NAME> s_cache_##CLASS_NAME;

#define STATIC_PRINT_CONFIG_CACHE(CLASS_NAME) \
    STATIC_PRINT_CONFIG_CACHE_BASE(CLASS_NAME) \
public: \
    /* 公共默认构造函数将初始化键/选项缓存和默认对象副本（如果需要）。 */ \
    CLASS_NAME() { assert(s_cache_##CLASS_NAME.initialized()); *this = s_cache_##CLASS_NAME.defaults(); } \
protected: \
    /* 合成时要调用的受保护构造函数。 */ \
    CLASS_NAME(int) {}

#define STATIC_PRINT_CONFIG_CACHE_DERIVED(CLASS_NAME) \
    STATIC_PRINT_CONFIG_CACHE_BASE(CLASS_NAME) \
public: \
    /* 覆盖 ConfigBase::def()。静态配置定义。存储在此 ConfigBase 中的任何值都应有其定义。 */ \
    const ConfigDef*    def() const override { return &print_config_def; } \
    /* 处理遗留和废弃的配置键 */ \
    void                handle_legacy(t_config_option_key &opt_key, std::string &value) const override \
        { PrintConfigDef::handle_legacy(opt_key, value); }

#define PRINT_CONFIG_CLASS_ELEMENT_DEFINITION(r, data, elem) BOOST_PP_TUPLE_ELEM(0, elem) BOOST_PP_TUPLE_ELEM(1, elem);
#define PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION2(KEY) cache.opt_add(BOOST_PP_STRINGIZE(KEY), base_ptr, this->KEY);
#define PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION(r, data, elem) PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION2(BOOST_PP_TUPLE_ELEM(1, elem))
#define PRINT_CONFIG_CLASS_ELEMENT_HASH(r, data, elem) boost::hash_combine(seed, BOOST_PP_TUPLE_ELEM(1, elem).hash());
#define PRINT_CONFIG_CLASS_ELEMENT_EQUAL(r, data, elem) if (! (BOOST_PP_TUPLE_ELEM(1, elem) == rhs.BOOST_PP_TUPLE_ELEM(1, elem))) return false;
#define PRINT_CONFIG_CLASS_ELEMENT_LOWER(r, data, elem) \
        if (BOOST_PP_TUPLE_ELEM(1, elem) < rhs.BOOST_PP_TUPLE_ELEM(1, elem)) return true; \
        if (! (BOOST_PP_TUPLE_ELEM(1, elem) == rhs.BOOST_PP_TUPLE_ELEM(1, elem))) return false;

#define PRINT_CONFIG_CLASS_DEFINE(CLASS_NAME, PARAMETER_DEFINITION_SEQ) \
class CLASS_NAME : public StaticPrintConfig { \
    STATIC_PRINT_CONFIG_CACHE(CLASS_NAME) \
public: \
    BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_DEFINITION, _, PARAMETER_DEFINITION_SEQ) \
    size_t hash() const throw() \
    { \
        size_t seed = 0; \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_HASH, _, PARAMETER_DEFINITION_SEQ) \
        return seed; \
    } \
    bool operator==(const CLASS_NAME &rhs) const throw() \
    { \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_EQUAL, _, PARAMETER_DEFINITION_SEQ) \
        return true; \
    } \
    bool operator!=(const CLASS_NAME &rhs) const throw() { return ! (*this == rhs); } \
    bool operator<(const CLASS_NAME &rhs) const throw() \
    { \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_LOWER, _, PARAMETER_DEFINITION_SEQ) \
        return false; \
    } \
protected: \
    void initialize(StaticCacheBase &cache, const char *base_ptr) \
    { \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION, _, PARAMETER_DEFINITION_SEQ) \
    } \
};

#define PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST_ITEM(r, data, i, elem) BOOST_PP_COMMA_IF(i) public elem
#define PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST(CLASSES_PARENTS_TUPLE) BOOST_PP_SEQ_FOR_EACH_I(PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST_ITEM, _, BOOST_PP_TUPLE_TO_SEQ(CLASSES_PARENTS_TUPLE))
#define PRINT_CONFIG_CLASS_DERIVED_INITIALIZER_ITEM(r, VALUE, i, elem) BOOST_PP_COMMA_IF(i) elem(VALUE)
#define PRINT_CONFIG_CLASS_DERIVED_INITIALIZER(CLASSES_PARENTS_TUPLE, VALUE) BOOST_PP_SEQ_FOR_EACH_I(PRINT_CONFIG_CLASS_DERIVED_INITIALIZER_ITEM, VALUE, BOOST_PP_TUPLE_TO_SEQ(CLASSES_PARENTS_TUPLE))
#define PRINT_CONFIG_CLASS_DERIVED_INITCACHE_ITEM(r, data, elem) this->elem::initialize(cache, base_ptr);
#define PRINT_CONFIG_CLASS_DERIVED_INITCACHE(CLASSES_PARENTS_TUPLE) BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_DERIVED_INITCACHE_ITEM, _, BOOST_PP_TUPLE_TO_SEQ(CLASSES_PARENTS_TUPLE))
#define PRINT_CONFIG_CLASS_DERIVED_HASH(r, data, elem) boost::hash_combine(seed, static_cast<const elem*>(this)->hash());
#define PRINT_CONFIG_CLASS_DERIVED_EQUAL(r, data, elem) \
    if (! (*static_cast<const elem*>(this) == static_cast<const elem&>(rhs))) return false;

// 通用版本，带或不带新参数。不要直接使用。
#define PRINT_CONFIG_CLASS_DERIVED_DEFINE1(CLASS_NAME, CLASSES_PARENTS_TUPLE, PARAMETER_DEFINITION, PARAMETER_REGISTRATION, PARAMETER_HASHES, PARAMETER_EQUALS) \
class CLASS_NAME : PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST(CLASSES_PARENTS_TUPLE) { \
    STATIC_PRINT_CONFIG_CACHE_DERIVED(CLASS_NAME) \
    CLASS_NAME() : PRINT_CONFIG_CLASS_DERIVED_INITIALIZER(CLASSES_PARENTS_TUPLE, 0) { assert(s_cache_##CLASS_NAME.initialized()); *this = s_cache_##CLASS_NAME.defaults(); } \
public: \
    PARAMETER_DEFINITION \
    size_t hash() const throw() \
    { \
        size_t seed = 0; \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_DERIVED_HASH, _, BOOST_PP_TUPLE_TO_SEQ(CLASSES_PARENTS_TUPLE)) \
        PARAMETER_HASHES \
        return seed; \
    } \
    bool operator==(const CLASS_NAME &rhs) const throw() \
    { \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_DERIVED_EQUAL, _, BOOST_PP_TUPLE_TO_SEQ(CLASSES_PARENTS_TUPLE)) \
        PARAMETER_EQUALS \
        return true; \
    } \
    bool operator!=(const CLASS_NAME &rhs) const throw() { return ! (*this == rhs); } \
protected: \
    CLASS_NAME(int) : PRINT_CONFIG_CLASS_DERIVED_INITIALIZER(CLASSES_PARENTS_TUPLE, 1) {} \
    void initialize(StaticCacheBase &cache, const char* base_ptr) { \
        PRINT_CONFIG_CLASS_DERIVED_INITCACHE(CLASSES_PARENTS_TUPLE) \
        PARAMETER_REGISTRATION \
    } \
};
// 不添加新参数的变体。
#define PRINT_CONFIG_CLASS_DERIVED_DEFINE0(CLASS_NAME, CLASSES_PARENTS_TUPLE) \
    PRINT_CONFIG_CLASS_DERIVED_DEFINE1(CLASS_NAME, CLASSES_PARENTS_TUPLE, BOOST_PP_EMPTY(), BOOST_PP_EMPTY(), BOOST_PP_EMPTY(), BOOST_PP_EMPTY())
// 添加新参数的变体。
#define PRINT_CONFIG_CLASS_DERIVED_DEFINE(CLASS_NAME, CLASSES_PARENTS_TUPLE, PARAMETER_DEFINITION_SEQ) \
    PRINT_CONFIG_CLASS_DERIVED_DEFINE1(CLASS_NAME, CLASSES_PARENTS_TUPLE, \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_DEFINITION, _, PARAMETER_DEFINITION_SEQ), \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION, _, PARAMETER_DEFINITION_SEQ), \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_HASH, _, PARAMETER_DEFINITION_SEQ), \
        BOOST_PP_SEQ_FOR_EACH(PRINT_CONFIG_CLASS_ELEMENT_EQUAL, _, PARAMETER_DEFINITION_SEQ))

// 此对象映射到 Perl 作为 Slic3r::Config::PrintObject。
PRINT_CONFIG_CLASS_DEFINE(
    PrintObjectConfig,

    ((ConfigOptionFloat,               brim_object_gap))
    ((ConfigOptionEnum<BrimType>,      brim_type))
    ((ConfigOptionFloat,               brim_width))
    ((ConfigOptionFloat,               brim_ears_detection_length))
    ((ConfigOptionFloat,               brim_ears_max_angle))
    ((ConfigOptionFloat,               skirt_start_angle))
    ((ConfigOptionBool,                bridge_no_support))
    ((ConfigOptionFloat,               elefant_foot_compensation))
    ((ConfigOptionInt,                 elefant_foot_compensation_layers))
    ((ConfigOptionFloat,               max_bridge_length))
    ((ConfigOptionFloatOrPercent,      line_width))
    // 强制在相邻材料/体积之间生成实体外壳。
    ((ConfigOptionBool,                interface_shells))
    ((ConfigOptionFloat,               layer_height))
    ((ConfigOptionFloat,               mmu_segmented_region_max_width))
    ((ConfigOptionFloat,               mmu_segmented_region_interlocking_depth))
    ((ConfigOptionFloat,               raft_contact_distance))
    ((ConfigOptionFloat,               raft_expansion))
    ((ConfigOptionPercent,             raft_first_layer_density))
    ((ConfigOptionFloat,               raft_first_layer_expansion))
    ((ConfigOptionInt,                 raft_layers))
    ((ConfigOptionEnum<SeamPosition>,  seam_position))
    ((ConfigOptionBool,                staggered_inner_seams))
    ((ConfigOptionFloat,               slice_closing_radius))
    ((ConfigOptionEnum<SlicingMode>,   slicing_mode))
    ((ConfigOptionBool,                enable_support))
    // 自动支撑（基于 support_threshold_angle 生成）。
    ((ConfigOptionEnum<SupportType>,   support_type))
    // 支撑图案方向（在 XY 平面中）。`
    ((ConfigOptionFloat,               support_angle))
    ((ConfigOptionBool,                support_on_build_plate_only))
    ((ConfigOptionBool,                support_critical_regions_only))
    ((ConfigOptionBool,                support_remove_small_overhang))
    ((ConfigOptionFloat,               support_top_z_distance))
    ((ConfigOptionFloat,               support_bottom_z_distance))
    ((ConfigOptionInt,                 enforce_support_layers))
    ((ConfigOptionInt,                 support_filament))
    ((ConfigOptionFloatOrPercent,      support_line_width))
    ((ConfigOptionBool,                support_interface_not_for_body))
    ((ConfigOptionBool,                support_interface_loop_pattern))
    ((ConfigOptionInt,                 support_interface_filament))
    ((ConfigOptionInt,                 support_interface_top_layers))
    ((ConfigOptionInt,                 support_interface_bottom_layers))
    // 接口线之间的间距（填充线距离）。设为零以获得实体接口。
    ((ConfigOptionFloat,               support_interface_spacing))
    ((ConfigOptionFloat,               support_interface_speed))
    ((ConfigOptionEnum<SupportMaterialPattern>, support_base_pattern))
    ((ConfigOptionEnum<SupportMaterialInterfacePattern>, support_interface_pattern))
    // 支撑材料线之间的间距（填充线距离）。
    ((ConfigOptionFloat,               support_base_pattern_spacing))
    ((ConfigOptionFloat,               support_expansion))
    ((ConfigOptionFloat,               support_speed))
    ((ConfigOptionEnum<SupportMaterialStyle>, support_style))
    // BBS
    //((ConfigOptionBool,                independent_support_layer_height))
    // Orca 内部厚桥接
    ((ConfigOptionBool,                thick_bridges))
    ((ConfigOptionBool,                thick_internal_bridges))
    ((ConfigOptionEnum<InternalBridgeFilter>,  dont_filter_internal_bridges))
    // Orca
    ((ConfigOptionEnum<EnableExtraBridgeLayer>,  enable_extra_bridge_layer))
    ((ConfigOptionPercent,              internal_bridge_density))
    // 悬垂角度阈值。
    ((ConfigOptionInt,                 support_threshold_angle))
    ((ConfigOptionFloatOrPercent,      support_threshold_overlap))
    ((ConfigOptionFloat,               support_object_xy_distance))
    ((ConfigOptionFloat,               support_object_first_layer_gap))
    ((ConfigOptionBool,                support_ironing))
    ((ConfigOptionEnum<InfillPattern>, support_ironing_pattern))
    ((ConfigOptionPercent,             support_ironing_flow))
    ((ConfigOptionFloat,               support_ironing_spacing))
    ((ConfigOptionFloat,               xy_hole_compensation))
    ((ConfigOptionFloat,               xy_contour_compensation))
    ((ConfigOptionBool,                flush_into_objects))
    // BBS
    ((ConfigOptionBool,                flush_into_infill))
    ((ConfigOptionBool,                flush_into_support))
    // BBS
    ((ConfigOptionFloat,              tree_support_branch_distance))
    ((ConfigOptionFloat,              tree_support_tip_diameter))
    ((ConfigOptionFloat,              tree_support_branch_diameter))
    ((ConfigOptionFloat,              tree_support_branch_angle))
    ((ConfigOptionFloat,              tree_support_branch_diameter_angle))
    ((ConfigOptionFloat,              tree_support_angle_slow))
    ((ConfigOptionInt,                tree_support_wall_count))
    ((ConfigOptionBool,               tree_support_adaptive_layer_height))
    ((ConfigOptionBool,               tree_support_auto_brim))
    ((ConfigOptionFloat,              tree_support_brim_width))
    ((ConfigOptionBool,               detect_narrow_internal_solid_infill))
    // ((ConfigOptionBool,               adaptive_layer_height))
    ((ConfigOptionFloat,              support_bottom_interface_spacing))
    ((ConfigOptionEnum<PerimeterGeneratorType>, wall_generator))
    ((ConfigOptionPercent,            wall_transition_length))
    ((ConfigOptionPercent,            wall_transition_filter_deviation))
    ((ConfigOptionFloat,              wall_transition_angle))
    ((ConfigOptionInt,                wall_distribution_count))
    ((ConfigOptionPercent,            min_feature_size))
    ((ConfigOptionPercent,            initial_layer_min_bead_width))
    ((ConfigOptionPercent,            min_bead_width))

    // Orca
    ((ConfigOptionFloat,              make_overhang_printable_angle))
    ((ConfigOptionFloat,              make_overhang_printable_hole_size))
    ((ConfigOptionFloat,              tree_support_branch_distance_organic))
    ((ConfigOptionPercent,            tree_support_top_rate))
    ((ConfigOptionFloat,              tree_support_branch_diameter_organic))
    ((ConfigOptionFloat,              tree_support_branch_angle_organic))
    ((ConfigOptionEnum<GapFillTarget>,gap_fill_target))
    ((ConfigOptionFloat,              min_length_factor))

    // 将所有加速度和加加速度设置移至物体
    ((ConfigOptionFloat,              default_acceleration))
    ((ConfigOptionFloat,              outer_wall_acceleration))
    ((ConfigOptionFloat,              inner_wall_acceleration))
    ((ConfigOptionFloat,              top_surface_acceleration))
    ((ConfigOptionFloat,              initial_layer_acceleration))
    ((ConfigOptionFloatOrPercent,     bridge_acceleration))
    ((ConfigOptionFloat,              travel_acceleration))
    ((ConfigOptionFloatOrPercent,     sparse_infill_acceleration))
    ((ConfigOptionFloatOrPercent,     internal_solid_infill_acceleration))

    ((ConfigOptionFloat,              default_jerk))
    ((ConfigOptionFloat,              outer_wall_jerk))
    ((ConfigOptionFloat,              inner_wall_jerk))
    ((ConfigOptionFloat,              infill_jerk))
    ((ConfigOptionFloat,              top_surface_jerk))
    ((ConfigOptionFloat,              initial_layer_jerk))
    ((ConfigOptionFloat,              travel_jerk))
    ((ConfigOptionBool,               precise_z_height))
    ((ConfigOptionFloat,              default_junction_deviation))
        
    ((ConfigOptionBool, interlocking_beam))
    ((ConfigOptionFloat,interlocking_beam_width))
    ((ConfigOptionFloat,interlocking_orientation))
    ((ConfigOptionInt,  interlocking_beam_layer_count))
    ((ConfigOptionInt,  interlocking_depth))
    ((ConfigOptionInt,  interlocking_boundary_avoidance))

    // Orca: 仅内部使用
    ((ConfigOptionBool,  calib_flowrate_topinfill_special_order)) // ORCA：流量校准的特殊标志


)

// This object is mapped to Perl as Slic3r::Config::PrintRegion.
PRINT_CONFIG_CLASS_DEFINE(
    PrintRegionConfig,

    ((ConfigOptionInt,                  bottom_shell_layers))
    ((ConfigOptionFloat,                bottom_shell_thickness))
    ((ConfigOptionFloat,                bridge_angle))
    ((ConfigOptionFloat,                internal_bridge_angle)) // ORCA: Internal bridge angle override
    ((ConfigOptionFloat,                bridge_flow))
    ((ConfigOptionFloat,                internal_bridge_flow))
    ((ConfigOptionFloat,                bridge_speed))
    ((ConfigOptionFloatOrPercent,       internal_bridge_speed))
    ((ConfigOptionEnum<EnsureVerticalShellThickness>,   ensure_vertical_shell_thickness))
    ((ConfigOptionPercent,              top_surface_density))
    ((ConfigOptionPercent,               bottom_surface_density))
    ((ConfigOptionEnum<InfillPattern>,  top_surface_pattern))
    ((ConfigOptionEnum<InfillPattern>,  bottom_surface_pattern))
    ((ConfigOptionEnum<InfillPattern>, internal_solid_infill_pattern))
    ((ConfigOptionFloatOrPercent,       outer_wall_line_width))
    ((ConfigOptionFloat,                outer_wall_speed))
    ((ConfigOptionFloat,                infill_direction))
    ((ConfigOptionFloat,                solid_infill_direction))
    ((ConfigOptionString,               solid_infill_rotate_template))
    ((ConfigOptionBool,                 symmetric_infill_y_axis))
    ((ConfigOptionFloat,                infill_shift_step))
    ((ConfigOptionString,               sparse_infill_rotate_template))
    ((ConfigOptionPercent,              sparse_infill_density))
    ((ConfigOptionEnum<InfillPattern>,  sparse_infill_pattern))
    ((ConfigOptionFloat,                lateral_lattice_angle_1))
    ((ConfigOptionFloat,                lateral_lattice_angle_2))
    ((ConfigOptionFloat,                infill_overhang_angle))
    ((ConfigOptionBool,                 align_infill_direction_to_model))
    ((ConfigOptionString,               extra_solid_infills))
    ((ConfigOptionEnum<FuzzySkinType>,  fuzzy_skin))
    ((ConfigOptionFloat,                fuzzy_skin_thickness))
    ((ConfigOptionFloat,                fuzzy_skin_point_distance))
    ((ConfigOptionBool,                 fuzzy_skin_first_layer))
    ((ConfigOptionEnum<NoiseType>,      fuzzy_skin_noise_type))
    ((ConfigOptionEnum<FuzzySkinMode>,  fuzzy_skin_mode))
    ((ConfigOptionFloat,                fuzzy_skin_scale))
    ((ConfigOptionInt,                  fuzzy_skin_octaves))
    ((ConfigOptionFloat,                fuzzy_skin_persistence))
    ((ConfigOptionFloat,                gap_infill_speed))
    ((ConfigOptionInt,                  sparse_infill_filament))
    ((ConfigOptionFloatOrPercent,       sparse_infill_line_width))
    ((ConfigOptionPercent,              infill_wall_overlap))
    ((ConfigOptionPercent,              top_bottom_infill_wall_overlap))
    ((ConfigOptionFloat,                sparse_infill_speed))
    ((ConfigOptionPercent, skeleton_infill_density))
    ((ConfigOptionPercent, skin_infill_density))
    ((ConfigOptionFloat, infill_lock_depth))
    ((ConfigOptionFloat, skin_infill_depth))
    ((ConfigOptionFloatOrPercent, skin_infill_line_width))
    ((ConfigOptionFloatOrPercent, skeleton_infill_line_width))
    ((ConfigOptionBool, infill_combination))
    // Orca:
    ((ConfigOptionFloatOrPercent,                infill_combination_max_layer_height))
    ((ConfigOptionInt,                  fill_multiline))
    // 熨烫选项
    ((ConfigOptionEnum<IroningType>, ironing_type))
    ((ConfigOptionEnum<InfillPattern>, ironing_pattern))
    ((ConfigOptionPercent, ironing_flow))
    ((ConfigOptionFloat, ironing_spacing))
    ((ConfigOptionFloat, ironing_inset))
    ((ConfigOptionFloat, ironing_direction))
    ((ConfigOptionFloat, ironing_speed))
    ((ConfigOptionFloat, ironing_angle))
    // 检测桥接周长
    ((ConfigOptionBool, detect_overhang_wall))
    ((ConfigOptionInt, wall_filament))
    ((ConfigOptionFloatOrPercent, inner_wall_line_width))
    ((ConfigOptionFloat, inner_wall_speed))
    // 周长总数。
    ((ConfigOptionInt, wall_loops))
    ((ConfigOptionBool, alternate_extra_wall))
    ((ConfigOptionFloat, minimum_sparse_infill_area))
    ((ConfigOptionInt, solid_infill_filament))
    ((ConfigOptionFloatOrPercent, internal_solid_infill_line_width))
    ((ConfigOptionFloat, internal_solid_infill_speed))
    // 检测薄壁。
    ((ConfigOptionBool, detect_thin_wall))
    ((ConfigOptionFloatOrPercent, top_surface_line_width))
    ((ConfigOptionInt, top_shell_layers))
    ((ConfigOptionFloat, top_shell_thickness))
    ((ConfigOptionFloat, top_surface_speed))
    //BBS
    ((ConfigOptionBool,                 enable_overhang_speed))
    ((ConfigOptionFloatOrPercent,       overhang_1_4_speed))
    ((ConfigOptionFloatOrPercent,       overhang_2_4_speed))
    ((ConfigOptionFloatOrPercent,       overhang_3_4_speed))
    ((ConfigOptionFloatOrPercent,       overhang_4_4_speed))
    ((ConfigOptionBool,                 only_one_wall_top))

    //SoftFever
    ((ConfigOptionFloatOrPercent,       min_width_top_surface))
    ((ConfigOptionBool,                 only_one_wall_first_layer))
    ((ConfigOptionFloat,                print_flow_ratio))
    ((ConfigOptionFloatOrPercent,       seam_gap))
    ((ConfigOptionBool,                 role_based_wipe_speed))
    ((ConfigOptionFloatOrPercent,       wipe_speed))
    ((ConfigOptionBool,                 wipe_on_loops))
    ((ConfigOptionBool,                 wipe_before_external_loop))
    ((ConfigOptionEnum<WallInfillOrder>, wall_infill_order))
    ((ConfigOptionBool,                 precise_outer_wall))
    ((ConfigOptionPercent,              bridge_density))
    ((ConfigOptionFloat,                 filter_out_gap_fill))
    ((ConfigOptionFloatOrPercent,       small_perimeter_speed))
    ((ConfigOptionFloat,                small_perimeter_threshold))
    ((ConfigOptionFloat,                top_solid_infill_flow_ratio))
    ((ConfigOptionFloat,                bottom_solid_infill_flow_ratio))
    ((ConfigOptionFloatOrPercent,       infill_anchor))
    ((ConfigOptionFloatOrPercent,       infill_anchor_max))

    // Orca
    ((ConfigOptionBool,                 make_overhang_printable))
    ((ConfigOptionBool,                 extra_perimeters_on_overhangs))
    ((ConfigOptionBool,                 slowdown_for_curled_perimeters))
    ((ConfigOptionBool,                 hole_to_polyhole))
    ((ConfigOptionFloatOrPercent,       hole_to_polyhole_threshold))
    ((ConfigOptionBool,                 hole_to_polyhole_twisted))
    ((ConfigOptionBool,                 overhang_reverse))
    ((ConfigOptionBool,                 overhang_reverse_internal_only))
    ((ConfigOptionFloatOrPercent,       overhang_reverse_threshold))
    ((ConfigOptionEnum<CounterboreHoleBridgingOption>, counterbore_hole_bridging))
    ((ConfigOptionEnum<WallSequence>,  wall_sequence))
    ((ConfigOptionBool,                is_infill_first))
    ((ConfigOptionBool,                small_area_infill_flow_compensation))
    ((ConfigOptionEnum<WallDirection>,  wall_direction))

    // Orca: seam slopes
    ((ConfigOptionEnum<SeamScarfType>,  seam_slope_type))
    ((ConfigOptionBool,                 seam_slope_conditional))
    ((ConfigOptionInt,                  scarf_angle_threshold))
    ((ConfigOptionFloatOrPercent,       seam_slope_start_height))
    ((ConfigOptionBool,                 seam_slope_entire_loop))
    ((ConfigOptionFloat,                seam_slope_min_length))
    ((ConfigOptionInt,                  seam_slope_steps))
    ((ConfigOptionBool,                 seam_slope_inner_walls))
    ((ConfigOptionFloatOrPercent,       scarf_joint_speed))
    ((ConfigOptionFloat,                scarf_joint_flow_ratio))
    ((ConfigOptionPercent,              scarf_overhang_threshold))
)

PRINT_CONFIG_CLASS_DEFINE(
    MachineEnvelopeConfig,

    // Orca: 是否将机器限制输出到 G-code 的开头。
    ((ConfigOptionBool,                 emit_machine_limits_to_gcode))
    // M201 X... Y... Z... E... [mm/sec^2]
    ((ConfigOptionFloats,               machine_max_acceleration_x))
    ((ConfigOptionFloats,               machine_max_acceleration_y))
    ((ConfigOptionFloats,               machine_max_acceleration_z))
    ((ConfigOptionFloats,               machine_max_acceleration_e))
    // M203 X... Y... Z... E... [mm/sec]
    ((ConfigOptionFloats,               machine_max_speed_x))
    ((ConfigOptionFloats,               machine_max_speed_y))
    ((ConfigOptionFloats,               machine_max_speed_z))
    ((ConfigOptionFloats,               machine_max_speed_e))

    // M204 P... R... T...[mm/sec^2]
    ((ConfigOptionFloats,               machine_max_acceleration_extruding))
    ((ConfigOptionFloats,               machine_max_acceleration_retracting))
    ((ConfigOptionFloats,               machine_max_acceleration_travel))

    // M205 X... Y... Z... E... [mm/sec]
    ((ConfigOptionFloats,               machine_max_jerk_x))
    ((ConfigOptionFloats,               machine_max_jerk_y))
    ((ConfigOptionFloats,               machine_max_jerk_z))
    ((ConfigOptionFloats,               machine_max_jerk_e))
    // M205 J... [mm]
    ((ConfigOptionFloats,               machine_max_junction_deviation))
    // M205 T... [mm/sec]
    ((ConfigOptionFloats,               machine_min_travel_rate))
    // M205 S... [mm/sec]
    ((ConfigOptionFloats,               machine_min_extruding_rate))

    // 共振避让，从 qidi slicer 移植
    ((ConfigOptionBool,                 resonance_avoidance))
    ((ConfigOptionFloat,                min_resonance_avoidance_speed))
    ((ConfigOptionFloat,                max_resonance_avoidance_speed))
)

// This object is mapped to Perl as Slic3r::Config::GCode.
PRINT_CONFIG_CLASS_DEFINE(
    GCodeConfig,

    ((ConfigOptionString,              before_layer_change_gcode)) 
    ((ConfigOptionString,              printing_by_object_gcode)) 
    ((ConfigOptionFloats,              deretraction_speed))
    //BBS
    ((ConfigOptionBool,                enable_arc_fitting))
    ((ConfigOptionString,              machine_end_gcode))
    ((ConfigOptionStrings,             filament_end_gcode))
    ((ConfigOptionFloats,              filament_flow_ratio))
    ((ConfigOptionBools,               enable_pressure_advance))
    ((ConfigOptionFloats,              pressure_advance))
    // Orca: 自适应压力提前和校准模型
    ((ConfigOptionBools,                adaptive_pressure_advance))
    ((ConfigOptionBools,                adaptive_pressure_advance_overhangs))
    ((ConfigOptionStrings,             adaptive_pressure_advance_model))
    ((ConfigOptionFloats,              adaptive_pressure_advance_bridges))
    //
    ((ConfigOptionFloat,               fan_kickstart))
    ((ConfigOptionBool,                fan_speedup_overhangs))
    ((ConfigOptionFloat,               fan_speedup_time))
    ((ConfigOptionFloats,              filament_diameter))
    ((ConfigOptionFloats,              filament_density))
    ((ConfigOptionStrings,             filament_type))
    ((ConfigOptionBools,               filament_soluble))
    ((ConfigOptionBools,               filament_is_support))
    ((ConfigOptionFloats,              filament_cost))
    ((ConfigOptionStrings,             default_filament_colour))
    ((ConfigOptionInts,                temperature_vitrification))  //BBS
    ((ConfigOptionBools,               filament_is_high_temperature))
    ((ConfigOptionFloats,              filament_max_volumetric_speed))
    ((ConfigOptionInts,                required_nozzle_HRC))
    // BBS
    ((ConfigOptionBool,                scan_first_layer))
    ((ConfigOptionPoints,              thumbnail_size))
    // ((ConfigOptionBool,                spaghetti_detector))
    ((ConfigOptionBool,                gcode_add_line_number))
    ((ConfigOptionBool,                bbl_bed_temperature_gcode))
    ((ConfigOptionEnum<GCodeFlavor>,   gcode_flavor))

    ((ConfigOptionFloat,               time_cost)) 
    ((ConfigOptionString,              layer_change_gcode))
    ((ConfigOptionString,              time_lapse_gcode))

    ((ConfigOptionFloat,               max_volumetric_extrusion_rate_slope))
    ((ConfigOptionFloat,               max_volumetric_extrusion_rate_slope_segment_length))
    ((ConfigOptionBool,               extrusion_rate_smoothing_external_perimeter_only))

    
    ((ConfigOptionPercents,            retract_before_wipe))
    ((ConfigOptionFloats,              retraction_length))
    ((ConfigOptionFloats,              retract_length_toolchange))
    ((ConfigOptionInt,                 enable_long_retraction_when_cut))
    ((ConfigOptionFloats,              retraction_distances_when_cut))
    ((ConfigOptionBools,               long_retractions_when_cut))
    ((ConfigOptionFloats,              z_hop))
    // BBS
    ((ConfigOptionBools,               z_hop_when_prime))
    ((ConfigOptionEnumsGeneric,        z_hop_types))
    ((ConfigOptionFloats,              travel_slope))
    ((ConfigOptionFloats,              retract_lift_above))
    ((ConfigOptionFloats,              retract_lift_below))
    ((ConfigOptionEnumsGeneric,        retract_lift_enforce))
    ((ConfigOptionFloats,              retract_restart_extra))
    ((ConfigOptionFloats,              retract_restart_extra_toolchange))
    ((ConfigOptionFloats,              retraction_speed))
    ((ConfigOptionString,              machine_start_gcode))
    ((ConfigOptionStrings,             filament_start_gcode))
    ((ConfigOptionBool,                single_extruder_multi_material))
    ((ConfigOptionBool,                manual_filament_change))
    ((ConfigOptionBool,                single_extruder_multi_material_priming))
    ((ConfigOptionBool,                wipe_tower_no_sparse_layers))
    ((ConfigOptionString,              change_filament_gcode))
    ((ConfigOptionString,              change_extrusion_role_gcode))
    ((ConfigOptionFloat,               travel_speed))
    ((ConfigOptionFloat,               travel_speed_z))
    ((ConfigOptionBool,                silent_mode))
    ((ConfigOptionString,              machine_pause_gcode))
    ((ConfigOptionString,              template_custom_gcode))
    //BBS
    ((ConfigOptionEnum<NozzleType>,    nozzle_type))
    ((ConfigOptionInt,                 nozzle_hrc))
    ((ConfigOptionBool,                auxiliary_fan))
    ((ConfigOptionBool,                support_air_filtration))
    ((ConfigOptionEnum<PrinterStructure>,printer_structure))
    ((ConfigOptionBool,                support_chamber_temp_control))


    // SoftFever
    ((ConfigOptionBool,                use_firmware_retraction))
    ((ConfigOptionBool,                use_relative_e_distances))
    ((ConfigOptionBool,                accel_to_decel_enable))
    ((ConfigOptionPercent,             accel_to_decel_factor))
    ((ConfigOptionFloatOrPercent,      initial_layer_travel_speed))
    ((ConfigOptionBool,                bbl_calib_mark_logo))
    ((ConfigOptionBool,                disable_m73))

    // Orca: mmu
    ((ConfigOptionFloat,               cooling_tube_retraction))
    ((ConfigOptionFloat,               cooling_tube_length))
    ((ConfigOptionBool,                high_current_on_filament_swap))
    ((ConfigOptionFloat,               parking_pos_retraction))
    ((ConfigOptionFloat,               extra_loading_move))
    ((ConfigOptionFloat,               machine_load_filament_time))
    ((ConfigOptionFloat,               machine_tool_change_time))
    ((ConfigOptionBool,                tool_change_temprature_wait))
    ((ConfigOptionFloat,               machine_unload_filament_time))
    ((ConfigOptionFloats,              filament_loading_speed))
    ((ConfigOptionFloats,              filament_loading_speed_start))
    ((ConfigOptionFloats,              filament_unloading_speed))
    ((ConfigOptionFloats,              filament_unloading_speed_start))
    ((ConfigOptionFloats,              filament_toolchange_delay))
    ((ConfigOptionInts,                filament_cooling_moves))
    ((ConfigOptionFloats,              filament_cooling_initial_speed))
    ((ConfigOptionFloats,              filament_minimal_purge_on_wipe_tower))
    ((ConfigOptionFloats,              filament_cooling_final_speed))
    ((ConfigOptionStrings,             filament_ramming_parameters))
    ((ConfigOptionBools,               filament_multitool_ramming))
    ((ConfigOptionFloats,              filament_multitool_ramming_volume))
    ((ConfigOptionFloats,              filament_multitool_ramming_flow))
    ((ConfigOptionFloats,              filament_stamping_loading_speed))
    ((ConfigOptionFloats,              filament_stamping_distance))
    ((ConfigOptionBool,                purge_in_prime_tower))
    ((ConfigOptionBool,                enable_filament_ramming))
    ((ConfigOptionFloat,                ramming_line_width_ratio))
    ((ConfigOptionBool,                enable_change_pressure_when_wiping))
    ((ConfigOptionFloat,                ramming_pressure_advance_value))
    ((ConfigOptionBool,                support_multi_bed_types))

    // 小面积填充流量补偿
    ((ConfigOptionStrings,              small_area_infill_flow_compensation_model))

    ((ConfigOptionBool,                has_scarf_joint_seam))
)

// This object is mapped to Perl as Slic3r::Config::Print.
PRINT_CONFIG_CLASS_DERIVED_DEFINE(
    PrintConfig,
    (MachineEnvelopeConfig, GCodeConfig),

    //BBS
    ((ConfigOptionInts,               additional_cooling_fan_speed))
    ((ConfigOptionBool,               reduce_crossing_wall))
    ((ConfigOptionFloatOrPercent,     max_travel_detour_distance))
    ((ConfigOptionPoints,             printable_area))
    //BBS: add bed_exclude_area
    ((ConfigOptionPoints,             bed_exclude_area))
    ((ConfigOptionPoints,             head_wrap_detect_zone))
    // BBS
    ((ConfigOptionString,             bed_custom_texture))
    ((ConfigOptionString,             bed_custom_model))
    ((ConfigOptionEnum<BedType>,      curr_bed_type))
    ((ConfigOptionInts,               cool_plate_temp))
    ((ConfigOptionInts,               textured_cool_plate_temp))
    ((ConfigOptionInts,               supertack_plate_temp))
    ((ConfigOptionInts,               eng_plate_temp))
    ((ConfigOptionInts,               hot_plate_temp)) // hot is short for high temperature
    ((ConfigOptionInts,               textured_plate_temp))
    ((ConfigOptionInts,               graphic_effect_plate_temp))
    ((ConfigOptionInts,               supertack_plate_temp_initial_layer))
    ((ConfigOptionInts,               cool_plate_temp_initial_layer))
    ((ConfigOptionInts,               textured_cool_plate_temp_initial_layer))
    ((ConfigOptionInts,               eng_plate_temp_initial_layer))
    ((ConfigOptionInts,               hot_plate_temp_initial_layer)) // hot is short for high temperature
    ((ConfigOptionInts,               textured_plate_temp_initial_layer))
    ((ConfigOptionInts,               graphic_effect_plate_temp_initial_layer))
    ((ConfigOptionBools,              enable_overhang_bridge_fan))
    ((ConfigOptionInts,               overhang_fan_speed))
    ((ConfigOptionEnumsGeneric,       overhang_fan_threshold))
    ((ConfigOptionEnum<PrintSequence>,print_sequence))
    ((ConfigOptionEnum<PrintOrder>,   print_order))
    ((ConfigOptionInts,               first_layer_print_sequence))
    ((ConfigOptionInts,               other_layers_print_sequence))
    ((ConfigOptionInt,                other_layers_print_sequence_nums))
    ((ConfigOptionBools,              slow_down_for_layer_cooling))
    ((ConfigOptionInts,               close_fan_the_first_x_layers))
    ((ConfigOptionEnum<DraftShield>,  draft_shield))
    ((ConfigOptionFloat,              extruder_clearance_height_to_rod))//BBs
    ((ConfigOptionFloat,              extruder_clearance_height_to_lid))//BBS
    ((ConfigOptionFloat,              extruder_clearance_radius))
    ((ConfigOptionFloat,              nozzle_height))
    ((ConfigOptionStrings,            extruder_colour))
    ((ConfigOptionPoints,             extruder_offset))
    ((ConfigOptionBools,              reduce_fan_stop_start_freq))
    ((ConfigOptionBools,              dont_slow_down_outer_wall))
    ((ConfigOptionFloats,             fan_cooling_layer_time))
    ((ConfigOptionStrings,            filament_colour))
    ((ConfigOptionStrings,            filament_multi_colors))
    ((ConfigOptionInts,               filament_colour_mode))
    ((ConfigOptionBools,              activate_air_filtration))
    ((ConfigOptionInts,               during_print_exhaust_fan_speed))
    ((ConfigOptionInts,               complete_print_exhaust_fan_speed))
    ((ConfigOptionFloatOrPercent,     initial_layer_line_width))
    ((ConfigOptionFloat,              initial_layer_print_height))
    ((ConfigOptionFloat,              initial_layer_speed))

    //BBS
    ((ConfigOptionFloat,              initial_layer_infill_speed))
    ((ConfigOptionInts,               nozzle_temperature_initial_layer))
    ((ConfigOptionInts,               full_fan_speed_layer))
    ((ConfigOptionFloats,               fan_max_speed))
    ((ConfigOptionFloats,             max_layer_height))
    ((ConfigOptionFloats,               fan_min_speed))
    ((ConfigOptionFloats,             min_layer_height))
    ((ConfigOptionFloat,              printable_height))
    ((ConfigOptionPoint,              best_object_pos))
    ((ConfigOptionFloats,             slow_down_min_speed))
    ((ConfigOptionFloats,             nozzle_diameter))
    ((ConfigOptionBool,               reduce_infill_retraction))
    ((ConfigOptionBool,               ooze_prevention))
    ((ConfigOptionString,             filename_format))
    ((ConfigOptionStrings,            post_process))
    ((ConfigOptionFloat,              mixed_color_layer_height_a))
    ((ConfigOptionFloat,              mixed_color_layer_height_b))
    ((ConfigOptionBool,               mixed_filament_gradient_mode))
    ((ConfigOptionFloat,              mixed_filament_height_lower_bound))
    ((ConfigOptionFloat,              mixed_filament_height_upper_bound))
    ((ConfigOptionBool,               mixed_filament_advanced_dithering))
    ((ConfigOptionFloat,              mixed_filament_pointillism_pixel_size))
    ((ConfigOptionFloat,              mixed_filament_pointillism_line_gap))
    ((ConfigOptionBool,               mixed_filament_component_bias_enabled))
    ((ConfigOptionFloat,              mixed_filament_surface_indentation))
    ((ConfigOptionBool,               mixed_filament_region_collapse))
    ((ConfigOptionString,             mixed_filament_definitions))
    ((ConfigOptionFloat,              dithering_z_step_size))
    ((ConfigOptionBool,               dithering_local_z_mode))
    ((ConfigOptionBool,               dithering_local_z_whole_objects))
    ((ConfigOptionBool,               dithering_local_z_infill))
    ((ConfigOptionBool,               dithering_local_z_direct_multicolor))
    ((ConfigOptionBool,               dithering_step_painted_zones_only))
    ((ConfigOptionString,             printer_model))
    ((ConfigOptionFloat,              resolution))
    ((ConfigOptionFloats,             retraction_minimum_travel))
    ((ConfigOptionBools,              retract_when_changing_layer))
    ((ConfigOptionFloat,              skirt_distance))
    ((ConfigOptionInt,                skirt_height))
    ((ConfigOptionInt,                skirt_loops))
    ((ConfigOptionEnum<SkirtType>,    skirt_type))
    ((ConfigOptionFloat,              skirt_speed))
    ((ConfigOptionBool,               single_loop_draft_shield))
    ((ConfigOptionFloat,              min_skirt_length))
    ((ConfigOptionFloats,             slow_down_layer_time))
    ((ConfigOptionBool,               spiral_mode))
    ((ConfigOptionBool,               spiral_mode_smooth))
    ((ConfigOptionFloatOrPercent,     spiral_mode_max_xy_smoothing))
    ((ConfigOptionFloat,              spiral_finishing_flow_ratio))
    ((ConfigOptionFloat,              spiral_starting_flow_ratio))
    ((ConfigOptionInt,                standby_temperature_delta))
    ((ConfigOptionFloat,                preheat_time))
    ((ConfigOptionInt,                delta_temperature))
    ((ConfigOptionInt,                preheat_steps))
    ((ConfigOptionInts,               nozzle_temperature))
    ((ConfigOptionBools,              wipe))
    // BBS
    ((ConfigOptionInts,               nozzle_temperature_range_low))
    ((ConfigOptionInts,               nozzle_temperature_range_high))
    ((ConfigOptionFloats,             wipe_distance))
    ((ConfigOptionBool,               enable_prime_tower))
    // BBS: change wipe_tower_x and wipe_tower_y data type to floats to add partplate logic
    ((ConfigOptionFloats,             wipe_tower_x))
    ((ConfigOptionFloats,             wipe_tower_y))
    ((ConfigOptionFloat,              prime_tower_width))
    ((ConfigOptionFloat,              wipe_tower_per_color_wipe))
    ((ConfigOptionFloat,              wipe_tower_rotation_angle))
    ((ConfigOptionFloat,              prime_tower_brim_width))
    ((ConfigOptionBool,               prime_tower_brim_chamfer))
    ((ConfigOptionFloat,              prime_tower_brim_chamfer_max_width))
    ((ConfigOptionFloat,              wipe_tower_bridging))
    ((ConfigOptionPercent,            wipe_tower_extra_flow))
    ((ConfigOptionFloat,              local_z_wipe_tower_purge_lines))
    ((ConfigOptionFloats,             flush_volumes_matrix))
    ((ConfigOptionFloats,             flush_volumes_vector))

    // Orca: mmu support
    ((ConfigOptionFloat,              wipe_tower_cone_angle))
    ((ConfigOptionPercent,            wipe_tower_extra_spacing))
    ((ConfigOptionFloat,              wipe_tower_max_purge_speed))
    ((ConfigOptionEnum<WipeTowerWallType>,    wipe_tower_wall_type))
    ((ConfigOptionFloat,              wipe_tower_extra_rib_length))
    ((ConfigOptionFloat,              wipe_tower_rib_width))
    ((ConfigOptionBool,               wipe_tower_fillet_wall))
    ((ConfigOptionBool,               wipe_tower_wall_gap))
    ((ConfigOptionInt,                wipe_tower_filament))
    ((ConfigOptionFloats,             wiping_volumes_extruders))
    ((ConfigOptionInts,       idle_temperature))
    ((ConfigOptionFloats, filament_tower_ironing_area))

    // BBS: wipe tower is only used for priming
    ((ConfigOptionFloat,              prime_volume))
    ((ConfigOptionFloat,              flush_multiplier))
    ((ConfigOptionFloat,              z_offset))
    // BBS: project filaments
    ((ConfigOptionFloats,             filament_colour_new))
    // BBS: not in any preset, calculated before slicing
    ((ConfigOptionFloat,              nozzle_volume))
    ((ConfigOptionPoints,             start_end_points))
    ((ConfigOptionEnum<TimelapseType>,    timelapse_type))
    ((ConfigOptionString,             thumbnails))
    // BBS: move from PrintObjectConfig
    ((ConfigOptionBool, independent_support_layer_height))
    // SoftFever
    ((ConfigOptionPercents,            filament_shrink))
    ((ConfigOptionPercents,            filament_shrinkage_compensation_z))
    ((ConfigOptionBool,                gcode_label_objects))
    ((ConfigOptionBool,                exclude_object))
    ((ConfigOptionBool,                gcode_comments))
    ((ConfigOptionInt,                 slow_down_layers))
    ((ConfigOptionInts,                support_material_interface_fan_speed))
    ((ConfigOptionInts,                internal_bridge_fan_speed)) // ORCA: Add support for separate internal bridge fan speed control
    ((ConfigOptionInts,                ironing_fan_speed))
    // Orca: notes for profiles from PrusaSlicer
    ((ConfigOptionStrings,             filament_notes))
    ((ConfigOptionString,              notes))
    ((ConfigOptionString,              printer_notes))

    ((ConfigOptionBools,               activate_chamber_temp_control))
    ((ConfigOptionInts ,               chamber_temperature))
    
    // Orca: support adaptive bed mesh
    ((ConfigOptionFloat,               preferred_orientation))
    ((ConfigOptionPoint,               bed_mesh_min))
    ((ConfigOptionPoint,               bed_mesh_max))
    ((ConfigOptionPoint,               bed_mesh_probe_distance))
    ((ConfigOptionFloat,               adaptive_bed_mesh_margin))


)

// This object is mapped to Perl as Slic3r::Config::Full.
PRINT_CONFIG_CLASS_DERIVED_DEFINE0(
    FullPrintConfig,
    (PrintObjectConfig, PrintRegionConfig, PrintConfig)
)

// Validate the FullPrintConfig. Returns an empty string on success, otherwise an error message is returned.
std::map<std::string, std::string> validate(const FullPrintConfig &config, bool under_cli = false);

PRINT_CONFIG_CLASS_DEFINE(
    SLAPrintConfig,
    ((ConfigOptionString,     filename_format))
)

PRINT_CONFIG_CLASS_DEFINE(
    SLAPrintObjectConfig,

    ((ConfigOptionFloat, layer_height))

    // 曝光时间淡出所需的层数 [3;20]
    ((ConfigOptionInt,  faded_layers))/*= 10*/

    ((ConfigOptionFloat, slice_closing_radius))

    // 启用或禁用支撑生成
    ((ConfigOptionBool,  supports_enable))

    // 支撑头尖端的直径（毫米）。
    ((ConfigOptionFloat, support_head_front_diameter))/*= 0.2*/

    // 支撑头需穿透模型表面的深度
    ((ConfigOptionFloat, support_head_penetration))/*= 0.2*/

    // 从后球心到前球心的宽度（毫米）。
    ((ConfigOptionFloat, support_head_width))/*= 1.0*/

    // 支撑柱的半径（毫米）。
    ((ConfigOptionFloat, support_pillar_diameter))/*= 0.8*/

    // 较小支柱占正常支柱直径的百分比，
    // 用于正常支柱无法容纳的问题区域。
    ((ConfigOptionPercent, support_small_pillar_diameter_percent))

    // 一个支柱上可放置的桥接数量。
    ((ConfigOptionInt,   support_max_bridges_on_pillar))

    // 支柱之间的桥接连接方式
    ((ConfigOptionEnum<SLAPillarConnectionMode>, support_pillar_connection_mode))

    // 仅生成面向底板的支撑
    ((ConfigOptionBool, support_buildplate_only))

    // TODO: 目前尚未实现。此系数将影响桥接和支柱合并时的效果。
    // 合并后的支柱应比合并前的支柱略粗。
    // 具体粗多少尚不确定，但将由该值推导得出。
    ((ConfigOptionFloat, support_pillar_widening_factor))

    // 支柱底座的半径（毫米）。
    ((ConfigOptionFloat, support_base_diameter))/*= 2.0*/

    // 支柱底座锥体的高度（毫米）。
    ((ConfigOptionFloat, support_base_height))/*= 1.0*/

    // 支柱底座与模型之间的最小距离（毫米）。
    ((ConfigOptionFloat, support_base_safety_distance)) /*= 1.0*/

    // 连接支撑杆和节点的默认角度。
    ((ConfigOptionFloat, support_critical_angle))/*= 45*/

    // 桥接的最大长度（毫米）
    ((ConfigOptionFloat, support_max_bridge_length))/*= 15.0*/

    // 两个支柱可交叉连接的最大距离。
    ((ConfigOptionFloat, support_max_pillar_link_distance))

    // 向上的 Z 方向抬升高度。这是垫板与模型物体边界框底部之间的空间。单位：毫米。
    ((ConfigOptionFloat, support_object_elevation))/*= 5.0*/

    /////// 以下选项影响自动支撑点放置：
    ((ConfigOptionInt, support_points_density_relative))
    ((ConfigOptionFloat, support_points_minimal_distance))

    // 基础垫板 /////////////////////////////////////////////

    // 启用或禁用垫板
    ((ConfigOptionBool,  pad_enable))

    // 垫板壁的厚度
    ((ConfigOptionFloat, pad_wall_thickness))/*= 2*/

    // 垫板从底部到顶部的高度，不考虑凹坑
    ((ConfigOptionFloat, pad_wall_height))/*= 5*/

    // 垫板应围绕包含的几何体延伸多远
    ((ConfigOptionFloat, pad_brim_size))

    // 两个独立垫板合并为一个的最大距离。
    // 距离大致从垫板的质心测量。
    ((ConfigOptionFloat, pad_max_merge_distance))/*= 50*/

    // 垫板边缘的平滑半径
    // ((ConfigOptionFloat, pad_edge_radius))/*= 1*/;

    // 垫板壁的倾斜度...
    ((ConfigOptionFloat, pad_wall_slope))

    // /////////////////////////////////////////////////////////////////////////
    // 零抬升模式参数：
    //    - 物体垫板将从模型几何体派生。
    //    - 物体垫板和生成的垫板之间将根据
    //      support_base_safety_distance 参数存在间隙。
    //    - 两个垫板将通过微小的连接棒连接。
    // /////////////////////////////////////////////////////////////////////////

    // 禁用抬升（忽略其值）并使用零抬升模式
    ((ConfigOptionBool, pad_around_object))

    ((ConfigOptionBool, pad_around_object_everywhere))

    // 物体底部与生成的垫板之间的间隙
    ((ConfigOptionFloat, pad_object_gap))

    // 在物体垫板周长上放置连接棒的间距
    ((ConfigOptionFloat, pad_object_connector_stride))

    // 连接棒的宽度
    ((ConfigOptionFloat, pad_object_connector_width))

    // 微小连接器应穿透模型本体的深度
    ((ConfigOptionFloat, pad_object_connector_penetration))

    // /////////////////////////////////////////////////////////////////////////
    // 模型掏空参数：
    //   - 模型可以在 SLA 打印过程中被掏空
    //   - 掏空模型壁的厚度可以调整
    //   -
    //   - 将在掏空模型上钻额外的孔以便树脂排出
    // /////////////////////////////////////////////////////////////////////////

    ((ConfigOptionBool, hollowing_enable))

    // 要维持的模型壁的最小厚度。注意，
    // 由于平滑处理可能导致树脂卡住的细小空腔，
    // 最终壁可能会更厚。
    ((ConfigOptionFloat, hollowing_min_thickness))

    // 间接控制 openvdb 使用的体素大小（分辨率）
    ((ConfigOptionFloat, hollowing_quality))

    // 间接控制创建的空腔的最小尺寸。
    ((ConfigOptionFloat, hollowing_closing_distance))
)

enum SLAMaterialSpeed { slamsSlow, slamsFast };

PRINT_CONFIG_CLASS_DEFINE(
    SLAMaterialConfig,

    ((ConfigOptionFloat,                       initial_layer_height))
    ((ConfigOptionFloat,                       bottle_cost))
    ((ConfigOptionFloat,                       bottle_volume))
    ((ConfigOptionFloat,                       bottle_weight))
    ((ConfigOptionFloat,                       material_density))
    ((ConfigOptionFloat,                       exposure_time))
    ((ConfigOptionFloat,                       initial_exposure_time))
    ((ConfigOptionFloats,                      material_correction))
    ((ConfigOptionFloat,                       material_correction_x))
    ((ConfigOptionFloat,                       material_correction_y))
    ((ConfigOptionFloat,                       material_correction_z))
    ((ConfigOptionEnum<SLAMaterialSpeed>,      material_print_speed))
)

PRINT_CONFIG_CLASS_DEFINE(
    SLAPrinterConfig,

    ((ConfigOptionEnum<PrinterTechnology>,    printer_technology))
    ((ConfigOptionPoints,                     printable_area))
    ((ConfigOptionFloat,                      printable_height))
    ((ConfigOptionFloat,                      display_width))
    ((ConfigOptionFloat,                      display_height))
    ((ConfigOptionInt,                        display_pixels_x))
    ((ConfigOptionInt,                        display_pixels_y))
    ((ConfigOptionEnum<SLADisplayOrientation>,display_orientation))
    ((ConfigOptionBool,                       display_mirror_x))
    ((ConfigOptionBool,                       display_mirror_y))
    ((ConfigOptionFloats,                     relative_correction))
    ((ConfigOptionFloat,                      relative_correction_x))
    ((ConfigOptionFloat,                      relative_correction_y))
    ((ConfigOptionFloat,                      relative_correction_z))
    ((ConfigOptionFloat,                      absolute_correction))
    ((ConfigOptionFloat,                      elefant_foot_compensation))
    ((ConfigOptionFloat,                      elefant_foot_min_width))
    ((ConfigOptionFloat,                      gamma_correction))
    ((ConfigOptionFloat,                      fast_tilt_time))
    ((ConfigOptionFloat,                      slow_tilt_time))
    ((ConfigOptionFloat,                      area_fill))
    ((ConfigOptionFloat,                      min_exposure_time))
    ((ConfigOptionFloat,                      max_exposure_time))
    ((ConfigOptionFloat,                      min_initial_exposure_time))
    ((ConfigOptionFloat,                      max_initial_exposure_time))
)

PRINT_CONFIG_CLASS_DERIVED_DEFINE0(
    SLAFullPrintConfig,
    (SLAPrinterConfig, SLAPrintConfig, SLAPrintObjectConfig, SLAMaterialConfig)
)

#undef STATIC_PRINT_CONFIG_CACHE
#undef STATIC_PRINT_CONFIG_CACHE_BASE
#undef STATIC_PRINT_CONFIG_CACHE_DERIVED
#undef PRINT_CONFIG_CLASS_ELEMENT_DEFINITION
#undef PRINT_CONFIG_CLASS_ELEMENT_EQUAL
#undef PRINT_CONFIG_CLASS_ELEMENT_LOWER
#undef PRINT_CONFIG_CLASS_ELEMENT_HASH
#undef PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION
#undef PRINT_CONFIG_CLASS_ELEMENT_INITIALIZATION2
#undef PRINT_CONFIG_CLASS_DEFINE
#undef PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST
#undef PRINT_CONFIG_CLASS_DERIVED_CLASS_LIST_ITEM
#undef PRINT_CONFIG_CLASS_DERIVED_DEFINE
#undef PRINT_CONFIG_CLASS_DERIVED_DEFINE0
#undef PRINT_CONFIG_CLASS_DERIVED_DEFINE1
#undef PRINT_CONFIG_CLASS_DERIVED_HASH
#undef PRINT_CONFIG_CLASS_DERIVED_EQUAL
#undef PRINT_CONFIG_CLASS_DERIVED_INITCACHE_ITEM
#undef PRINT_CONFIG_CLASS_DERIVED_INITCACHE
#undef PRINT_CONFIG_CLASS_DERIVED_INITIALIZER
#undef PRINT_CONFIG_CLASS_DERIVED_INITIALIZER_ITEM

class CLIActionsConfigDef : public ConfigDef
{
public:
    CLIActionsConfigDef();
};

class CLITransformConfigDef : public ConfigDef
{
public:
    CLITransformConfigDef();
};

class CLIMiscConfigDef : public ConfigDef
{
public:
    CLIMiscConfigDef();
};

typedef std::string t_custom_gcode_key;
// 此映射包含每个自定义 G-code 的特定占位符列表（如果存在）
const std::map<t_custom_gcode_key, t_config_option_keys>& custom_gcode_specific_placeholders();

// 接下来定义的类用于 GUI::EditGCodeDialog 使用的占位符。

class ReadOnlySlicingStatesConfigDef : public ConfigDef
{
public:
    ReadOnlySlicingStatesConfigDef();
};

class ReadWriteSlicingStatesConfigDef : public ConfigDef
{
public:
    ReadWriteSlicingStatesConfigDef();
};

class OtherSlicingStatesConfigDef : public ConfigDef
{
public:
    OtherSlicingStatesConfigDef();
};

class PrintStatisticsConfigDef : public ConfigDef
{
public:
    PrintStatisticsConfigDef();
};

class ObjectsInfoConfigDef : public ConfigDef
{
public:
    ObjectsInfoConfigDef();
};

class DimensionsConfigDef : public ConfigDef
{
public:
    DimensionsConfigDef();
};

class TemperaturesConfigDef : public ConfigDef
{
public:
    TemperaturesConfigDef();
};

class TimestampsConfigDef : public ConfigDef
{
public:
    TimestampsConfigDef();
};

class OtherPresetsConfigDef : public ConfigDef
{
public:
    OtherPresetsConfigDef();
};

// 此类定义所有自定义 G-code 特定的占位符。
class CustomGcodeSpecificConfigDef : public ConfigDef
{
public:
    CustomGcodeSpecificConfigDef();
};
extern const CustomGcodeSpecificConfigDef    custom_gcode_specific_config_def;

// 此类定义代表操作的命令行选项。
extern const CLIActionsConfigDef    cli_actions_config_def;

// 此类定义代表变换的命令行选项。
extern const CLITransformConfigDef  cli_transform_config_def;

// 此类定义所有既不是操作也不是变换的命令行选项。
extern const CLIMiscConfigDef       cli_misc_config_def;

class DynamicPrintAndCLIConfig : public DynamicPrintConfig
{
public:
    DynamicPrintAndCLIConfig() {}
    DynamicPrintAndCLIConfig(const DynamicPrintAndCLIConfig &other) : DynamicPrintConfig(other) {}

    // 覆盖 ConfigBase::def()。静态配置定义。存储在此 ConfigBase 中的任何值都应有其定义。
    const ConfigDef*        def() const override { return &s_def; }

    // 验证 opt_key 是否已废弃或重命名。
    // opt_key 和 value 都可能被 handle_legacy() 修改。
    // 如果 opt_key 在此版本的 Slic3r 中不再有效，handle_legacy() 会清空 opt_key。
    // handle_legacy() 由 set_deserialize() 内部调用。
    void                    handle_legacy(t_config_option_key &opt_key, std::string &value) const override;

private:
    class PrintAndCLIConfigDef : public ConfigDef
    {
    public:
        PrintAndCLIConfigDef() {
            this->options.insert(print_config_def.options.begin(), print_config_def.options.end());
            this->options.insert(cli_actions_config_def.options.begin(), cli_actions_config_def.options.end());
            this->options.insert(cli_transform_config_def.options.begin(), cli_transform_config_def.options.end());
            this->options.insert(cli_misc_config_def.options.begin(), cli_misc_config_def.options.end());
            for (const auto &kvp : this->options)
                this->by_serialization_key_ordinal[kvp.second.serialization_key_ordinal] = &kvp.second;
        }
        // 不要释放默认值，它们由 print_config_def 和 cli_actions_config_def / cli_transform_config_def / cli_misc_config_def 管理。
        ~PrintAndCLIConfigDef() { this->options.clear(); }
    };
    static PrintAndCLIConfigDef s_def;
};

bool is_XL_printer(const DynamicPrintConfig &cfg);
bool is_XL_printer(const PrintConfig &cfg);

Points get_bed_shape(const DynamicPrintConfig &cfg);
Points get_bed_shape(const PrintConfig &cfg);
Points get_bed_shape(const SLAPrinterConfig &cfg);
Slic3r::Polygons get_bed_excluded_area(const PrintConfig& cfg);
Slic3r::Polygon get_bed_shape_with_excluded_area(const PrintConfig& cfg);
bool has_skirt(const DynamicPrintConfig& cfg);
float get_real_skirt_dist(const DynamicPrintConfig& cfg);

// ModelConfig 是在 DynamicPrintConfig 基础上添加了时间戳的封装。
// ModelConfig 的每次更改都通过从全局计数器分配新时间戳来跟踪。
// 该计数器用于通过跳过相等配置字典的同步来加快后台切片线程与前端的同步速度。
// 全局计数器还用于在执行撤销快照时避免不必要的配置字典序列化。
//
// 全局计数器不是线程安全的，因此建议仅从主线程使用 ModelConfig。
//
// 由于存在全局计数器且每次对任何 ModelConfig 的更改都会增加它，
// 如果两个 ModelConfig 字典不同，它们的时间戳也应不同。
// 因此，复制包含时间戳的 ModelConfig 是安全的，只要字典相等，
// 拥有多个具有相同时间戳的 ModelConfig 是无害的。
//
// 时间戳由撤销/重做栈使用。由于零时间戳对撤销/重做栈意味着无效时间戳
//（零时间戳意味着撤销/重做栈需要序列化并比较序列化数据的差异），
// 因此绝不应使用零时间戳。
// Timestamp==1 仅应用于空字典。
class ModelConfig
{
public:
    // 以下方法清除配置并增加其时间戳，以便从撤销/重做栈的角度认为已删除状态已更改。
    void         reset() { m_data.clear(); touch(); }

    void         assign_config(const ModelConfig &rhs) {
        if (m_timestamp != rhs.m_timestamp) {
            m_data      = rhs.m_data;
            m_timestamp = rhs.m_timestamp;
        }
    }
    void         assign_config(ModelConfig &&rhs) {
        if (m_timestamp != rhs.m_timestamp) {
            m_data      = std::move(rhs.m_data);
            m_timestamp = rhs.m_timestamp;
            rhs.reset();
        }
    }

    // 由于全局时间戳计数器，ModelConfig 的修改不是线程安全的！
    // 不要从后端调用修改方法！
    // 如果 src==dst，分配方法不会进行分配，以避免在相等时增加时间戳。
    void         assign_config(const DynamicPrintConfig &rhs)  { if (m_data != rhs) { m_data = rhs; this->touch(); } }
    void         assign_config(DynamicPrintConfig &&rhs)       { if (m_data != rhs) { m_data = std::move(rhs); this->touch(); } }
    void         apply(const ModelConfig &other, bool ignore_nonexistent = false) { this->apply(other.get(), ignore_nonexistent); }
    void         apply(const ConfigBase &other, bool ignore_nonexistent = false) { m_data.apply_only(other, other.keys(), ignore_nonexistent); this->touch(); }
    void         apply_only(const ModelConfig &other, const t_config_option_keys &keys, bool ignore_nonexistent = false) { this->apply_only(other.get(), keys, ignore_nonexistent); }
    void         apply_only(const ConfigBase &other, const t_config_option_keys &keys, bool ignore_nonexistent = false) { m_data.apply_only(other, keys, ignore_nonexistent); this->touch(); }
    bool         set_key_value(const std::string &opt_key, ConfigOption *opt) { bool out = m_data.set_key_value(opt_key, opt); this->touch(); return out; }
    template<typename T>
    void         set(const std::string &opt_key, T value) { m_data.set(opt_key, value, true); this->touch(); }
    void         set_deserialize(const t_config_option_key &opt_key, const std::string &str, ConfigSubstitutionContext &substitution_context, bool append = false)
        { m_data.set_deserialize(opt_key, str, substitution_context, append); this->touch(); }
    bool         erase(const t_config_option_key &opt_key) { bool out = m_data.erase(opt_key); if (out) this->touch(); return out; }

    // Getter 是线程安全的。
    // 以下隐式转换会破坏 Cereal 序列化。
//    operator const DynamicPrintConfig&() const throw() { return this->get(); }
    const DynamicPrintConfig&   get() const throw() { return m_data; }
    bool                        empty() const throw() { return m_data.empty(); }
    size_t                      size() const throw() { return m_data.size(); }
    auto                        cbegin() const { return m_data.cbegin(); }
    auto                        cend() const { return m_data.cend(); }
    t_config_option_keys        keys() const { return m_data.keys(); }
    bool                        has(const t_config_option_key &opt_key) const { return m_data.has(opt_key); }
    const ConfigOption*         option(const t_config_option_key &opt_key) const { return m_data.option(opt_key); }
    int                         opt_int(const t_config_option_key &opt_key) const { return m_data.opt_int(opt_key); }
    int                         extruder() const { return opt_int("extruder"); }
    double opt_float(const t_config_option_key &opt_key) const {
      return m_data.opt_float(opt_key);
    }
    double get_abs_value(const t_config_option_key &opt_key) const {
      return m_data.get_abs_value(opt_key);
    }
    std::string                 opt_serialize(const t_config_option_key &opt_key) const { return m_data.opt_serialize(opt_key); }

    // 返回此对象的可选时间戳。
    // 如果返回的时间戳非零，则序列化框架仅在时间戳与撤销/重做栈顶部对象的时间戳不同时，
    // 才将此对象保存在撤销/重做栈上。
    virtual uint64_t    timestamp() const throw() { return m_timestamp; }
    bool                timestamp_matches(const ModelConfig &rhs) const throw() { return m_timestamp == rhs.m_timestamp; }
    // 不是线程安全的！不应在主线程之外调用！
    void                touch() { m_timestamp = ++ s_last_timestamp; }

private:
    friend class cereal::access;
    template<class Archive> void serialize(Archive& ar) { ar(m_timestamp); ar(m_data); }

    uint64_t                    m_timestamp { 1 };
    DynamicPrintConfig          m_data;

    static uint64_t             s_last_timestamp;
};

} // namespace Slic3r

// 通过 Cereal 库进行序列化
namespace cereal {
    // 让 cereal 知道 DynamicPrintConfig 声明了 load/save 非成员函数，忽略父类 DynamicConfig 的 serialize/load/save。
    template <class Archive> struct specialize<Archive, Slic3r::DynamicPrintConfig, cereal::specialization::non_member_load_save> {};

    template<class Archive> void load(Archive& archive, Slic3r::DynamicPrintConfig &config)
    {
        size_t cnt;
        archive(cnt);
        config.clear();
        for (size_t i = 0; i < cnt; ++ i) {
            size_t serialization_key_ordinal;
            archive(serialization_key_ordinal);
            assert(serialization_key_ordinal > 0);
            auto it = Slic3r::print_config_def.by_serialization_key_ordinal.find(serialization_key_ordinal);
            assert(it != Slic3r::print_config_def.by_serialization_key_ordinal.end());
            config.set_key_value(it->second->opt_key, it->second->load_option_from_archive(archive));
        }
    }

    template<class Archive> void save(Archive& archive, const Slic3r::DynamicPrintConfig &config)
    {
        size_t cnt = config.size();
        archive(cnt);
        for (auto it = config.cbegin(); it != config.cend(); ++it) {
            const Slic3r::ConfigOptionDef* optdef = Slic3r::print_config_def.get(it->first);
            assert(optdef != nullptr);
            assert(optdef->serialization_key_ordinal > 0);
            archive(optdef->serialization_key_ordinal);
            optdef->save_option_to_archive(archive, it->second.get());
        }
    }
}

#endif
