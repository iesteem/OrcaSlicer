// 工具排序以最小化工具切换。

#ifndef slic3r_ToolOrdering_hpp_
#define slic3r_ToolOrdering_hpp_

#include "../libslic3r.h"
#include "../MixedFilament.hpp"

#include <utility>

#include <boost/container/small_vector.hpp>

namespace Slic3r {

class Print;
class PrintObject;
class LayerTools;
namespace CustomGCode { struct Item; }
class PrintRegion;

// 此类的对象保存关于挤出是在工具更换后立即打印
// （作为填充/周长擦拭的一部分）还是不是的信息。一个挤出可以是多个副本的一部分
// - 这必须考虑到。
class WipingExtrusions
{
public:
    bool is_anything_overridden() const {   // 如果没有覆盖，则所有议程都可以跳过 - 此函数可以告诉我们是否如此
        return something_overridden;
    }

    // 当分配对象ExtrusionEntity的挤出机覆盖时，最多分配3个副本的覆盖。
    typedef boost::container::small_vector<int32_t, 3> ExtruderPerCopy;

    // 从GCode::process_layer调用 - 详见实现：
    const ExtruderPerCopy* get_extruder_overrides(const ExtrusionEntity* entity, const PrintObject* object, int correct_extruder_id, size_t num_of_copies);
    int get_support_extruder_overrides(const PrintObject* object);
    int get_support_interface_extruder_overrides(const PrintObject* object);

    // 此函数遍历所有填充实体，决定哪些将用于擦拭并
    // 用挤出机id标记它们。返回擦拭塔上仍需擦拭的体积：
    float mark_wiping_extrusions(const Print& print, unsigned int old_extruder, unsigned int new_extruder, float volume_to_wipe);

    void ensure_perimeters_infills_order(const Print& print);

    bool is_overriddable(const ExtrusionEntityCollection& ee, const PrintConfig& print_config, const PrintObject& object, const PrintRegion& region) const;
    bool is_overriddable_and_mark(const ExtrusionEntityCollection& ee, const PrintConfig& print_config, const PrintObject& object, const PrintRegion& region) {
        bool out = this->is_overriddable(ee, print_config, object, region);
        this->something_overridable |= out;
        return out;
    }

    // BBS
    bool is_support_overriddable(const ExtrusionRole role, const PrintObject& object) const;
    bool is_support_overriddable_and_mark(const ExtrusionRole role, const PrintObject& object) {
        bool out = this->is_support_overriddable(role, object);
        this->something_overridable |= out;
        return out;
    }

    bool is_support_overridden(const PrintObject* object) const {
        return support_map.find(object) != support_map.end();
    }

    bool is_support_interface_overridden(const PrintObject* object) const {
        return support_intf_map.find(object) != support_intf_map.end();
    }

    void set_layer_tools_ptr(const LayerTools* lt) { m_layer_tools = lt; }

private:
    int first_nonsoluble_extruder_on_layer(const PrintConfig& print_config) const;
    int last_nonsoluble_extruder_on_layer(const PrintConfig& print_config) const;

    // 此函数从mark_wiping_extrusions调用，设置挤出机应使用的挤出机（-1 .. 表示正常）
    void set_extruder_override(const ExtrusionEntity* entity, const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies);
    // BBS
    void set_support_extruder_override(const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies);
    void set_support_interface_extruder_override(const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies);

    // 如果给定副本的实体不是用其通常的挤出机打印的，则返回true：
    bool is_entity_overridden(const ExtrusionEntity* entity, const PrintObject *object, size_t copy_id) const {
        auto it = entity_map.find(std::make_tuple(entity, object));
        return it == entity_map.end() ? false : it->second[copy_id] != -1;
    }

    std::map<std::tuple<const ExtrusionEntity*, const PrintObject *>, ExtruderPerCopy> entity_map;  // 跟踪谁打印什么
    // BBS
    std::map<const PrintObject*, int> support_map;
    std::map<const PrintObject*, int> support_intf_map;
    bool something_overridable = false;
    bool something_overridden = false;
    const LayerTools* m_layer_tools = nullptr;    // 这样我们就知道这属于哪个LayerTools对象
};

class LayerTools
{
public:
    LayerTools(const coordf_t z) : print_z(z) {}

