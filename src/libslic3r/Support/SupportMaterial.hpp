#ifndef slic3r_SupportMaterial_hpp_
#define slic3r_SupportMaterial_hpp_

#include "Flow.hpp"
#include "PrintConfig.hpp"
#include "Slicing.hpp"
#include "Fill/FillBase.hpp"
#include "SupportLayer.hpp"
#include "SupportParameters.hpp"
namespace Slic3r {

class PrintObject;
class PrintConfig;
class PrintObjectConfig;

// 此类管理单个PrintObject的筏和支撑。
// 由Slic3r::Print::Object->_support_material()实例化。
// 此类在切片开始前实例化，因为Object将查询筏的参数以确定第一层高度和厚度。
class PrintObjectSupportMaterial
{
public:
	PrintObjectSupportMaterial(const PrintObject *object, const SlicingParameters &slicing_params);

	// 筏层是否启用？
	bool 		has_raft() 					const { return m_slicing_params.has_raft(); }
	// 是否有任何支撑？
	bool 		has_support()				const { return m_object_config->enable_support.value || m_object_config->enforce_support_layers; }
	bool 		build_plate_only() 			const { return this->has_support() && m_object_config->support_on_build_plate_only.value; }
	// BBS
	bool 		synchronize_layers()		const { return /*m_slicing_params.soluble_interface && */!m_print_config->independent_support_layer_height.value; }
	bool 		has_contact_loops() 		const { return m_object_config->support_interface_loop_pattern.value; }

	// 为对象生成支撑材料。
	// 新的支撑层将添加到对象中，
	// 包含每个支撑层的挤出路径和填充区域。
	void 		generate(PrintObject &object);

private:
	std::vector<Polygons> buildplate_covered(const PrintObject &object) const;

	// 生成支撑悬垂的顶部接触层。
	// 对于可溶性界面材料，使层高与对象同步，否则保持层高未定义。
	// 如果仅请求打印板表面支撑，则不在对象上方生成接触层。
	SupportGeneratorLayersPtr top_contact_layers(const PrintObject &object, const std::vector<Polygons> &buildplate_covered, SupportGeneratorLayerStorage &layer_storage) const;

	// 生成支撑顶部接触层的底部接触层。
	// 对于可溶性界面材料，使层高与对象同步，
	// 否则将层高设置为支撑界面喷嘴的桥接流量。
	SupportGeneratorLayersPtr bottom_contact_layers_and_layer_support_areas(
		const PrintObject &object, const SupportGeneratorLayersPtr &top_contacts, std::vector<Polygons> &buildplate_covered, 
		SupportGeneratorLayerStorage &layer_storage, std::vector<Polygons> &layer_support_areas) const;

	// 如果顶部接触层与底部接触层重叠，则修剪它们，以免两者没有足够的垂直空间。
	void trim_top_contacts_by_bottom_contacts(const PrintObject &object, const SupportGeneratorLayersPtr &bottom_contacts, SupportGeneratorLayersPtr &top_contacts) const;

	// 生成筏层以及底部接触面和顶部接触面之间的中间支撑层。
	SupportGeneratorLayersPtr raft_and_intermediate_support_layers(
	    const PrintObject   &object,
	    const SupportGeneratorLayersPtr   &bottom_contacts,
	    const SupportGeneratorLayersPtr   &top_contacts,
	    SupportGeneratorLayerStorage	  &layer_storage) const;

	// 使用多边形填充基础层。
	void generate_base_layers(
	    const PrintObject   &object,
	    const SupportGeneratorLayersPtr   &bottom_contacts,
	    const SupportGeneratorLayersPtr   &top_contacts,
	    SupportGeneratorLayersPtr         &intermediate_layers,
	    const std::vector<Polygons> &layer_support_areas) const;



	// 通过对象修剪支撑层，以在支撑体和对象之间留下定义的间隙。
	void trim_support_layers_by_object(
	    const PrintObject   &object,
	    SupportGeneratorLayersPtr         &support_layers,
	    const coordf_t       gap_extra_above,
	    const coordf_t       gap_extra_below,
	    const coordf_t       gap_xy) const;

/*
	void generate_pillars_shape();
	void clip_with_shape();
*/

	// 以下对象不属于SupportMaterial类所有。
	const PrintObject 		*m_object;
	const PrintConfig 		*m_print_config;
	const PrintObjectConfig *m_object_config;
	// 在对象切片器和支撑生成器之间共享的预计算参数，
	// 包含筏、第一层高度、第一对象层高度、筏与对象之间的间隙等信息。
	SlicingParameters	     m_slicing_params;
	// 与外部函数共享的各种预计算支撑参数。
	SupportParameters   	 m_support_params;
};

} // namespace Slic3r

#endif /* slic3r_SupportMaterial_hpp_ */
