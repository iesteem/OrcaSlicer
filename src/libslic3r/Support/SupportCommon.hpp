#ifndef slic3r_SupportCommon_hpp_
#define slic3r_SupportCommon_hpp_

#include "../Layer.hpp"
#include "../Polygon.hpp"
#include "../Print.hpp"
#include "SupportLayer.hpp"
#include "SupportParameters.hpp"

namespace Slic3r {

class PrintObject;
class SupportLayer;

// 从支撑接触区域移除桥接。
// 在PrintObjectConfig::dont_support_bridges时调用。
void remove_bridges_from_contacts(
    const PrintConfig   &print_config, 
    const Layer         &lower_layer,
    const LayerRegion   &layerm,
    float                fw, 
    Polygons            &contact_polygons);

// 将部分基础层转换为基础界面层。
// 对于可溶性界面与非可溶性基础，使用基础挤出机最多打印前两个界面层，
// 以改善可溶性耗材与基础的附着力。
// 对于Organic支撑，将top_interface_layers和top_base_interface_layers与此函数生成的界面合并。
std::pair<SupportGeneratorLayersPtr, SupportGeneratorLayersPtr> generate_interface_layers(
    const PrintObjectConfig           &config,
    const SupportParameters           &support_params,
    const SupportGeneratorLayersPtr   &bottom_contacts,
    const SupportGeneratorLayersPtr   &top_contacts,
    // 输入/输出，将与输出合并
    SupportGeneratorLayersPtr         &top_interface_layers,
    SupportGeneratorLayersPtr         &top_base_interface_layers,
    // 输入，将使用新创建的界面层进行修剪。
    SupportGeneratorLayersPtr         &intermediate_layers,
    SupportGeneratorLayerStorage      &layer_storage);

// 生成筏层，如果没有筏层则扩展第一层支撑层以改善支撑附着力。
SupportGeneratorLayersPtr generate_raft_base(
	const PrintObject				&object,
	const SupportParameters			&support_params,
	const SlicingParameters			&slicing_params,
	const SupportGeneratorLayersPtr &top_contacts,
	const SupportGeneratorLayersPtr &interface_layers,
	const SupportGeneratorLayersPtr &base_interface_layers,
	const SupportGeneratorLayersPtr &base_layers,
	SupportGeneratorLayerStorage    &layer_storage);

void tree_supports_generate_paths(ExtrusionEntitiesPtr &dst, const Polygons &polygons, const Flow &flow, const SupportParameters &support_params);

void fill_expolygons_with_sheath_generate_paths(
    ExtrusionEntitiesPtr &dst, const Polygons &polygons, Fill *filler, float density, ExtrusionRole role, const Flow &flow, const SupportParameters& support_params, bool with_sheath, bool no_sort);

// 返回排序后的层
SupportGeneratorLayersPtr generate_support_layers(
	PrintObject							&object,
    const SupportGeneratorLayersPtr     &raft_layers,
    const SupportGeneratorLayersPtr     &bottom_contacts,
    const SupportGeneratorLayersPtr     &top_contacts,
    const SupportGeneratorLayersPtr     &intermediate_layers,
    const SupportGeneratorLayersPtr     &interface_layers,
    const SupportGeneratorLayersPtr     &base_interface_layers);

// 生成支撑G代码。
// 同时用于经典支撑和树状支撑。
void generate_support_toolpaths(
	SupportLayerPtrs    				&support_layers,
	const PrintObjectConfig 			&config,
	const SupportParameters 			&support_params,
	const SlicingParameters 			&slicing_params,
    const SupportGeneratorLayersPtr 	&raft_layers,
    const SupportGeneratorLayersPtr   	&bottom_contacts,
    const SupportGeneratorLayersPtr   	&top_contacts,
    const SupportGeneratorLayersPtr   	&intermediate_layers,
	const SupportGeneratorLayersPtr   	&interface_layers,
    const SupportGeneratorLayersPtr   	&base_interface_layers);

// FN_HIGHER_EQUAL: 提供的对象指针的Z值 >= 内部阈值。
// 查找第一个Z值 >= fn_higher_equal内部阈值的项目。
// 如果未找到Z值 >= fn_higher_equal内部阈值的vec项目，则返回vec.size()。
// 如果初始idx为size_t(-1)，则使用二分搜索。
// 否则线性向上搜索。
template<typename IteratorType, typename IndexType, typename FN_HIGHER_EQUAL>
IndexType idx_higher_or_equal(IteratorType begin, IteratorType end, IndexType idx, FN_HIGHER_EQUAL fn_higher_equal)
{
    auto size = int(end - begin);
    if (size == 0) {
        idx = 0;
    } else if (idx == IndexType(-1)) {
        // 每个线程池调用批次的第一层。使用二分搜索。
        int idx_low  = 0;
        int idx_high = std::max(0, size - 1);
        while (idx_low + 1 < idx_high) {
            int idx_mid  = (idx_low + idx_high) / 2;
            if (fn_higher_equal(begin[idx_mid]))
                idx_high = idx_mid;
            else
                idx_low  = idx_mid;
        }
        idx =  fn_higher_equal(begin[idx_low])  ? idx_low  :
              (fn_higher_equal(begin[idx_high]) ? idx_high : size);
    } else {
        // 对于该批次的其他层，递增搜索，这比二分搜索更廉价。
        while (int(idx) < size && ! fn_higher_equal(begin[idx]))
            ++ idx;
    }
    return idx;
}
template<typename T, typename IndexType, typename FN_HIGHER_EQUAL>
IndexType idx_higher_or_equal(const std::vector<T>& vec, IndexType idx, FN_HIGHER_EQUAL fn_higher_equal)
{
    return idx_higher_or_equal(vec.begin(), vec.end(), idx, fn_higher_equal);
}

// FN_LOWER_EQUAL: 提供的对象指针的Z值 <= 内部阈值。
// 查找第一个Z值 <= fn_lower_equal内部阈值的项目。
// 如果未找到Z值 <= fn_lower_equal内部阈值的vec项目，则返回-1。
// 如果初始idx < -1，则使用二分搜索。
// 否则线性向下搜索。
template<typename IT, typename FN_LOWER_EQUAL>
int idx_lower_or_equal(IT begin, IT end, int idx, FN_LOWER_EQUAL fn_lower_equal)
{
    auto size = int(end - begin);
    if (size == 0) {
        idx = -1;
    } else if (idx < -1) {
        // 每个线程池调用批次的第一层。使用二分搜索。
        int idx_low  = 0;
        int idx_high = std::max(0, size - 1);
        while (idx_low + 1 < idx_high) {
            int idx_mid  = (idx_low + idx_high) / 2;
            if (fn_lower_equal(begin[idx_mid]))
                idx_low  = idx_mid;
            else
                idx_high = idx_mid;
        }
        idx =  fn_lower_equal(begin[idx_high]) ? idx_high :
              (fn_lower_equal(begin[idx_low ]) ? idx_low  : -1);
    } else {
        // 对于该批次的其他层，递增搜索，这比二分搜索更廉价。
        while (idx >= 0 && ! fn_lower_equal(begin[idx]))
            -- idx;
    }
    return idx;
}
template<typename T, typename FN_LOWER_EQUAL>
int idx_lower_or_equal(const std::vector<T*> &vec, int idx, FN_LOWER_EQUAL fn_lower_equal)
{
    return idx_lower_or_equal(vec.begin(), vec.end(), idx, fn_lower_equal);
}

} // namespace Slic3r

#endif /* slic3r_SupportCommon_hpp_ */