    // 将这些操作符更改为epsilon版本可能会在支撑层和对象层靠近时产生问题。
    // 如果有人尝试这样做，请确保你知道你在做什么并正确测试（同时用支撑一次切片多个对象）。
    bool operator< (const LayerTools &rhs) const { return print_z < rhs.print_z; }
    bool operator==(const LayerTools &rhs) const { return print_z == rhs.print_z; }

    bool is_extruder_order(unsigned int a, unsigned int b) const;
    bool has_extruder(unsigned int extruder) const { return std::find(this->extruders.begin(), this->extruders.end(), extruder) != this->extruders.end(); }

    // 从区域返回基于零的挤出机，如果覆盖则返回extruder_override。
    unsigned int wall_filament(const PrintRegion &region) const;
    unsigned int sparse_infill_filament(const PrintRegion &region) const;
    unsigned int solid_infill_filament(const PrintRegion &region) const;
    // 根据PrintRegion配置或挤出机覆盖返回应打印此eec的基于零的挤出机。
    unsigned int extruder(const ExtrusionEntityCollection &extrusions, const PrintRegion &region) const;

    coordf_t                    print_z = 0.;
    bool                        has_object = false;
    bool                        has_support = false;
    // 基于零的挤出机ID，按最小化工具切换的方式排序。
    std::vector<unsigned int>   extruders;
    bool                        preserve_extruder_order = false;
    // 如果通过G-code预览滑块插入逐层挤出机切换，此值包含新的（基于1的）挤出机，整个对象层将用其打印。
    // 如果未覆盖，则设为0。
    unsigned int                extruder_override = 0;
    // 顺序层索引（基于0），用于混合丝材解析。
    int                         layer_index = 0;
    // 当前打印对象的总对象层数。
    int                         object_layer_count = 0;
    // 此print_z处的实际层高（如可用）。
    coordf_t                    layer_height = 0.;
    // 应在该层打印裙边吗？
    // 层被标记为无限裙边即防尘罩。并非所有层都需要打印。
    bool                        has_skirt = false;
    // 该层是否有任何擦拭塔的挤出？
    // 由于支撑层可能与对象层交错，
    // 擦拭塔将仅对某些支撑层禁用。
    bool                        has_wipe_tower = false;
    // 擦拭塔分区数，以支持所需的工具切换数
    // 并支持此分区上方的擦拭塔分区。
    size_t                      wipe_tower_partitions = 0;
    coordf_t                    wipe_tower_layer_height = 0.;
    // 开始打印此层前要执行的自定义G-code（颜色更改、挤出机切换、暂停）。
    const CustomGCode::Item    *custom_gcode = nullptr;

    WipingExtrusions& wiping_extrusions() {
        m_wiping_extrusions.set_layer_tools_ptr(this);
        return m_wiping_extrusions;
    }

    // 混合丝材解析上下文（由ToolOrdering在collect_extruders期间设置）。
    const MixedFilamentManager *mixed_mgr    = nullptr;
    size_t                      num_physical = 0;
    // 来自打印设置的可选混合层节奏覆盖。
    float                       mixed_layer_height_a    = 0.f;
    float                       mixed_layer_height_b    = 0.f;
    float                       mixed_base_layer_height = 0.2f;
    const PrintObject          *current_object = nullptr;

private:
    // 通过此层的混合丝材管理器解析基于1的丝材ID。
    unsigned int resolve_mixed_1based(unsigned int filament_id) const;
    // 此对象保存将用于挤出机擦拭的挤出列表
    WipingExtrusions m_wiping_extrusions;
};

class ToolOrdering
{
public:
    ToolOrdering() = default;

    // 用于每个对象单独打印的情况
    // (print->config().print_sequence == PrintSequence::ByObject为真)。
    ToolOrdering(const PrintObject &object, unsigned int first_extruder, bool prime_multi_material = false);

    // 用于所有对象同时打印的情况。
    // (print->config().print_sequence == PrintSequence::ByObject为假)。
    ToolOrdering(const Print& print, unsigned int first_extruder, bool prime_multi_material = false);

    void                clear() {
        m_layer_tools.clear(); m_tool_order_cache.clear();
    }

    // 仅对非顺序打印有效：
    // 将自定义G-code的指针分配给相应的ToolOrdering::LayerTools。
    // 忽略在层上执行且对于该挤出机在该层以上不会打印的挤出机的颜色更改。
    // 如果单个层跨越多个事件，使用最后一个。
    void                assign_custom_gcodes(const Print &print);

