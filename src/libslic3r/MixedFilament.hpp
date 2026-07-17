#ifndef slic3r_MixedFilament_hpp_
#define slic3r_MixedFilament_hpp_

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace Slic3r {

class PrintObject;

std::vector<int> fill_continuous_layer_range(const std::vector<int> &sorted_layers);

// 表示由物理耗材创建的虚拟"混合"耗材
// （层节奏和/或同层交错条纹分布）。显示
// 颜色混合使用FilamentMixer，以便配对预览更
// 匹配预期的打印混合效果
// （例如蓝+黄 -> 绿，红+黄 -> 橙，红+蓝 -> 紫）。
// 旧的RYB代码保留在源代码中仅供参考。
struct MixedFilament
{
    enum DistributionMode : uint8_t {
        LayerCycle = 0,
        SameLayerPointillisme = 1,
        Simple = 2
    };

    // 组合的基于1的物理耗材ID。
    unsigned int component_a = 1;
    unsigned int component_b = 2;

    // 持久行标识，用于在可见的混合耗材列表重建时保持绘制的虚拟工具分配的稳定性。
    uint64_t stable_id = 0;

    // 层交替比例。当ratio_a = 2, ratio_b = 1时，循环为A, A, B, A, A, B, ...
    int ratio_a = 1;
    int ratio_b = 1;

    // 组分B的混合百分比，范围[0..100]。
    int mix_b_percent = 50;

    // 此混合耗材的可选手动模式。
    // 旧格式（无'/'）：每个'1'-'9'字符是一个标记。
    //   '1'=>component_a, '2'=>component_b, '3'-'9'=>直接物理ID。
    // 新格式（带'/'）：'/'分隔标记，支持多位数字ID。
    //   例如 "1/10/2/11/12"。逗号分隔每个周长的组。
    std::string manual_pattern;

    // 可选的显式渐变多色组分列表，编码为紧凑的物理耗材ID（例如"123" -> 耗材1,2,3）。
    // 仅当此列表有3个以上ID时，渐变行的交错条纹模式才激活。
    std::string gradient_component_ids;
    // 与gradient_component_ids对齐的可选显式多色权重。
    // 用'/'连接的紧凑整数列表：例如"50/25/25"。
    std::string gradient_component_weights;

    // 来自早期原型序列化的旧兼容性标志。
    bool pointillism_all_filaments = false;

    // 这个混合行的分布方式：
    // - LayerCycle：基于节奏每层一个耗材。
    // - SameLayerPointillisme：在每层的XY上分割绘制的遮罩。
    int distribution_mode = int(Simple);

    // 此混合行的可选Local-Z上限。0表示禁用上限。
    int local_z_max_sublayers = 0;

    static constexpr float k_default_gradient_dominant = 0.8f;  // 主要组分比例
    static constexpr float k_default_gradient_minority = 0.2f;  // 次要组分比例
    static constexpr float k_min_gradient_difference   = 0.05f; // 有效渐变的最小差值
    
    bool  gradient_enabled = false;
    float gradient_start = k_default_gradient_dominant;
    float gradient_end   = k_default_gradient_minority;

    // 以毫米为单位的附加XY表面偏移，在此混合行解析为整个层的组分A或B时应用。
    // 正值向内收缩；负值向外扩展。
    float component_a_surface_offset = 0.f;
    float component_b_surface_offset = 0.f;

    // 此混合耗材是否启用（可用于分配）。
    bool enabled = true;

    // 当此混合耗材行从UI删除时应保持隐藏时为true。
    bool deleted = false;

    // 当此行是用户创建的（自定义）而不是自动生成的时为true。
    bool custom = false;

    // 当此行源自自动生成的对时为true。即使在编辑后仍保持true，
    // 以便删除逻辑可以保持基础自动对已删除状态，而不是让重新生成复活它。
    bool origin_auto = false;

    // 创建此行的UI模式（-1=未知/旧版，0=RATIO，1=CYCLE，2=MATCH，3=GRADIENT）。
    int ui_mode = -1;

    // 计算出的显示颜色，格式为 "#RRGGBB"。
    std::string display_color;

    bool operator==(const MixedFilament &rhs) const
    {
        constexpr float k_surface_offset_epsilon = 1e-6f;
        constexpr float k_gradient_epsilon       = 1e-4f;
        return component_a == rhs.component_a &&
               component_b == rhs.component_b &&
               stable_id   == rhs.stable_id   &&
               ratio_a     == rhs.ratio_a     &&
               ratio_b     == rhs.ratio_b     &&
               mix_b_percent == rhs.mix_b_percent &&
               manual_pattern == rhs.manual_pattern &&
               gradient_component_ids == rhs.gradient_component_ids &&
               gradient_component_weights == rhs.gradient_component_weights &&
               pointillism_all_filaments == rhs.pointillism_all_filaments &&
               distribution_mode == rhs.distribution_mode &&
               local_z_max_sublayers == rhs.local_z_max_sublayers &&
               gradient_enabled == rhs.gradient_enabled &&
               std::abs(gradient_start - rhs.gradient_start) <= k_gradient_epsilon &&
               std::abs(gradient_end   - rhs.gradient_end)   <= k_gradient_epsilon &&
               std::abs(component_a_surface_offset - rhs.component_a_surface_offset) <= k_surface_offset_epsilon &&
               std::abs(component_b_surface_offset - rhs.component_b_surface_offset) <= k_surface_offset_epsilon &&
               enabled      == rhs.enabled &&
               deleted      == rhs.deleted &&
               custom       == rhs.custom &&
               origin_auto  == rhs.origin_auto &&
               ui_mode      == rhs.ui_mode;
    }
    bool operator!=(const MixedFilament &rhs) const { return !(*this == rhs); }
};

struct MixedFilamentPreviewSettings
{
    double nominal_layer_height { 0.2 };
    double mixed_lower_bound { 0.04 };
    double mixed_upper_bound { 0.16 };
    double preferred_a_height { 0.0 };
    double preferred_b_height { 0.0 };
    bool   local_z_mode { false };
    bool   local_z_direct_multicolor { false };
    size_t wall_loops { 1 };
};

struct MixedFilamentDisplayContext
{
    size_t                       num_physical { 0 };
    std::vector<std::string>     physical_colors;
    std::vector<double>          nozzle_diameters;
    MixedFilamentPreviewSettings preview_settings;
    bool                         component_bias_enabled { false };
};

int mixed_filament_effective_local_z_preview_mix_b_percent(const MixedFilament               &mf,
                                                           const MixedFilamentPreviewSettings &preview_settings);
bool mixed_filament_supports_bias_apparent_color(const MixedFilament               &mf,
                                                 const MixedFilamentPreviewSettings &preview_settings,
                                                 bool                                bias_mode_enabled);
std::pair<int, int> mixed_filament_apparent_pair_percentages(const MixedFilament               &mf,
                                                             const MixedFilamentPreviewSettings &preview_settings,
                                                             const std::vector<double>          &nozzle_diameters,
                                                             bool                                bias_mode_enabled);
std::string compute_mixed_filament_display_color(const MixedFilament &entry, const MixedFilamentDisplayContext &context);

// ---------------------------------------------------------------------------
// MixedFilamentManager
//
// 拥有混合耗材列表，并提供切片管道用于将虚拟ID解析回物理挤出机的辅助函数。
//
// 虚拟耗材ID从(num_physical + 1)开始编号。对于
// 4挤出机打印机，第一个混合耗材的ID为5，第二个为6，依此类推。
// ---------------------------------------------------------------------------
class MixedFilamentManager
{
public:
    MixedFilamentManager() = default;

    static void set_auto_generate_enabled(bool enabled);
    static bool auto_generate_enabled();

    // ---- 自动生成 ------------------------------------------------

    // 从当前物理耗材颜色集合重建混合耗材列表。生成所有C(N,2)配对组合。
    // 当组合仍然存在时，保留先前的比例/启用状态。
    void auto_generate(const std::vector<std::string> &filament_colours);

    // 从混合列表中移除物理耗材（基于1的ID）。
    // 任何包含被移除组件的混合耗材将被删除。
    // 剩余的组件ID向下移位以保持与物理ID对齐。
    void remove_physical_filament(unsigned int deleted_filament_id);

    // 添加自定义混合耗材。
    void add_custom_filament(unsigned int component_a, unsigned int component_b, int mix_b_percent, const std::vector<std::string> &filament_colours);

    // 移除所有自定义行，保留自动生成的行。
    void clear_custom_entries();

    // 从内存中清理已删除的混合耗材。
    // 应在序列化后调用，以移除不再需要的已删除条目。
    void cleanup_deleted_entries();

    // 根据渐变设置重新计算节奏比例。
    // gradient_mode: 0 = 层循环加权，1 = 高度加权。
    void apply_gradient_settings(int   gradient_mode,
                                 float lower_bound,
                                 float upper_bound,
                                 bool  advanced_dithering = false);

    // 将混合行（包括自动/已删除状态）持久化到紧凑的项目设置字符串中。
    std::string serialize_custom_entries();
    void load_custom_entries(const std::string &serialized, const std::vector<std::string> &filament_colours);

    // ---- 模式字符串函数 -------------------------------------------
    // 将手动混合模式字符串规范化为标准形式。
    // 格式：数字1-9表示ID 1-9，[N]表示ID >= 10，逗号作为组分隔符。
    // 如果无效则返回空字符串。
    static std::string normalize_manual_pattern(const std::string &pattern);
    static int         mix_percent_from_manual_pattern(const std::string &pattern);

    // 将单个模式组（无逗号）分词为标记字符串。
    // 处理单个数字和括号([N])标记。
    static std::vector<std::string> split_pattern_group_to_tokens(const std::string &group, size_t num_physical);

    // 将字符串标记映射到物理挤出机ID。
    // "1" => component_a, "2" => component_b, "3"+ => 直接物理ID。
    static unsigned int physical_filament_from_token(const std::string &token, const MixedFilament &mf, size_t num_physical);

    // 按逗号将规范化模式字符串分割为组字符串。
    static std::vector<std::string> split_pattern_groups(const std::string &pattern);

    // ---- 渐变组件ID编码/解码 ------------------------

    // 编码支持的最大物理耗材数量。
    static constexpr size_t kMaxPhysicalFilaments = 64;

    // 将耗材ID（基于1）编码为紧凑字符串。
    // 旧格式（所有ID ≤ 9）：连接单个字符，例如 "132"。
    // 扩展格式（任何ID > 9）：'/'分隔的十进制数，例如 "1/12/3"。
    // 单ID扩展使用前导'/'以消除歧义，例如 "/12"。
    static std::string encode_gradient_component_ids(const std::vector<unsigned int> &ids);

    // 将gradient_component_ids字符串解码为耗材ID的向量（基于1）。
    // 处理旧格式和扩展格式。当num_physical > 0时，每个ID都验证为≤ num_physical。
    static std::vector<unsigned int> decode_gradient_component_ids(const std::string &components,
                                                                   size_t             num_physical = 0);

    // 将排序/去重向量中的虚拟混合耗材ID扩展为它们的物理组件ID（component_a、component_b和梯度组件ID）。
    // ID ≤ num_physical保持不变。调用者负责在调用后重新排序和重新去重。
    void expand_virtual_extruder_ids(std::vector<int> &ids, size_t num_physical) const;

    // 将gradient_component_ids字符串规范化为标准形式。
    // 标准形式在ID均≤9时使用旧编码，否则使用扩展编码。
    static std::string normalize_gradient_component_ids(const std::string &components);

    // ---- 查询 --------------------------------------------------------

    // 当`filament_id`（基于1）引用混合耗材时为true。
    bool is_mixed(unsigned int filament_id, size_t num_physical) const
    {
        return mixed_index_from_filament_id(filament_id, num_physical) >= 0;
    }

    // 将混合耗材ID解析为给定层上下文的物理挤出机（基于1）。
    // 当不是混合耗材时，返回未更改的`filament_id`。
    unsigned int resolve(unsigned int filament_id,
                         size_t       num_physical,
                         int          layer_index,
                         float        layer_print_z = 0.f,
                         float        layer_height  = 0.f,
                         bool         force_height_weighted = false,
                         const PrintObject* current_object = nullptr) const;
    unsigned int resolve_perimeter(unsigned int filament_id,
                                   size_t       num_physical,
                                   int          layer_index,
                                   int          perimeter_index,
                                   float        layer_print_z = 0.f,
                                   float        layer_height  = 0.f,
                                   bool         force_height_weighted = false,
                                   const PrintObject* current_object = nullptr) const;
    // 解析应拥有此层上绘制区域的耗材ID。
    // 在G-code生成后期需要虚拟身份的模式保留原始混合ID；
    // 普通混合行折叠为当前物理挤出机，以便相邻的相同工具区域可以合并。
    unsigned int effective_painted_region_filament_id(unsigned int filament_id,
                                                      size_t       num_physical,
                                                      int          layer_index,
                                                      float        layer_print_z = 0.f,
                                                      float        layer_height  = 0.f,
                                                      float        layer_height_a = 0.f,
                                                      float        layer_height_b = 0.f,
                                                      float        base_layer_height = 0.2f) const;
    float component_surface_offset(unsigned int filament_id,
                                   size_t       num_physical,
                                   int          layer_index,
                                   float        layer_print_z = 0.f,
                                   float        layer_height  = 0.f,
                                   bool         force_height_weighted = false) const;
    std::vector<unsigned int> ordered_perimeter_extruders(unsigned int filament_id,
                                                          size_t       num_physical,
                                                          int          layer_index,
                                                          float        layer_print_z = 0.f,
                                                          float        layer_height  = 0.f,
                                                          bool         force_height_weighted = false) const;

    // 将虚拟耗材ID（基于1，物理ID之后）映射到m_mixed中的索引。虚拟ID仅枚举启用的混合行。
    int mixed_index_from_filament_id(unsigned int filament_id, size_t num_physical) const;

    // 使用加权FilamentMixer混合N种颜色。
    // color_percents: (hex_color, percent)的向量，其中百分比总和为100。
    static std::string blend_color_multi(
        const std::vector<std::pair<std::string, int>> &color_percents);

    const MixedFilament *mixed_filament_from_id(unsigned int filament_id, size_t num_physical) const;

    // 获取所有依赖于特定物理耗材（基于1的ID）的混合耗材索引。
    // 返回m_mixed中索引的向量，用于将物理耗材作为组件（component_a、component_b或gradient_component_ids中）使用的混合耗材。
    std::vector<size_t> mixed_filaments_using_physical(unsigned int physical_filament_1based) const;

    // 通过使用FilamentMixer混合两种颜色来计算显示颜色。
    static std::string blend_color(const std::string &color_a,
                                   const std::string &color_b,
                                   int ratio_a, int ratio_b);
    static float max_component_surface_offset_mm(float reference_width_mm = 0.4f);
    static float max_pair_bias_mm(float reference_width_mm = 0.4f);
    static std::pair<float, float> surface_offset_pair_from_signed_bias(float bias_mm,
                                                                        float reference_width_mm = 0.4f);
    static float bias_ui_value_from_surface_offsets(float component_a_surface_offset,
                                                    float component_b_surface_offset,
                                                    float reference_width_mm = 0.4f);
    static int apparent_mix_b_percent(int   mix_b_percent,
                                      float component_a_surface_offset,
                                      float component_b_surface_offset,
                                      float reference_width_mm = 0.4f);

    // 为单元测试公开——纯逻辑辅助函数。
    static int         safe_mod(int x, int m);
    static void        normalize_ratio_pair(int &a, int &b);
    static float       canonical_signed_bias_value(float component_a_surface_offset, float component_b_surface_offset);
    static std::string format_surface_offset_token(float value);
    static double      mixed_filament_reference_nozzle_mm(unsigned int component_a, unsigned int component_b, const std::vector<double> &nozzle_diameters);

    // ---- 访问器 ------------------------------------------------------

    const std::vector<MixedFilament> &mixed_filaments() const { return m_mixed; }
    std::vector<MixedFilament>       &mixed_filaments()       { return m_mixed; }

    size_t enabled_count() const;

    // 总耗材数量 = num_physical + *启用的*混合耗材数量。
    size_t total_filaments(size_t num_physical) const { return num_physical + enabled_count(); }

    // 返回所有启用的混合耗材的显示颜色（按顺序）。
    std::vector<std::string> display_colors() const;
    void set_display_context(const MixedFilamentDisplayContext &context);

private:
    // 将基于1的虚拟ID转换为m_mixed中基于0的索引。
    size_t index_of(unsigned int filament_id, size_t num_physical) const
    {
        return static_cast<size_t>(filament_id - num_physical - 1);
    }

    void refresh_display_colors(const std::vector<std::string> &filament_colours);
    uint64_t allocate_stable_id();
    uint64_t normalize_stable_id(uint64_t stable_id);

    std::vector<MixedFilament> m_mixed;
    int                        m_gradient_mode       = 0;
    float                      m_height_lower_bound  = 0.04f;
    float                      m_height_upper_bound  = 0.16f;
    bool                       m_advanced_dithering  = false;
    uint64_t                   m_next_stable_id      = 1;
    MixedFilamentDisplayContext m_display_context;
};

// 当混合耗材表示可以渲染为垂直颜色斜坡的简单双色渐变时返回true（无手动模式，恰好2个组件）。
inline bool is_simple_gradient(const MixedFilament& mf)
{
    // 轻量级ID计数，无堆分配。
    // 标准形式：旧版"12"=两个ID，扩展"1/12/3"=三个ID。
    auto count_ids = [](const std::string& s) -> size_t {
        if (s.empty()) return 0;
        if (s.find('/') != std::string::npos) {
            size_t n = 0;
            bool in_token = false;
            for (char c : s) {
                if (c == '/') {
                    if (in_token) ++n;
                    in_token = false;
                } else {
                    in_token = true;
                }
            }
            if (in_token) ++n;
            return n;
        }
        return s.size();
    };
    return mf.gradient_enabled
        && mf.component_a != mf.component_b
        && MixedFilamentManager::normalize_manual_pattern(mf.manual_pattern).empty()
        && count_ids(mf.gradient_component_ids) < 3;
}

} // namespace Slic3r

#endif /* slic3r_MixedFilament_hpp_ */
