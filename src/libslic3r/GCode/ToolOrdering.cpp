#include "ExtrusionEntity.hpp"
#include "Print.hpp"
#include "ToolOrdering.hpp"
#include "Layer.hpp"
#include "ClipperUtils.hpp"
#include "ParameterUtils.hpp"

// #define SLIC3R_DEBUG

// 如果SLIC3R_DEBUG则使assert活动
#ifdef SLIC3R_DEBUG
    #define DEBUG
    #define _DEBUG
    #undef NDEBUG
#endif

#include <cassert>
#include <limits>
#include <algorithm>
#include <cmath>

#include <libslic3r.h>

namespace Slic3r {

const static bool g_wipe_into_objects = false;

namespace {

unsigned int resolve_mixed_with_layer_heights(const MixedFilamentManager *mixed_mgr,
                                              size_t                      num_physical,
                                              unsigned int                filament_id_1based,
                                              int                         layer_index,
                                              float                       layer_print_z,
                                              float                       layer_height,
                                              float                       layer_height_a,
                                              float                       layer_height_b,
                                              float                       base_layer_height,
                                              const PrintObject*          current_object = nullptr)
{
    if (!(mixed_mgr && mixed_mgr->is_mixed(filament_id_1based, num_physical)))
        return filament_id_1based;

    const MixedFilament *mixed_row = mixed_mgr->mixed_filament_from_id(filament_id_1based, num_physical);

    // Z方向梯度短路以下传统的A/B层循环：梯度路径需要
    // 在MixedFilamentManager::resolve内部执行每个（对象，层）运行时查找。
    const bool gradient_active =
        (mixed_row != nullptr && current_object != nullptr) &&
        (mixed_row->gradient_enabled && mixed_row->component_a != mixed_row->component_b) &&
        (layer_index > 0);

    if (mixed_row != nullptr && !mixed_row->custom && !gradient_active && (layer_height_a > 0.f || layer_height_b > 0.f)) {
        const float safe_base = std::max<float>(0.01f, base_layer_height);
        const int ratio_a = std::max(1, int(std::lround((layer_height_a > 0.f ? layer_height_a : safe_base) / safe_base)));
        const int ratio_b = std::max(1, int(std::lround((layer_height_b > 0.f ? layer_height_b : safe_base) / safe_base)));
        const int cycle   = ratio_a + ratio_b;

        if (cycle > 0) {
            if (mixed_row != nullptr) {
                const int pos = ((layer_index % cycle) + cycle) % cycle;
                return pos < ratio_a ? mixed_row->component_a : mixed_row->component_b;
            }
        }
    }

    return mixed_mgr->resolve(filament_id_1based, num_physical, layer_index, layer_print_z, layer_height, false, current_object);
}

bool has_grouped_manual_pattern(const MixedFilamentManager *mixed_mgr,
                                size_t                      num_physical,
                                unsigned int                filament_id_1based)
{
    if (!(mixed_mgr && mixed_mgr->is_mixed(filament_id_1based, num_physical)))
        return false;
    const MixedFilament *mixed_row = mixed_mgr->mixed_filament_from_id(filament_id_1based, num_physical);
    if (mixed_row == nullptr)
        return false;
    const std::string normalized = MixedFilamentManager::normalize_manual_pattern(mixed_row->manual_pattern);
    return normalized.find(',') != std::string::npos;
}

void append_unique_preserve_order(std::vector<unsigned int> &dst, unsigned int value)
{
    if (std::find(dst.begin(), dst.end(), value) == dst.end())
        dst.emplace_back(value);
}

bool internal_solid_infill_uses_sparse_filament(const PrintRegion &region, ExtrusionRole role)
{
    return role == erSolidInfill && std::abs(region.config().sparse_infill_density.value - 100.) < EPSILON;
}

unsigned int sparse_infill_filament_id_1based(const PrintRegion &region)
{
    return region.config().sparse_infill_filament.value;
}

unsigned int infill_filament_id_1based(const LayerTools &layer_tools, const PrintRegion &region, ExtrusionRole role)
{
    if (internal_solid_infill_uses_sparse_filament(region, role))
        return sparse_infill_filament_id_1based(region);
    return is_solid_infill(role) ? region.config().solid_infill_filament.value : sparse_infill_filament_id_1based(region);
}

unsigned int grouped_manual_pattern_mixed_filament_id_for_layer(const LayerTools& layer_tools,
                                                                unsigned int      configured_filament_id_1based)
{
    if (layer_tools.mixed_mgr == nullptr || layer_tools.num_physical == 0)
        return 0;

    if (has_grouped_manual_pattern(layer_tools.mixed_mgr, layer_tools.num_physical, configured_filament_id_1based))
        return configured_filament_id_1based;
    return 0;
}

unsigned int grouped_manual_pattern_infill_filament_1based(const LayerTools&  layer_tools,
                                                           const PrintRegion& region,
                                                           unsigned int       configured_filament_id_1based)
{
    const unsigned int grouped_id =
        grouped_manual_pattern_mixed_filament_id_for_layer(layer_tools, configured_filament_id_1based);
    if (grouped_id == 0)
        return 0;

    const int innermost_perimeter_index = std::max(0, region.config().wall_loops.value - 1);
    return layer_tools.mixed_mgr->resolve_perimeter(grouped_id,
                                                    layer_tools.num_physical,
                                                    layer_tools.layer_index,
                                                    innermost_perimeter_index,
                                                    float(layer_tools.print_z),
                                                    float(layer_tools.layer_height),
                                                    false,
                                                    layer_tools.current_object);
}

void remove_duplicates_preserve_order(std::vector<unsigned int> &values)
{
    std::vector<unsigned int> ordered;
    ordered.reserve(values.size());
    for (unsigned int value : values)
        append_unique_preserve_order(ordered, value);
    values = std::move(ordered);
}

} // namespace


// 最短哈密顿路径问题
static std::vector<unsigned int> solve_extruder_order(const std::vector<std::vector<float>>& wipe_volumes, std::vector<unsigned int> all_extruders, std::optional<unsigned int> start_extruder_id)
{
    bool add_start_extruder_flag = false;

    if (start_extruder_id) {
        auto start_iter = std::find(all_extruders.begin(), all_extruders.end(), start_extruder_id);
        if (start_iter == all_extruders.end())
            all_extruders.insert(all_extruders.begin(), *start_extruder_id), add_start_extruder_flag = true;
        else
            std::swap(*all_extruders.begin(), *start_iter);
    }
    else {
        start_extruder_id = all_extruders.front();
    }

    unsigned int iterations = (1 << all_extruders.size());
    unsigned int final_state = iterations - 1;
    std::vector<std::vector<float>>cache(iterations, std::vector<float>(all_extruders.size(),0x7fffffff));
    std::vector<std::vector<int>>prev(iterations, std::vector<int>(all_extruders.size(), -1));
    cache[1][0] = 0.;
    for (unsigned int state = 0; state < iterations; ++state) {
        if (state & 1) {
            for (unsigned int target = 0; target < all_extruders.size(); ++target) {
                if (state >> target & 1) {
                    for (unsigned int mid_point = 0; mid_point < all_extruders.size(); ++mid_point) {
                        if(state>>mid_point&1){
                            auto tmp = cache[state - (1 << target)][mid_point] + wipe_volumes[all_extruders[mid_point]][all_extruders[target]];
                            if (cache[state][target] >tmp) {
                                cache[state][target] = tmp;
                                prev[state][target] = mid_point;
                            }
                        }
                    }
                }
            }
        }
    }

    //获取结果
    float cost = std::numeric_limits<float>::max();
    int final_dst =0;
    for (unsigned int dst = 0; dst < all_extruders.size(); ++dst) {
        if (all_extruders[dst] != start_extruder_id && cost > cache[final_state][dst]) {
            cost = cache[final_state][dst];
            final_dst = dst;
        }
    }

    std::vector<unsigned int>path;
    unsigned int curr_state = final_state;
    int curr_point = final_dst;
    while (curr_point != -1) {
        path.emplace_back(all_extruders[curr_point]);
        auto mid_point = prev[curr_state][curr_point];
        curr_state -= (1 << curr_point);
        curr_point = mid_point;
    };

    if (add_start_extruder_flag)
        path.pop_back();

    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<unsigned int> get_extruders_order(const std::vector<std::vector<float>> &wipe_volumes, std::vector<unsigned int> all_extruders, std::optional<unsigned int>start_extruder_id)
{
#define USE_DP_OPTIMIZE
#ifdef USE_DP_OPTIMIZE
    return solve_extruder_order(wipe_volumes, all_extruders, start_extruder_id);
#else
if (all_extruders.size() > 1) {
        int begin_index = 0;
        auto iter = std::find(all_extruders.begin(), all_extruders.end(), start_extruder_id);
        if (iter != all_extruders.end()) {
            for (int i = 0; i < all_extruders.size(); ++i) {
                if (all_extruders[i] == start_extruder_id) {
                    std::swap(all_extruders[i], all_extruders[0]);
                }
            }
            begin_index = 1;
        }

        std::pair<float, std::vector<unsigned int>> volumes_to_extruder_order;
        volumes_to_extruder_order.first = 10000 * all_extruders.size();
        std::sort(all_extruders.begin() + begin_index, all_extruders.end());
        do {
            float flush_volume = 0;
            for (int i = 0; i < all_extruders.size() - 1; ++i) {
                flush_volume += wipe_volumes[all_extruders[i]][all_extruders[i + 1]];
            }
            if (flush_volume < volumes_to_extruder_order.first) {
                volumes_to_extruder_order = std::pair(flush_volume, all_extruders);
            }
        } while (std::next_permutation(all_extruders.begin() + begin_index, all_extruders.end()));

        if (volumes_to_extruder_order.second.size() > 0)
            return volumes_to_extruder_order.second;
    }
    return all_extruders;

#endif // OPTIMIZE
}

// 如果挤出机a在b之前，则返回true（b不必存在）。否则返回false。
bool LayerTools::is_extruder_order(unsigned int a, unsigned int b) const
{
    if (a == b)
        return false;

    for (auto extruder : extruders) {
        if (extruder == a)
            return true;
        if (extruder == b)
            return false;
    }

    return false;
}

// 通过此层的混合丝材管理器解析基于1的丝材ID。
unsigned int LayerTools::resolve_mixed_1based(unsigned int filament_id) const
{
    return resolve_mixed_with_layer_heights(mixed_mgr,
                                            num_physical,
                                            filament_id,
                                            this->layer_index,
                                            float(this->print_z),
                                            float(this->layer_height),
                                            mixed_layer_height_a,
                                            mixed_layer_height_b,
                                            mixed_base_layer_height,
                                            this->current_object);
}

// 从区域返回基于零的挤出机，如果覆盖则返回extruder_override。
unsigned int LayerTools::wall_filament(const PrintRegion &region) const
{
    assert(region.config().wall_filament.value > 0);
    unsigned int id = (this->extruder_override == 0) ? region.config().wall_filament.value : this->extruder_override;
    return resolve_mixed_1based(id) - 1;
}

unsigned int LayerTools::sparse_infill_filament(const PrintRegion &region) const
{
    assert(region.config().wall_filament.value > 0);
    unsigned int id = (this->extruder_override == 0) ? sparse_infill_filament_id_1based(region) : this->extruder_override;
    const unsigned int grouped = grouped_manual_pattern_infill_filament_1based(*this, region, id);
    return ((grouped != 0) ? grouped : resolve_mixed_1based(id)) - 1;
}

unsigned int LayerTools::solid_infill_filament(const PrintRegion &region) const
{
    assert(region.config().solid_infill_filament.value > 0);
    unsigned int id = (this->extruder_override == 0) ? region.config().solid_infill_filament.value : this->extruder_override;
    const unsigned int grouped = grouped_manual_pattern_infill_filament_1based(*this, region, id);
    return ((grouped != 0) ? grouped : resolve_mixed_1based(id)) - 1;
}

// 返回应使用此eec打印的基于零的挤出机，根据PrintRegion配置或挤出机覆盖（如被覆盖）。
unsigned int LayerTools::extruder(const ExtrusionEntityCollection &extrusions, const PrintRegion &region) const
{
    assert(region.config().wall_filament.value > 0);
    assert(region.config().sparse_infill_filament.value > 0);
    assert(region.config().solid_infill_filament.value > 0);
    if (extrusions.has_infill()) {
        const ExtrusionRole role = extrusions.entities.empty() ? erNone : extrusions.entities.front()->role();
        if (internal_solid_infill_uses_sparse_filament(region, role))
            return sparse_infill_filament(region);
        return is_solid_infill(role) ? solid_infill_filament(region) : sparse_infill_filament(region);
    }
    return wall_filament(region);
}

static double calc_max_layer_height(const PrintConfig &config, double max_object_layer_height)
{
    double max_layer_height = std::numeric_limits<double>::max();
    for (size_t i = 0; i < config.nozzle_diameter.values.size(); ++ i) {
        double mlh = config.max_layer_height.values[i];
        if (mlh == 0.)
            mlh = 0.75 * config.nozzle_diameter.values[i];
        max_layer_height = std::min(max_layer_height, mlh);
    }
    // Prusa3D Fast（0.35mm层高）打印配置文件设置的层高高于喷嘴通常允许的值。
    // 这是一个hack，通过增加挤出宽度来实现。参见GH #3919。
    return std::max(max_layer_height, max_object_layer_height);
}

// 用于每个对象单独打印的情况
// (print->config().print_sequence == PrintSequence::ByObject为真)。
ToolOrdering::ToolOrdering(const PrintObject &object, unsigned int first_extruder, bool prime_multi_material)
{
    m_is_BBL_printer = object.print()->is_BBL_printer();
    m_print_full_config = &object.print()->full_print_config();
    m_print_config_ptr = &object.print()->config();
    m_print_object_ptr = &object;
    // 混合丝材支持。
    m_mixed_mgr   = &object.print()->mixed_filament_manager();
    m_num_physical = object.print()->config().filament_diameter.size();
    update_mixed_layer_height_settings();
    if (object.layers().empty())
        return;

    // 为单个对象初始化打印层。
    {
        std::vector<coordf_t> zs;
        zs.reserve(zs.size() + object.layers().size() + object.support_layers().size());
        for (auto layer : object.layers())
            zs.emplace_back(layer->print_z);
        for (auto layer : object.support_layers())
            zs.emplace_back(layer->print_z);
        this->initialize_layers(zs);
    }
    double max_layer_height = calc_max_layer_height(object.print()->config(), object.config().layer_height);

    // 收集打印层所需的挤出机。
    this->collect_extruders(object, std::vector<std::pair<double, unsigned int>>());

    // BBS
    // 重新排序挤出机以最小化工具切换。
    std::vector<unsigned int> first_layer_tool_order;
    if (first_extruder == (unsigned int) -1) {
        first_layer_tool_order = generate_first_layer_tool_order(object);
    }

    if (!first_layer_tool_order.empty()) {
        this->reorder_extruders(first_layer_tool_order);
    } else {
        this->reorder_extruders(first_extruder);
    }

    this->fill_wipe_tower_partitions(object.print()->config(), object.layers().front()->print_z - object.layers().front()->height, max_layer_height);

    this->collect_extruder_statistics(prime_multi_material);

    this->mark_skirt_layers(object.print()->config(), max_layer_height);
}

bool ToolOrdering::insert_wipe_tower_extruder()
{
    if(!m_print_config_ptr->enable_prime_tower)
        return false;
    // 如果wipe_tower_extruder设置为非零，我们必须确保挤出机在列表中。
    bool changed = false;
    if (m_print_config_ptr->wipe_tower_filament != 0) {
        for (LayerTools& lt : m_layer_tools) {
            if (lt.wipe_tower_partitions > 0) {
                lt.extruders.emplace_back(m_print_config_ptr->wipe_tower_filament - 1);
                sort_remove_duplicates(lt.extruders);
                changed = true;
            }
        }
    }
    return changed;
}

// 用于所有对象同时打印的情况。
// (print->config().print_sequence == PrintSequence::ByObject为假)。
ToolOrdering::ToolOrdering(const Print &print, unsigned int first_extruder, bool prime_multi_material)
{
    m_is_BBL_printer = print.is_BBL_printer();
    m_print_full_config = &print.full_print_config();
    m_print_config_ptr = &print.config();
    // 混合丝材支持。
    m_mixed_mgr   = &print.mixed_filament_manager();
    m_num_physical = print.config().filament_diameter.size();
    update_mixed_layer_height_settings();

    // 初始化所有对象和所有层的打印层。
    coordf_t object_bottom_z = 0.;
    coordf_t max_layer_height = 0.;
    {
        std::vector<coordf_t> zs;
        for (auto object : print.objects()) {
            zs.reserve(zs.size() + object->layers().size() + object->support_layers().size());
            for (auto layer : object->layers())
                zs.emplace_back(layer->print_z);
            for (auto layer : object->support_layers())
                zs.emplace_back(layer->print_z);

            // 找到非空的第一个对象层并保存其print_z
            for (const Layer* layer : object->layers())
                if (layer->has_extrusions()) {
                    object_bottom_z = layer->print_z - layer->height;
                    break;
                }

            max_layer_height = std::max(max_layer_height, object->config().layer_height.value);
        }
        this->initialize_layers(zs);
    }
    max_layer_height = calc_max_layer_height(print.config(), max_layer_height);

    // 使用Model::custom_gcode_per_print_z的挤出机切换来覆盖打印对象的挤出机。
    // 仅当所有对象配置为使用单个挤出机打印时才这样做。
    std::vector<std::pair<double, unsigned int>> per_layer_extruder_switches;

    // BBS
    if (auto num_physical = unsigned(print.config().filament_diameter.size());
        num_physical > 1 && print.object_extruders().size() == 1 && // 当前Print的配置是CustomGCode::MultiAsSingle
        //BBS: 用当前板块自定义gcode替换模型自定义gcode
        print.model().get_curr_plate_custom_gcodes().mode == CustomGCode::MultiAsSingle) {
        // 在具有1个以上挤出机（或单挤出机多材料）的打印机上打印单个挤出机拼盘。
        // 可能有可用的自定义逐层工具更改。
        const size_t num_filaments = (m_mixed_mgr == nullptr) ? num_physical : m_mixed_mgr->total_filaments(num_physical);
        per_layer_extruder_switches = custom_tool_changes(print.model().get_curr_plate_custom_gcodes(), num_filaments);
    }

    // 收集打印层所需的挤出机。
    for (auto object : print.objects())
        this->collect_extruders(*object, per_layer_extruder_switches);

    // 重新排序挤出机以最小化工具切换。
    std::vector<unsigned int> first_layer_tool_order;
    if (first_extruder == (unsigned int)-1) {
        first_layer_tool_order = generate_first_layer_tool_order(print);
    }

    if (!first_layer_tool_order.empty()) {
        this->reorder_extruders(first_layer_tool_order);
    }
    else {
        this->reorder_extruders(first_extruder);
    }

    this->fill_wipe_tower_partitions(print.config(), object_bottom_z, max_layer_height);

    /*if (prime_multi_material) {
        std::map<unsigned int, int> extrudeCount;
        for (const LayerTools& lt : m_layer_tools) {
            for (unsigned int currentExtruder : lt.extruders) {
                extrudeCount[currentExtruder]++;
            }
        }

        unsigned int maxExtrude = -1;
        int maxCount = 0;
        for (auto& itPair : extrudeCount) {
            if (itPair.second > maxCount && !m_print_config_ptr->filament_soluble.get_at(itPair.first)) {
                maxCount = itPair.second;
                maxExtrude = itPair.first;
            }
        }
        const_cast<PrintConfig*>(m_print_config_ptr)->wipe_tower_filament.setInt(maxExtrude + 1);
    }*/

    if (this->insert_wipe_tower_extruder()) {
        // 现在将基于0的列表再次转换为基于1的，因为这就是reorder_extruder所期望的。
        for (LayerTools& lt : m_layer_tools) {
            for (auto& extruder : lt.extruders)
                ++extruder;
        }
        this->reorder_extruders(first_extruder);
        this->fill_wipe_tower_partitions(print.config(), object_bottom_z, max_layer_height);
    }

    this->collect_extruder_statistics(prime_multi_material);

    this->mark_skirt_layers(print.config(), max_layer_height);
}

void ToolOrdering::update_mixed_layer_height_settings()
{
    const PrintConfig *cfg = m_print_config_ptr;
    if (cfg == nullptr && m_print_object_ptr != nullptr)
        cfg = &m_print_object_ptr->print()->config();

    m_mixed_layer_height_a = 0.f;
    m_mixed_layer_height_b = 0.f;
    if (m_print_full_config != nullptr &&
        m_print_full_config->has("mixed_color_layer_height_a") &&
        m_print_full_config->has("mixed_color_layer_height_b")) {
        m_mixed_layer_height_a = float(m_print_full_config->opt_float("mixed_color_layer_height_a"));
        m_mixed_layer_height_b = float(m_print_full_config->opt_float("mixed_color_layer_height_b"));
    } else if (cfg != nullptr) {
        m_mixed_layer_height_a = cfg->mixed_color_layer_height_a.value;
        m_mixed_layer_height_b = cfg->mixed_color_layer_height_b.value;
    }

    float base_height = 0.2f;
    if (m_print_object_ptr != nullptr)
        base_height = float(m_print_object_ptr->config().layer_height.value);
    else if (m_print_full_config != nullptr && m_print_full_config->has("layer_height"))
        base_height = float(m_print_full_config->opt_float("layer_height"));

    m_mixed_base_layer_height = std::max<float>(0.01f, base_height);
}

static void apply_first_layer_order(const DynamicPrintConfig* config, std::vector<unsigned int>& tool_order) {
    const ConfigOptionInts* first_layer_print_sequence_op = config->option<ConfigOptionInts>("first_layer_print_sequence");
    if (first_layer_print_sequence_op) {
        const std::vector<int>& print_sequence_1st = first_layer_print_sequence_op->values;
        if (print_sequence_1st.size() >= tool_order.size()) {
            std::sort(tool_order.begin(), tool_order.end(), [&print_sequence_1st](int lh, int rh) {
                auto lh_it = std::find(print_sequence_1st.begin(), print_sequence_1st.end(), lh);
                auto rh_it = std::find(print_sequence_1st.begin(), print_sequence_1st.end(), rh);

                if (lh_it == print_sequence_1st.end() || rh_it == print_sequence_1st.end())
                    return false;

                return lh_it < rh_it;
            });
        }
    }
}

// BBS
std::vector<unsigned int> ToolOrdering::generate_first_layer_tool_order(const Print& print)
{
    std::vector<unsigned int> tool_order;
    int initial_extruder_id = -1;
    std::map<int, double> min_areas_per_extruder;

    for (auto object : print.objects()) {
        auto first_layer = object->get_layer(0);
        for (auto layerm : first_layer->regions()) {
            int extruder_id = layerm->region().config().option("wall_filament")->getInt();

            for (auto expoly : layerm->raw_slices) {
                const double nozzle_diameter = print.config().nozzle_diameter.get_at(0);
                const coordf_t initial_layer_line_width = print.config().get_abs_value("initial_layer_line_width", nozzle_diameter);

                if (offset_ex(expoly, -0.2 * scale_(initial_layer_line_width)).empty())
                    continue;

                double contour_area = expoly.contour.area();
                auto iter = min_areas_per_extruder.find(extruder_id);
                if (iter == min_areas_per_extruder.end()) {
                    min_areas_per_extruder.insert({ extruder_id, contour_area });
                }
                else {
                    if (contour_area < min_areas_per_extruder.at(extruder_id)) {
                        min_areas_per_extruder[extruder_id] = contour_area;
                    }
                }
            }
        }
    }

    double max_minimal_area = 0.;
    for (auto ape : min_areas_per_extruder) {
        auto iter = tool_order.begin();
        for (; iter != tool_order.end(); iter++) {
            if (min_areas_per_extruder.at(*iter) < min_areas_per_extruder.at(ape.first))
                break;
        }

        tool_order.insert(iter, ape.first);
    }

    apply_first_layer_order(m_print_full_config, tool_order);

    return tool_order;
}

std::vector<unsigned int> ToolOrdering::generate_first_layer_tool_order(const PrintObject& object)
{
    std::vector<unsigned int> tool_order;
    int initial_extruder_id = -1;
    std::map<int, double> min_areas_per_extruder;
    auto first_layer = object.get_layer(0);
    for (auto layerm : first_layer->regions()) {
        int extruder_id = layerm->region().config().option("wall_filament")->getInt();
        for (auto expoly : layerm->raw_slices) {
            const double nozzle_diameter = object.print()->config().nozzle_diameter.get_at(0);
            const coordf_t line_width = object.config().get_abs_value("line_width", nozzle_diameter);

            if (offset_ex(expoly, -0.2 * scale_(line_width)).empty())
                continue;

            double contour_area = expoly.contour.area();
            auto iter = min_areas_per_extruder.find(extruder_id);
            if (iter == min_areas_per_extruder.end()) {
                min_areas_per_extruder.insert({ extruder_id, contour_area });
            }
            else {
                if (contour_area < min_areas_per_extruder.at(extruder_id)) {
                    min_areas_per_extruder[extruder_id] = contour_area;
                }
            }
        }
    }

    double max_minimal_area = 0.;
    for (auto ape : min_areas_per_extruder) {
        auto iter = tool_order.begin();
        for (; iter != tool_order.end(); iter++) {
            if (min_areas_per_extruder.at(*iter) < min_areas_per_extruder.at(ape.first))
                break;
        }

        tool_order.insert(iter, ape.first);
    }

    apply_first_layer_order(m_print_full_config, tool_order);

    return tool_order;
}

void ToolOrdering::initialize_layers(std::vector<coordf_t> &zs)
{
    sort_remove_duplicates(zs);
    // 合并数值非常接近的Z值。
    for (size_t i = 0; i < zs.size();) {
        // 找到具有大致相同print_z的最后一层。
        size_t j = i + 1;
        coordf_t zmax = zs[i] + EPSILON;
        for (; j < zs.size() && zs[j] <= zmax; ++ j) ;
        // 为具有几乎相等print_z的层集合分配平均print_z。
        m_layer_tools.emplace_back(LayerTools(0.5 * (zs[i] + zs[j-1])));
        i = j;
    }
}

// 收集打印层所需的挤出机。
void ToolOrdering::collect_extruders(const PrintObject &object, const std::vector<std::pair<double, unsigned int>> &per_layer_extruder_switches)
{
    for (LayerTools &layer_tools : m_layer_tools) {
        layer_tools.mixed_mgr                = m_mixed_mgr;
        layer_tools.num_physical             = m_num_physical;
        layer_tools.mixed_layer_height_a     = m_mixed_layer_height_a;
        layer_tools.mixed_layer_height_b     = m_mixed_layer_height_b;
        layer_tools.mixed_base_layer_height  = m_mixed_base_layer_height;
        // 为此collect_extruders调用期间设置每对象上下文。
        // 在下面的循环后重置，以便不相关的调用者看到nullptr。
        layer_tools.current_object           = &object;
    }

    // 收集支撑挤出机。
    for (auto support_layer : object.support_layers()) {
        LayerTools   &layer_tools = this->tools_for_layer(support_layer->print_z);
        layer_tools.layer_height = support_layer->height;
        ExtrusionRole role = support_layer->support_fills.role();
        bool         has_support        = role == erMixed || role == erSupportMaterial || role == erSupportTransition;
        bool         has_interface      = role == erMixed || role == erSupportMaterialInterface;
        unsigned int extruder_support   = resolve_mixed(object.config().support_filament.value,
                                                        layer_tools.layer_index,
                                                        float(support_layer->print_z),
                                                        float(support_layer->height),
                                                        &object);
        unsigned int extruder_interface = resolve_mixed(object.config().support_interface_filament.value,
                                                        layer_tools.layer_index,
                                                        float(support_layer->print_z),
                                                        float(support_layer->height),
                                                        &object);
        if (has_support)
            layer_tools.extruders.push_back(extruder_support);
        if (has_interface)
            layer_tools.extruders.push_back(extruder_interface);
        if (has_support || has_interface) {
            layer_tools.has_support = true;
            layer_tools.wiping_extrusions().is_support_overriddable_and_mark(role, object);
        }
    }

    // 挤出机覆盖按print_z排序。
    std::vector<std::pair<double, unsigned int>>::const_iterator it_per_layer_extruder_override;
    it_per_layer_extruder_override = per_layer_extruder_switches.begin();
    unsigned int extruder_override = 0;

    // BBS: 收集对象壁的第一层挤出机，供brim生成器使用
    int layerCount = 0;
    std::vector<int> firstLayerExtruders;
    firstLayerExtruders.clear();

    // 收集对象挤出机。
    for (auto layer : object.layers()) {
        LayerTools &layer_tools = this->tools_for_layer(layer->print_z);
        // 存储顺序层索引和混合丝材上下文以供解析。
        layer_tools.layer_index       = layerCount;
        layer_tools.object_layer_count = int(object.layers().size());
        layer_tools.layer_height      = layer->height;

        // 用下一个挤出机覆盖覆盖
        for (; it_per_layer_extruder_override != per_layer_extruder_switches.end() && it_per_layer_extruder_override->first < layer->print_z + EPSILON; ++ it_per_layer_extruder_override)
            extruder_override = (int)it_per_layer_extruder_override->second;

        // 存储当前挤出机覆盖（如果未覆盖则设为零），以便layer_tools.wiping_extrusions().is_overridable_and_mark()使用它。
        layer_tools.extruder_override = extruder_override;

        // 打印此对象层需要哪些挤出机？
        for (const LayerRegion *layerm : layer->regions()) {
            const PrintRegion &region = layerm->region();

            if (! layerm->perimeters.entities.empty()) {
                bool something_nonoverriddable = true;

                if (m_print_config_ptr) { // 在这种情况下print->config().print_sequence != PrintSequence::ByObject（参见ToolOrdering构造函数）
                    something_nonoverriddable = false;
                    for (const auto& eec : layerm->perimeters.entities) // 让我们检查是否有不可覆盖的实体
                        if (!layer_tools.wiping_extrusions().is_overriddable_and_mark(dynamic_cast<const ExtrusionEntityCollection&>(*eec), *m_print_config_ptr, object, region))
                            something_nonoverriddable = true;
                }

                if (something_nonoverriddable){
                    const unsigned int configured_wall = (extruder_override == 0) ? region.config().wall_filament.value : extruder_override;
                    unsigned int       wall_ext        = resolve_mixed(configured_wall, layerCount, float(layer->print_z), float(layer->height), &object);
                    const unsigned int grouped_id =
                        grouped_manual_pattern_mixed_filament_id_for_layer(layer_tools, configured_wall);
                    if (grouped_id != 0) {
                        const std::vector<unsigned int> ordered =
                            m_mixed_mgr->ordered_perimeter_extruders(grouped_id,
                                                                     m_num_physical,
                                                                     layerCount,
                                                                     float(layer->print_z),
                                                                     float(layer->height));
                        if (!ordered.empty()) {
                            if (ordered.size() >= 2)
                                layer_tools.preserve_extruder_order = true;
                            for (unsigned int extruder_id : ordered) {
                                layer_tools.extruders.emplace_back(extruder_id);
                                if (layerCount == 0 &&
                                    std::find(firstLayerExtruders.begin(), firstLayerExtruders.end(), int(extruder_id)) == firstLayerExtruders.end())
                                    firstLayerExtruders.emplace_back(int(extruder_id));
                            }
                        } else {
                            layer_tools.extruders.emplace_back(wall_ext);
                            if (layerCount == 0)
                                firstLayerExtruders.emplace_back(wall_ext);
                        }
                    } else {
                        layer_tools.extruders.emplace_back(wall_ext);
                        if (layerCount == 0)
                            firstLayerExtruders.emplace_back(wall_ext);
                    }
                }

                layer_tools.has_object = true;
            }

            bool has_sparse_infill = false;
            bool has_solid_infill  = false;
            bool something_nonoverriddable = false;
            for (const ExtrusionEntity *ee : layerm->fills.entities) {
                // fill表示单个岛的填充挤出。
                const auto *fill = dynamic_cast<const ExtrusionEntityCollection*>(ee);
                ExtrusionRole role = fill->entities.empty() ? erNone : fill->entities.front()->role();
                if (internal_solid_infill_uses_sparse_filament(region, role))
                    has_sparse_infill = true;
                else if (is_solid_infill(role))
                    has_solid_infill = true;
                else if (role != erNone)
                    has_sparse_infill = true;

                if (m_print_config_ptr) {
                    if (! layer_tools.wiping_extrusions().is_overriddable_and_mark(*fill, *m_print_config_ptr, object, region))
                        something_nonoverriddable = true;
                }
            }

            if (something_nonoverriddable || !m_print_config_ptr) {
                if (extruder_override == 0) {
                    if (has_solid_infill) {
                        layer_tools.extruders.emplace_back(layer_tools.solid_infill_filament(region) + 1);
                    }
                    if (has_sparse_infill) {
                        layer_tools.extruders.emplace_back(layer_tools.sparse_infill_filament(region) + 1);
                    }
                } else if (has_solid_infill || has_sparse_infill) {
                    layer_tools.extruders.emplace_back(resolve_mixed(extruder_override,
                                                                      layerCount,
                                                                      float(layer->print_z),
                                                                      float(layer->height),
                                                                      &object));
                }
            }
            if (has_solid_infill || has_sparse_infill)
                layer_tools.has_object = true;
        }
        layerCount++;
    }

    sort_remove_duplicates(firstLayerExtruders);
    const_cast<PrintObject&>(object).object_first_layer_wall_extruders = firstLayerExtruders;

    for (auto& layer : m_layer_tools) {
        if (layer.preserve_extruder_order)
            remove_duplicates_preserve_order(layer.extruders);
        else
            sort_remove_duplicates(layer.extruders);

        // 确保每个对象层都有一些工具（例如，高擦拭对象将导致空的挤出机向量）
        if (layer.extruders.empty() && layer.has_object)
            layer.extruders.emplace_back(0); // 0="不关心"挤出机 - 将在reorder_extruders中处理

        // 现在此对象的收集已完成，重置每对象上下文。
        layer.current_object = nullptr;
    }
}

// 重新排序挤出机以最小化层更改。
void ToolOrdering::reorder_extruders(unsigned int last_extruder_id)
{
    if (m_layer_tools.empty())
        return;

    if (last_extruder_id == (unsigned int)-1) {
        // 初始打印挤出机尚未确定。
        // 用打印使用的第一个非零挤出机id初始化last_extruder_id。
        last_extruder_id = 0;
        for (size_t i = 0; i < m_layer_tools.size() && last_extruder_id == 0; ++ i) {
            const LayerTools &lt = m_layer_tools[i];
            for (unsigned int extruder_id : lt.extruders)
                if (extruder_id > 0) {
                    last_extruder_id = extruder_id;
                    break;
                }
        }
        if (last_extruder_id == 0)
            // 没有要挤出的东西。
            return;
    } else
        // 基于1的索引
        ++ last_extruder_id;

    for (LayerTools &lt : m_layer_tools) {
        if (lt.extruders.empty())
            continue;
        if (lt.extruders.size() == 1 && lt.extruders.front() == 0)
            lt.extruders.front() = last_extruder_id;
        else {
            if (lt.extruders.front() == 0)
                // 弹出"不关心"挤出机，"不关心"区域将与下一个合并。
                lt.extruders.erase(lt.extruders.begin());
            if (lt.preserve_extruder_order) {
                last_extruder_id = lt.extruders.back();
                continue;
            }
            // 重新排序挤出机以从最后一个开始。
            for (size_t i = 1; i < lt.extruders.size(); ++ i)
                if (lt.extruders[i] == last_extruder_id) {
                    // 将最后一个挤出机移到前面。
                    memmove(lt.extruders.data() + 1, lt.extruders.data(), i * sizeof(unsigned int));
                    lt.extruders.front() = last_extruder_id;
                    break;
                }

            if (lt == m_layer_tools[0]) {
                // 在第一层使用擦拭塔时，优先在开头使用可溶性挤出机，
                // 这样它就不会被擦拭在第一层上。
                if (m_print_config_ptr && m_print_config_ptr->enable_prime_tower) {
                    for (size_t i = 0; i<lt.extruders.size(); ++i)
                        if (m_print_config_ptr->filament_soluble.get_at(lt.extruders[i]-1)) { // 基于1的...
                            std::swap(lt.extruders[i], lt.extruders.front());
                            break;
                        }
                }

                // 然后，如果我们指定了工具顺序，现在应用它
                apply_first_layer_order(m_print_full_config, lt.extruders);
            }
        }
        last_extruder_id = lt.extruders.back();
    }

    // 重新索引挤出机，使它们基于0，而不是基于1。
    for (LayerTools &lt : m_layer_tools)
        for (unsigned int &extruder_id : lt.extruders) {
            assert(extruder_id > 0);
            -- extruder_id;
        }

    // 为最小冲洗体积重新排序挤出机
    reorder_extruders_for_minimum_flush_volume();
}

// BBS
void ToolOrdering::reorder_extruders(std::vector<unsigned int> tool_order_layer0)
{
    if (m_layer_tools.empty())
        return;

    if (tool_order_layer0.empty())
        return;

    // 重新排序第一层的挤出机
    {
        LayerTools& lt = m_layer_tools[0];
        if (!lt.preserve_extruder_order) {
            std::vector<unsigned int> layer0_extruders = lt.extruders;
            lt.extruders.clear();
            for (unsigned int extruder_id : tool_order_layer0) {
                auto iter = std::find(layer0_extruders.begin(), layer0_extruders.end(), extruder_id);
                if (iter != layer0_extruders.end()) {
                    lt.extruders.push_back(extruder_id);
                    *iter = (unsigned int)-1;
                }
            }

            for (unsigned int extruder_id : layer0_extruders) {
                if (extruder_id == 0)
                    continue;

                if (extruder_id != (unsigned int)-1)
                    lt.extruders.push_back(extruder_id);
            }

            // 所有挤出机都是零
            if (lt.extruders.empty()) {
                lt.extruders.push_back(tool_order_layer0[0]);
            }
        }
    }

    int last_extruder_id = m_layer_tools[0].extruders.back();
    for (int i = 1; i < m_layer_tools.size(); i++) {
        LayerTools& lt = m_layer_tools[i];

        if (lt.extruders.empty())
            continue;
        if (lt.extruders.size() == 1 && lt.extruders.front() == 0)
            lt.extruders.front() = last_extruder_id;
        else {
            if (lt.extruders.front() == 0)
                // 弹出"不关心"挤出机，"不关心"区域将与下一个合并。
                lt.extruders.erase(lt.extruders.begin());
            if (lt.preserve_extruder_order) {
                last_extruder_id = lt.extruders.back();
                continue;
            }
            // 重新排序挤出机以从最后一个开始。
            for (size_t i = 1; i < lt.extruders.size(); ++i)
                if (lt.extruders[i] == last_extruder_id) {
                    // 将最后一个挤出机移到前面。
                    memmove(lt.extruders.data() + 1, lt.extruders.data(), i * sizeof(unsigned int));
                    lt.extruders.front() = last_extruder_id;
                    break;
                }
        }
        last_extruder_id = lt.extruders.back();
    }

    // 重新索引挤出机，使它们基于0，而不是基于1。
    for (LayerTools& lt : m_layer_tools)
        for (unsigned int& extruder_id : lt.extruders) {
            assert(extruder_id > 0);
            --extruder_id;
        }

    // 为最小冲洗体积重新排序挤出机
    reorder_extruders_for_minimum_flush_volume();
}

void ToolOrdering::fill_wipe_tower_partitions(const PrintConfig &config, coordf_t object_bottom_z, coordf_t max_layer_height)
{
    if (m_layer_tools.empty())
        return;

    // 计算每层的最小工具更改次数。
    size_t last_extruder = size_t(-1);
    for (LayerTools &lt : m_layer_tools) {
        lt.wipe_tower_partitions = lt.extruders.size();
        if (! lt.extruders.empty()) {
            if (last_extruder == size_t(-1) || last_extruder == lt.extruders.front())
                // 此层上的第一个挤出机与当前挤出机相同，无需进行初始工具更改。
                -- lt.wipe_tower_partitions;
            last_extruder = lt.extruders.back();
        }
    }

    // 将擦拭塔分区向下传播，以支持下层对上层分区的支撑。
    for (int i = int(m_layer_tools.size()) - 2; i >= 0; -- i)
        m_layer_tools[i].wipe_tower_partitions = std::max(m_layer_tools[i + 1].wipe_tower_partitions, m_layer_tools[i].wipe_tower_partitions);

    //FIXME 这是一个让事情运转起来的hack。
    for (LayerTools &lt : m_layer_tools)
        lt.has_wipe_tower = (lt.has_object && (config.timelapse_type == TimelapseType::tlSmooth || lt.wipe_tower_partitions > 0))
            || lt.print_z < object_bottom_z + EPSILON;

    // 测试是否有支撑，插入额外的擦拭塔层以填充支撑分离间隙。
    for (size_t i = 0; i + 1 < m_layer_tools.size(); ++ i) {
        const LayerTools &lt      = m_layer_tools[i];
        const LayerTools &lt_next = m_layer_tools[i + 1];
        if (lt.print_z < object_bottom_z + EPSILON && lt_next.print_z >= object_bottom_z + EPSILON) {
            // lt是最后一个支撑层。找到第一个对象层。
            size_t j = i + 1;
            for (; j < m_layer_tools.size() && ! m_layer_tools[j].has_wipe_tower; ++ j);
            if (j < m_layer_tools.size()) {
                const LayerTools &lt_object = m_layer_tools[j];
                coordf_t gap = lt_object.print_z - lt.print_z;
                assert(gap > 0.f);
                if (gap > max_layer_height + EPSILON) {
                    // 在lh.print_z和lt_object.print_z之间插入一个额外的擦拭塔层。
                    LayerTools lt_new(0.5f * (lt.print_z + lt_object.print_z));
                    // 找到lt_new上方的第一层。
                    for (j = i + 1; j < m_layer_tools.size() && m_layer_tools[j].print_z < lt_new.print_z - EPSILON; ++ j);
                    if (std::abs(m_layer_tools[j].print_z - lt_new.print_z) < EPSILON) {
                        m_layer_tools[j].has_wipe_tower = true;
                    } else {
                        LayerTools &lt_extra = *m_layer_tools.insert(m_layer_tools.begin() + j, lt_new);
                        //LayerTools &lt_prev  = m_layer_tools[j];
                        LayerTools &lt_next  = m_layer_tools[j + 1];
                        assert(! m_layer_tools[j - 1].extruders.empty() && ! lt_next.extruders.empty());
                        // FIXME: 运行combine_infill.t时触发了以下断言。我决定暂时注释掉它。
                        // 如果这是一个bug，可能不严重，因为这段代码已经很长时间没有改变了。它可能
                        // 仍然值得进一步研究，以确定是bug还是过时的断言。
                        //assert(lt_prev.extruders.back() == lt_next.extruders.front());
                        lt_extra.has_wipe_tower = true;
                        lt_extra.extruders.push_back(lt_next.extruders.front());
                        lt_extra.wipe_tower_partitions = lt_next.wipe_tower_partitions;
                    }
                }
            }
            break;
        }
    }

    // 如果模型包含空层（如https://github.com/prusa3d/Slic3r/issues/1266），可能存在
    // 未标记为has_wipe_tower的层，即使它们应该被标记。这会导致可溶性支撑崩溃
    // 和其他问题。因此，我们将遍历layer_tools并检测和修复此问题。
    // 所以，如果有一个非对象层，其开始的挤出机与最后一层结束的挤出机不同（或包含多个挤出机），
    // 我们将用has_wipe tower标记它。
    for (unsigned int i=0; i+1<m_layer_tools.size(); ++i) {
        LayerTools& lt = m_layer_tools[i];
        LayerTools& lt_next = m_layer_tools[i+1];
        if (lt.extruders.empty() || lt_next.extruders.empty())
            break;
        if (!lt_next.has_wipe_tower && (lt_next.extruders.front() != lt.extruders.back() || lt_next.extruders.size() > 1))
            lt_next.has_wipe_tower = true;
        // 我们还应该检查下一个擦拭塔层不超过max_layer_height：
        unsigned int j = i+1;
        double last_wipe_tower_print_z = lt_next.print_z;
        while (++j < m_layer_tools.size()-1 && !m_layer_tools[j].has_wipe_tower)
            if (m_layer_tools[j+1].print_z - last_wipe_tower_print_z > max_layer_height + EPSILON) {
                m_layer_tools[j].has_wipe_tower = true;
                last_wipe_tower_print_z = m_layer_tools[j].print_z;
            }
    }

    // 计算wipe_tower_layer_height值。
    coordf_t wipe_tower_print_z_last = 0.;
    for (LayerTools &lt : m_layer_tools)
        if (lt.has_wipe_tower) {
            lt.wipe_tower_layer_height = lt.print_z - wipe_tower_print_z_last;
            wipe_tower_print_z_last = lt.print_z;
        }
}

void ToolOrdering::collect_extruder_statistics(bool prime_multi_material)
{
    m_first_printing_extruder = (unsigned int)-1;
    for (const auto &lt : m_layer_tools)
        if (! lt.extruders.empty()) {
            m_first_printing_extruder = lt.extruders.front();
            break;
        }

    m_last_printing_extruder = (unsigned int)-1;
    for (auto lt_it = m_layer_tools.rbegin(); lt_it != m_layer_tools.rend(); ++ lt_it)
        if (! lt_it->extruders.empty()) {
            m_last_printing_extruder = lt_it->extruders.back();
            break;
        }

    m_all_printing_extruders.clear();
    for (const auto &lt : m_layer_tools) {
        append(m_all_printing_extruders, lt.extruders);
        sort_remove_duplicates(m_all_printing_extruders);
    }

    if (prime_multi_material && ! m_all_printing_extruders.empty()) {
        // 按初始顺序重新排序m_all_printing_extruders，最后一个将是m_first_printing_extruder。
        // 然后将m_first_printing_extruder设置为第一个初始化的挤出机。
        m_all_printing_extruders.erase(
            std::remove_if(m_all_printing_extruders.begin(), m_all_printing_extruders.end(),
                [ this ](const unsigned int eid) { return eid == m_first_printing_extruder; }),
            m_all_printing_extruders.end());
        m_all_printing_extruders.emplace_back(m_first_printing_extruder);
        m_first_printing_extruder = m_all_printing_extruders.front();
    }
}

void ToolOrdering::reorder_extruders_for_minimum_flush_volume()
{
    const PrintConfig *print_config = m_print_config_ptr;
    if (!print_config && m_print_object_ptr) {
        print_config = &(m_print_object_ptr->print()->config());
    }

    if (!print_config || m_layer_tools.empty())
        return;

    // 获取擦拭矩阵以获取挤出机数量并将vector<double>转换为vector<float>：
    std::vector<float> flush_matrix(cast<float>(print_config->flush_volumes_matrix.values));
    const unsigned int number_of_extruders = (unsigned int) (sqrt(flush_matrix.size()) + EPSILON);
    // 提取每对挤出机的冲洗体积：
    std::vector<std::vector<float>> wipe_volumes;
    if ((print_config->purge_in_prime_tower && print_config->single_extruder_multi_material) || m_is_BBL_printer) {
        for (unsigned int i = 0; i < number_of_extruders; ++i)
            wipe_volumes.push_back( std::vector<float>(flush_matrix.begin() + i * number_of_extruders,
                                                       flush_matrix.begin() + (i + 1) * number_of_extruders));
    } else {
        // 用prime_volume填充wipe_volumes
        for (unsigned int i = 0; i < number_of_extruders; ++i)
            wipe_volumes.push_back(std::vector<float>(number_of_extruders, print_config->prime_volume));
    }

    auto extruders_to_hash_key = [](const std::vector<unsigned int>& extruders,
                                    std::optional<unsigned int>      initial_extruder_id) -> uint32_t {
        uint32_t hash_key = 0;
        // 高16位定义初始挤出机，低16位定义挤出机集合
        if (initial_extruder_id)
            hash_key |= (1 << (16 + *initial_extruder_id));
        for (auto item : extruders)
            hash_key |= (1 << item);
        return hash_key;
    };

    std::vector<LayerPrintSequence> other_layers_seqs;
    const ConfigOptionInts *other_layers_print_sequence_op = print_config->option<ConfigOptionInts>("other_layers_print_sequence");
    const ConfigOptionInt *other_layers_print_sequence_nums_op = print_config->option<ConfigOptionInt>("other_layers_print_sequence_nums");
    if (other_layers_print_sequence_op && other_layers_print_sequence_nums_op) {
        const std::vector<int> &print_sequence = other_layers_print_sequence_op->values;
        int sequence_nums = other_layers_print_sequence_nums_op->value;
        other_layers_seqs = get_other_layers_print_sequence(sequence_nums, print_sequence);
    }

    // other_layers_seq: layer_idx和extruder_idx基于1
    auto get_custom_seq = [&other_layers_seqs](int layer_idx, std::vector<int>& out_seq) -> bool {
        for (size_t idx = other_layers_seqs.size() - 1; idx != size_t(-1); --idx) {
            const auto &other_layers_seq = other_layers_seqs[idx];
            if (layer_idx + 1 >= other_layers_seq.first.first && layer_idx + 1 <= other_layers_seq.first.second) {
                out_seq = other_layers_seq.second;
                return true;
            }
        }
        return false;
    };

    std::optional<unsigned int> current_extruder_id;
    for (int i = 0; i < m_layer_tools.size(); ++i) {
        LayerTools& lt = m_layer_tools[i];
        if (lt.extruders.empty())
            continue;
        if (lt.preserve_extruder_order) {
            current_extruder_id = lt.extruders.back();
            continue;
        }

        std::vector<int> custom_extruder_seq;
        if (get_custom_seq(i, custom_extruder_seq) && !custom_extruder_seq.empty()) {
            std::vector<unsigned int> unsign_custom_extruder_seq;
            for (int extruder : custom_extruder_seq) {
                unsigned int unsign_extruder = static_cast<unsigned int>(extruder) - 1;
                auto it = std::find(lt.extruders.begin(), lt.extruders.end(), unsign_extruder);
                if (it != lt.extruders.end()) {
                    unsign_custom_extruder_seq.emplace_back(unsign_extruder);
                }
            }
            assert(lt.extruders.size() == unsign_custom_extruder_seq.size());
            lt.extruders = unsign_custom_extruder_seq;
            current_extruder_id = lt.extruders.back();
            continue;
        }

        // 算法复杂度为O(n2*2^n)
        if (i != 0) {
            auto hash_key = extruders_to_hash_key(lt.extruders, current_extruder_id);
            auto iter = m_tool_order_cache.find(hash_key);
            if (iter == m_tool_order_cache.end()) {
                lt.extruders = get_extruders_order(wipe_volumes, lt.extruders, current_extruder_id);
                std::vector<uint8_t> hash_val;
                hash_val.reserve(lt.extruders.size());
                for (auto item : lt.extruders)
                    hash_val.emplace_back(static_cast<uint8_t>(item));
                m_tool_order_cache[hash_key] = hash_val;
            }
            else {
                std::vector<unsigned int>extruder_order;
                extruder_order.reserve(iter->second.size());
                for (auto item : iter->second)
                    extruder_order.emplace_back(static_cast<unsigned int>(item));
                lt.extruders = std::move(extruder_order);
            }
        }
        current_extruder_id = lt.extruders.back();
    }
}

// 层被标记为无限裙边即防尘罩。并非所有层都需要打印。
void ToolOrdering::mark_skirt_layers(const PrintConfig &config, coordf_t max_layer_height)
{
    if (m_layer_tools.empty())
        return;

    if (m_layer_tools.front().extruders.empty()) {
        // 第一层为空，不打印裙边。
        //FIXME 抛出异常？
        return;
    }

    size_t i = 0;
    for (;;) {
        m_layer_tools[i].has_skirt = true;
        size_t j = i + 1;
        for (; j < m_layer_tools.size() && ! m_layer_tools[j].has_object; ++ j);
        // i和j是两个连续的打印对象的层。
        if (j == m_layer_tools.size())
            // 不在最后一个对象层上方打印裙边。
            break;
        // 将一些打印中间层标记为有裙边。
        double last_z = m_layer_tools[i].print_z;
        for (size_t k = i + 1; k < j; ++ k) {
            if (m_layer_tools[k + 1].print_z - last_z > max_layer_height + EPSILON) {
                // 层k是最后一个不违反最大层高的层。
                // 不在空层上挤出裙边。
                while (m_layer_tools[k].extruders.empty())
                    -- k;
                if (m_layer_tools[k].has_skirt) {
                    // 由于空层无法生成裙边，因为裙边中会缺少一层。
                    //FIXME 抛出异常？
                    break;
                }
                m_layer_tools[k].has_skirt = true;
                last_z = m_layer_tools[k].print_z;
            }
        }
        i = j;
    }
}

// 将自定义G-code的指针分配给相应的ToolOrdering::LayerTools。
// 忽略在层上执行且对于该挤出机在该层以上不会打印的挤出机的颜色更改。
// 如果单个层跨越多个事件，使用最后一个。

// BBS: 用当前板块自定义gcode替换模型自定义gcode
static CustomGCode::Info custom_gcode_per_print_z;
void ToolOrdering::assign_custom_gcodes(const Print &print)
{
    // 仅对非顺序打印有效。
    assert(print.config().print_sequence == PrintSequence::ByLayer);

    custom_gcode_per_print_z = print.model().get_curr_plate_custom_gcodes();
    if (custom_gcode_per_print_z.gcodes.empty())
        return;

    // BBS
    auto                        num_filaments = unsigned(print.config().filament_diameter.size());
    CustomGCode::Mode           mode          =
        (num_filaments == 1) ? CustomGCode::SingleExtruder :
        print.object_extruders().size() == 1 ? CustomGCode::MultiAsSingle : CustomGCode::MultiExtruder;
    CustomGCode::Mode           model_mode    = print.model().get_curr_plate_custom_gcodes().mode;
    std::vector<unsigned char>  extruder_printing_above(num_filaments, false);
    auto                        custom_gcode_it = custom_gcode_per_print_z.gcodes.rbegin();
    // 如果模型的工具/颜色更改是以mm模式输入的，而打印模式不是mm模式，则工具更改和颜色更改将被忽略。
    bool                        ignore_tool_and_color_changes = (mode == CustomGCode::MultiExtruder) != (model_mode == CustomGCode::MultiExtruder);
    // 如果在单挤出机机器上打印，使工具更改触发颜色更改（M600）事件。
    bool                        tool_changes_as_color_changes = mode == CustomGCode::SingleExtruder && model_mode == CustomGCode::MultiAsSingle;

    // 从最后一层到第一层：
    coordf_t print_z_above = std::numeric_limits<coordf_t>::lowest();
    for (auto it_lt = m_layer_tools.rbegin(); it_lt != m_layer_tools.rend(); ++ it_lt) {
        LayerTools &lt = *it_lt;
        // 将当前层的挤出机添加到在此print_z及以上打印的挤出机集合中。
        for (unsigned int i : lt.extruders)
            extruder_printing_above[i] = true;
        // 跳过此层以上的所有自定义G-code并跳过所有挤出机切换。
        for (; custom_gcode_it != custom_gcode_per_print_z.gcodes.rend() && (
            (print_z_above > lt.print_z && custom_gcode_it->print_z > 0.5 * (lt.print_z + print_z_above))
            || custom_gcode_it->type == CustomGCode::ToolChange); ++ custom_gcode_it);
        print_z_above = lt.print_z;
        if (custom_gcode_it == custom_gcode_per_print_z.gcodes.rend())
            // 自定义G-code已处理。
            break;
        // 为当前层或以下层配置了一些自定义G-code。
        const CustomGCode::Item &custom_gcode = *custom_gcode_it;
        // 当前层以下层的print_z。
        coordf_t print_z_below = 0.;
        if (auto it_lt_below = it_lt; ++ it_lt_below != m_layer_tools.rend())
            print_z_below = it_lt_below->print_z;
        if (custom_gcode.print_z > 0.5 * (print_z_below + lt.print_z)) {
            // 自定义G-code适用于当前层。
            bool color_change = custom_gcode.type == CustomGCode::ColorChange;
            bool tool_change  = custom_gcode.type == CustomGCode::ToolChange;
            bool pause_or_custom_gcode = ! color_change && ! tool_change;
            bool apply_color_change = ! ignore_tool_and_color_changes &&
                // 如果是颜色更改，它实际上将在上方的挤出机上打印时有用。
                // BBS
                (color_change ?
                    mode == CustomGCode::SingleExtruder ||
                        (custom_gcode.extruder <= int(num_filaments) && extruder_printing_above[unsigned(custom_gcode.extruder - 1)]) :
                    tool_change && tool_changes_as_color_changes);
            if (pause_or_custom_gcode || apply_color_change)
                lt.custom_gcode = &custom_gcode;
            // 消耗该自定义G-code事件。
            ++ custom_gcode_it;
        }
    }
}

const LayerTools& ToolOrdering::tools_for_layer(coordf_t print_z) const
{
    auto it_layer_tools = std::lower_bound(m_layer_tools.begin(), m_layer_tools.end(), LayerTools(print_z - EPSILON));
    assert(it_layer_tools != m_layer_tools.end());
    coordf_t dist_min = std::abs(it_layer_tools->print_z - print_z);
    for (++ it_layer_tools; it_layer_tools != m_layer_tools.end(); ++ it_layer_tools) {
        coordf_t d = std::abs(it_layer_tools->print_z - print_z);
        if (d >= dist_min)
            break;
        dist_min = d;
    }
    -- it_layer_tools;
    assert(dist_min < EPSILON);
    return *it_layer_tools;
}

// 此函数从Print::mark_wiping_extrusions调用，设置此实体应使用的挤出机（-1 .. 表示正常）
void WipingExtrusions::set_extruder_override(const ExtrusionEntity* entity, const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies)
{
    something_overridden = true;

    auto entity_map_it = (entity_map.emplace(std::make_tuple(entity, object), ExtruderPerCopy())).first; // （添加并）返回迭代器
    ExtruderPerCopy& copies_vector = entity_map_it->second;
    copies_vector.resize(num_of_copies, -1);

    if (copies_vector[copy_id] != -1)
        std::cout << "ERROR: 实体挤出机被多次覆盖！！！\n";    // 调试消息 - 这绝不能发生。

    copies_vector[copy_id] = extruder;
}

// BBS
void WipingExtrusions::set_support_extruder_override(const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies)
{
    something_overridden = true;
    support_map.emplace(object, extruder);
}

void WipingExtrusions::set_support_interface_extruder_override(const PrintObject* object, size_t copy_id, int extruder, size_t num_of_copies)
{
    something_overridden = true;
    support_intf_map.emplace(object, extruder);
}

// 找到层上的第一个非可溶性挤出机
int WipingExtrusions::first_nonsoluble_extruder_on_layer(const PrintConfig& print_config) const
{
    const LayerTools& lt = *m_layer_tools;
    for (auto extruders_it = lt.extruders.begin(); extruders_it != lt.extruders.end(); ++extruders_it)
        if (!print_config.filament_soluble.get_at(*extruders_it) && !print_config.filament_is_support.get_at(*extruders_it))
            return (*extruders_it);

    return (-1);
}

// 找到层上的最后一个非可溶性挤出机
int WipingExtrusions::last_nonsoluble_extruder_on_layer(const PrintConfig& print_config) const
{
    const LayerTools& lt = *m_layer_tools;
    for (auto extruders_it = lt.extruders.rbegin(); extruders_it != lt.extruders.rend(); ++extruders_it)
        if (!print_config.filament_soluble.get_at(*extruders_it) && !print_config.filament_is_support.get_at(*extruders_it))
            return (*extruders_it);

    return (-1);
}

// 决定此实体是否可被覆盖
bool WipingExtrusions::is_overriddable(const ExtrusionEntityCollection& eec, const PrintConfig& print_config, const PrintObject& object, const PrintRegion& region) const
{
    if (print_config.filament_soluble.get_at(m_layer_tools->extruder(eec, region)))
        return false;

    if (object.config().flush_into_objects)
        return true;

    if (!object.config().flush_into_infill || eec.role() != erInternalInfill)
        return false;

    return true;
}

// BBS
bool WipingExtrusions::is_support_overriddable(const ExtrusionRole role, const PrintObject& object) const
{
    if (!object.config().flush_into_support)
        return false;

    if (role == erMixed) {
        return object.config().support_filament == 0 || object.config().support_interface_filament == 0;
    }
    else if (role == erSupportMaterial || role == erSupportTransition) {
        return object.config().support_filament == 0;
    }
    else if (role == erSupportMaterialInterface) {
        return object.config().support_interface_filament == 0;
    }

    return false;
}

// 以下函数遍历层上的所有挤出，记住那些可在工具更换后用于擦拭的挤出
// 并返回在擦拭塔上仍需擦拭的体积。
float WipingExtrusions::mark_wiping_extrusions(const Print& print, unsigned int old_extruder, unsigned int new_extruder, float volume_to_wipe)
{
    const LayerTools& lt = *m_layer_tools;
    const float min_infill_volume = 0.f; // 忽略小于此体积的填充

    if (! this->something_overridable || volume_to_wipe <= 0. || print.config().filament_soluble.get_at(old_extruder) || print.config().filament_soluble.get_at(new_extruder))
        return std::max(0.f, volume_to_wipe); // 可溶性丝材不能被随机填充擦拭，它后面的丝材也不行

    // BBS
    if (print.config().filament_is_support.get_at(old_extruder) || print.config().filament_is_support.get_at(new_extruder))
        return std::max(0.f, volume_to_wipe); // 支撑丝材不能用于打印支撑、填充、擦拭塔等。

    // 我们将对对象进行排序，以便用于擦拭的在前面：
    ConstPrintObjectPtrs object_list = print.objects().vector();
    // BBS: 修复由不同对象之间顺序不固定引起的异常
    std::sort(object_list.begin(), object_list.end(), [object_list](const PrintObject* a, const PrintObject* b) {
        if (a->config().flush_into_objects != b->config().flush_into_objects) {
            return a->config().flush_into_objects.getBool();
        }
        else {
            return a->id() < b->id();
        }
    });

    // 我们现在将遍历
    //  - 首先专用对象以标记周长或填充（取决于infill_first）
    //  - 其次再次遍历专用对象以标记填充或周长（取决于infill_first）
    //  - 然后所有其他对象以标记填充（如果!infill_first，我们还必须检查周长是否已完成
    // 这由以下变量控制：
    bool perimeters_done = false;

    for (int i=0 ; i<(int)object_list.size() + (perimeters_done ? 0 : 1); ++i) {
        if (!perimeters_done && (i==(int)object_list.size() || !object_list[i]->config().flush_into_objects)) { // 我们通过了列表中的最后一个专用对象
            perimeters_done = true;
            i=-1;   // 让我们从头再来
            continue;
        }

        const PrintObject* object = object_list[i];

        // 找到此层：
        const Layer* this_layer = object->get_layer_at_printz(lt.print_z, EPSILON);
        if (this_layer == nullptr)
            continue;

        size_t num_of_copies = object->instances().size();

        // 首先遍历副本（即PrintObject实例），以便我们标记相邻的填充以最小化移动移动
        for (unsigned int copy = 0; copy < num_of_copies; ++copy) {
            for (const LayerRegion *layerm : this_layer->regions()) {
                const auto &region = layerm->region();

                if (!object->config().flush_into_infill && !object->config().flush_into_objects && !object->config().flush_into_support)
                    continue;
                bool wipe_into_infill_only = !object->config().flush_into_objects && object->config().flush_into_infill;
                bool is_infill_first = region.config().is_infill_first;
                if (is_infill_first != perimeters_done || wipe_into_infill_only) {
                    for (const ExtrusionEntity* ee : layerm->fills.entities) {                      // 遍历所有填充Collection
                        auto* fill = dynamic_cast<const ExtrusionEntityCollection*>(ee);

                        if (!is_overriddable(*fill, print.config(), *object, region))
                            continue;

                        if (wipe_into_infill_only && ! is_infill_first)
                            // 在这种情况下，我们必须检查原始挤出机是否在此层之前于我们正在覆盖的挤出机之前使用
                            // （并且周长将在填充之前完成）：
                            if (!lt.is_extruder_order(lt.wall_filament(region), new_extruder))
                                continue;

                        if ((!is_entity_overridden(fill, object, copy) && fill->total_volume() > min_infill_volume))
                        {     // 此填充将用于擦拭此挤出机
                            set_extruder_override(fill, object, copy, new_extruder, num_of_copies);
                            if ((volume_to_wipe -= float(fill->total_volume())) <= 0.f)
                                // 已经冲洗了比要求的更多的材料。
                                return 0.f;
                        }
                    }
                }

                // 现在周长同理 - 见上面解释的注释：
                if (object->config().flush_into_objects && is_infill_first == perimeters_done)
                {
                    for (const ExtrusionEntity* ee : layerm->perimeters.entities) {
                        auto* fill = dynamic_cast<const ExtrusionEntityCollection*>(ee);
                        if (is_overriddable(*fill, print.config(), *object, region) && !is_entity_overridden(fill, object, copy) && fill->total_volume() > min_infill_volume) {
                            set_extruder_override(fill, object, copy, new_extruder, num_of_copies);
                            if ((volume_to_wipe -= float(fill->total_volume())) <= 0.f)
                                // 已经冲洗了比要求的更多的材料。
                                return 0.f;
                        }
                    }
                }
            }

            // BBS
            if (object->config().flush_into_support) {
                auto& object_config = object->config();
                const SupportLayer* this_support_layer = object->get_support_layer_at_printz(lt.print_z, EPSILON);

                do {
                    if (this_support_layer == nullptr)
                        break;

                    bool support_overriddable = object_config.support_filament == 0;
                    bool support_intf_overriddable = object_config.support_interface_filament == 0;
                    if (!support_overriddable && !support_intf_overriddable)
                        break;

                    auto &entities = this_support_layer->support_fills.entities;
                    if (support_overriddable && !is_support_overridden(object) && !(object_config.support_interface_not_for_body.value && !support_intf_overriddable &&(new_extruder==object_config.support_interface_filament-1||old_extruder==object_config.support_interface_filament-1))) {
                        set_support_extruder_override(object, copy, new_extruder, num_of_copies);
                        for (const ExtrusionEntity* ee : entities) {
                            if (ee->role() == erSupportMaterial || ee->role() == erSupportTransition)
                                volume_to_wipe -= ee->total_volume();

                            if (volume_to_wipe <= 0.f)
                                return 0.f;
                        }
                    }

                    if (support_intf_overriddable && !is_support_interface_overridden(object)) {
                        set_support_interface_extruder_override(object, copy, new_extruder, num_of_copies);
                        for (const ExtrusionEntity* ee : entities) {
                            if (ee->role() == erSupportMaterialInterface)
                                volume_to_wipe -= ee->total_volume();

                            if (volume_to_wipe <= 0.f)
                                return 0.f;
                        }
                    }
                } while (0);
            }
        }
    }
    // 一些冲洗需要在擦拭塔上完成。
    assert(volume_to_wipe > 0.);
    return volume_to_wipe;
}



// 在层上的所有工具更改都调用了mark_infill_overridden后调用。可能仍存在可覆盖的实体，
// 但实际上并未被覆盖。如果它们是专用对象的一部分，用最初分配给它们的挤出机打印它们可能意味着违反周长-填充顺序。因此，我们将再次遍历它们并确保我们覆盖它们。
void WipingExtrusions::ensure_perimeters_infills_order(const Print& print)
{
    if (! this->something_overridable)
        return;

    const LayerTools& lt = *m_layer_tools;
    unsigned int first_nonsoluble_extruder = first_nonsoluble_extruder_on_layer(print.config());
    unsigned int last_nonsoluble_extruder = last_nonsoluble_extruder_on_layer(print.config());

    for (const PrintObject* object : print.objects()) {
        // 找到此层：
        const Layer* this_layer = object->get_layer_at_printz(lt.print_z, EPSILON);
        if (this_layer == nullptr)
            continue;
        size_t num_of_copies = object->instances().size();

        for (size_t copy = 0; copy < num_of_copies; ++copy) {    // 首先遍历副本，以便标记相邻的填充以最小化移动移动
            for (const LayerRegion *layerm : this_layer->regions()) {
                const auto &region = layerm->region();
                //BBS
                if (!object->config().flush_into_infill && !object->config().flush_into_objects)
                    continue;

                bool is_infill_first = region.config().is_infill_first;
                for (const ExtrusionEntity* ee : layerm->fills.entities) {                      // 遍历所有填充Collection
                    auto* fill = dynamic_cast<const ExtrusionEntityCollection*>(ee);

                    if (!is_overriddable(*fill, print.config(), *object, region)
                     || is_entity_overridden(fill, object, copy) )
                        continue;

                    // 此填充本可以被覆盖但没有 - 除非我们采取措施，否则它可能
                    // 在其周长之前被打印，或根本不被打印（如果其原始挤出机
                    // 未添加到LayerTools中）
                    // 无论哪种方式，我们现在将强制用适当的内容覆盖它：
                    //BBS
                    if (is_infill_first
                    //BBS
                    //|| object->config().flush_into_objects  // 在这种情况下周长已被覆盖，因此我们可以安全地用最后一个覆盖
                    || lt.is_extruder_order(lt.wall_filament(region), last_nonsoluble_extruder    // !infill_first，但最后一个挤出机打印时周长已打印
                    || ! lt.has_extruder(lt.sparse_infill_filament(region)))) // 我们必须强制覆盖 - 这可能违反infill_first（FIXME）
                        set_extruder_override(fill, object, copy, (is_infill_first ? first_nonsoluble_extruder : last_nonsoluble_extruder), num_of_copies);
                    else {
                        // 在这种情况下，我们可以（也应该）让它正常打印。
                        // 强制覆盖将意味着它在周长之前被打印。
                    }
                }

                // 现在周长同理 - 见上面解释的注释：
                for (const ExtrusionEntity* ee : layerm->perimeters.entities) {                      // 遍历所有周长Collection
                    auto* fill = dynamic_cast<const ExtrusionEntityCollection*>(ee);
                    if (is_overriddable(*fill, print.config(), *object, region) && ! is_entity_overridden(fill, object, copy))
                        set_extruder_override(fill, object, copy, (is_infill_first ? last_nonsoluble_extruder : first_nonsoluble_extruder), num_of_copies);
                }
            }
        }
    }
}

// 以下函数从GCode::process_layer调用，返回指向向量的指针，其中包含有关应为此实体副本使用哪些挤出机的信息。
// 如果此挤出没有任何覆盖，则返回nullptr。
// 否则，它就地修改向量并将所有-1更改为correct_extruder_id（创建覆盖时，正确的挤出机未知，
// 因此使用-1表示"正常打印"）。
// 因此，结果向量跟踪哪些挤出是被覆盖的，哪些不是。如果使用的挤出机是被覆盖的，
// 其数字原样保存（基于零的索引）。常规挤出保存为-number-1（不幸的是没有负零）。
const WipingExtrusions::ExtruderPerCopy* WipingExtrusions::get_extruder_overrides(const ExtrusionEntity* entity, const PrintObject* object, int correct_extruder_id, size_t num_of_copies)
{
    ExtruderPerCopy *overrides = nullptr;
    auto entity_map_it = entity_map.find(std::make_tuple(entity, object));
    if (entity_map_it != entity_map.end()) {
        overrides = &entity_map_it->second;
        overrides->resize(num_of_copies, -1);
        // 每个-1现在表示"正常打印" - 我们将用实际挤出机id替换它（移位以便我们不丢失该信息）：
        std::replace(overrides->begin(), overrides->end(), -1, -correct_extruder_id-1);
    }
    return overrides;
}

// BBS
int WipingExtrusions::get_support_extruder_overrides(const PrintObject* object)
{
    auto iter = support_map.find(object);
    if (iter != support_map.end())
        return iter->second;

    return -1;
}

int WipingExtrusions::get_support_interface_extruder_overrides(const PrintObject* object)
{
    auto iter = support_intf_map.find(object);
    if (iter != support_intf_map.end())
        return iter->second;

    return -1;
}

// 通过混合丝材管理器解析基于1的丝材ID。
unsigned int ToolOrdering::resolve_mixed(unsigned int filament_id_1based,
                                         int          layer_index,
                                         float        layer_print_z,
                                         float        layer_height,
                                         const PrintObject* current_object) const
{
    return resolve_mixed_with_layer_heights(m_mixed_mgr,
                                            m_num_physical,
                                            filament_id_1based,
                                            layer_index,
                                            layer_print_z,
                                            layer_height,
                                            m_mixed_layer_height_a,
                                            m_mixed_layer_height_b,
                                            m_mixed_base_layer_height,
                                            current_object);
}

} // namespace Slic3r