    // 获取第一个打印的挤出机，包括挤出机初始区域，如果没有打印层则返回-1。
    unsigned int        first_extruder() const { return m_first_printing_extruder; }

    // 获取最后一个打印的挤出机，如果没有打印层则返回-1。
    unsigned int        last_extruder() const { return m_last_printing_extruder; }

    // 对于多材料打印，打印挤出机按其初始顺序排列。
    const std::vector<unsigned int>& all_extruders() const { return m_all_printing_extruders; }

    // 找到具有最接近print_z的LayerTools。
    const LayerTools&   tools_for_layer(coordf_t print_z) const;
    LayerTools&         tools_for_layer(coordf_t print_z) { return const_cast<LayerTools&>(std::as_const(*this).tools_for_layer(print_z)); }

    const LayerTools&   front()       const { return m_layer_tools.front(); }
    const LayerTools&   back()        const { return m_layer_tools.back(); }
    std::vector<LayerTools>::const_iterator begin() const { return m_layer_tools.begin(); }
    std::vector<LayerTools>::const_iterator end()   const { return m_layer_tools.end(); }
    bool                empty()       const { return m_layer_tools.empty(); }
    std::vector<LayerTools>& layer_tools() { return m_layer_tools; }
    bool                has_wipe_tower() const { return ! m_layer_tools.empty() && m_first_printing_extruder != (unsigned int)-1 && m_layer_tools.front().has_wipe_tower; }

private:
    void                initialize_layers(std::vector<coordf_t> &zs);
    void                collect_extruders(const PrintObject &object, const std::vector<std::pair<double, unsigned int>> &per_layer_extruder_switches);
    void                reorder_extruders(unsigned int last_extruder_id);
    // BBS
    void                reorder_extruders(std::vector<unsigned int> tool_order_layer0);
    void                fill_wipe_tower_partitions(const PrintConfig &config, coordf_t object_bottom_z, coordf_t max_layer_height);
    bool                insert_wipe_tower_extruder();
    void                mark_skirt_layers(const PrintConfig &config, coordf_t max_layer_height);
    void                collect_extruder_statistics(bool prime_multi_material);
    void                reorder_extruders_for_minimum_flush_volume();

    // BBS
    std::vector<unsigned int> generate_first_layer_tool_order(const Print& print);
    std::vector<unsigned int> generate_first_layer_tool_order(const PrintObject& object);
    void                      update_mixed_layer_height_settings();

    // 通过混合丝材管理器解析基于1的丝材ID。
    // 返回解析后的物理挤出机（基于1）。如果ID不是
    // 混合丝材或未设置管理器，则返回不变的输入。
    unsigned int resolve_mixed(unsigned int filament_id_1based,
                               int          layer_index,
                               float        layer_print_z = 0.f,
                               float        layer_height  = 0.f,
                               const PrintObject* current_object = nullptr) const;

    std::vector<LayerTools>    m_layer_tools;
    // 第一个打印挤出机，包括多材料初始序列。
    unsigned int               m_first_printing_extruder = (unsigned int)-1;
    // 最后一个打印挤出机。
    unsigned int               m_last_printing_extruder  = (unsigned int)-1;
    // 在m_layer_tools上挤出一些材料的所有挤出机。
    std::vector<unsigned int>  m_all_printing_extruders;
    std::unordered_map<uint32_t, std::vector<uint8_t>> m_tool_order_cache;
    const DynamicPrintConfig*  m_print_full_config = nullptr;
    const PrintConfig*         m_print_config_ptr = nullptr;
    const PrintObject*         m_print_object_ptr = nullptr;
    bool                       m_is_BBL_printer = false;
    // 混合丝材支持：指向管理器（由Print拥有）的指针和
    // 物理挤出机数量。
    const MixedFilamentManager* m_mixed_mgr    = nullptr;
    size_t                      m_num_physical  = 0;
    float                       m_mixed_layer_height_a    = 0.f;
    float                       m_mixed_layer_height_b    = 0.f;
    float                       m_mixed_base_layer_height = 0.2f;
};

} // namespace Slic3r

#endif /* slic3r_ToolOrdering_hpp_ */
