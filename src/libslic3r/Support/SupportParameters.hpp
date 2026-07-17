#ifndef slic3r_SupportParameters_hpp_
#define slic3r_SupportParameters_hpp_

#include <boost/log/trivial.hpp>
#include "../libslic3r.h"
#include "../Flow.hpp"

namespace Slic3r {
struct SupportParameters {
    SupportParameters() = delete;
    SupportParameters(const PrintObject& object)
    {
        const PrintConfig& print_config = object.print()->config();
        const PrintObjectConfig& object_config = object.config();
        const SlicingParameters& slicing_params = object.slicing_parameters();

	    this->soluble_interface = slicing_params.soluble_interface;
	    this->soluble_interface_non_soluble_base =
	        // 悬垂与支撑界面之间的零z间距。
	        slicing_params.soluble_interface &&
	        // 界面挤出机可溶性。
	        object_config.support_interface_filament.value > 0 && print_config.filament_soluble.get_at(object_config.support_interface_filament.value - 1) &&
	        // 基础挤出机："使用活动挤出机打印"不可溶性。
	        (object_config.support_filament.value == 0 || ! print_config.filament_soluble.get_at(object_config.support_filament.value - 1));

	    {
	        this->num_top_interface_layers    = std::max(0, object_config.support_interface_top_layers.value);
	        this->num_bottom_interface_layers = object_config.support_interface_bottom_layers < 0 ? 
	            num_top_interface_layers : object_config.support_interface_bottom_layers;
	        this->has_top_contacts              = num_top_interface_layers    > 0;
	        this->has_bottom_contacts           = num_bottom_interface_layers > 0;
	        if (this->soluble_interface_non_soluble_base) {
	            // 尝试用非可溶性致密界面层支撑可溶性致密界面层。
	            this->num_top_base_interface_layers    = size_t(std::min(int(num_top_interface_layers) / 2, 2));
	            this->num_bottom_base_interface_layers = size_t(std::min(int(num_bottom_interface_layers) / 2, 2));
	        } else {
                // BBS: 如果支撑界面和支撑基础不使用相同的耗材，添加一个基础层以改善它们的附着力
                // 注意：支撑材料（如Supp.W）现在不能用作支撑基础，因此即使support_filament==0，
                // 支撑界面和基础仍然使用不同的耗材
                bool differnt_support_interface_filament = object_config.support_interface_filament != 0 &&
                                                           object_config.support_interface_filament != object_config.support_filament;
                this->num_top_base_interface_layers    = differnt_support_interface_filament ? 1 : 0;
                this->num_bottom_base_interface_layers       = differnt_support_interface_filament ? 1 : 0;
	        }
	    }
        this->first_layer_flow = Slic3r::support_material_1st_layer_flow(&object, float(slicing_params.first_print_layer_height));
        this->support_material_flow = Slic3r::support_material_flow(&object, float(slicing_params.layer_height));
        this->support_material_interface_flow = Slic3r::support_material_interface_flow(&object, float(slicing_params.layer_height));
    	this->raft_interface_flow                = support_material_interface_flow;

        this->ironing = object_config.support_ironing;
        this->ironing_flow = support_material_interface_flow.with_height(support_material_interface_flow.height() * 0.01 * object_config.support_ironing_flow.value);
        this->ironing_spacing = object_config.support_ironing_spacing;
        this->ironing_pattern = object_config.support_ironing_pattern;

        // 计算最小支撑层高为所有挤出机的最小值，但不小于10um。
        this->support_layer_height_min = scaled<coord_t>(0.01);
        for (auto lh : print_config.min_layer_height.values)
            this->support_layer_height_min = std::min(this->support_layer_height_min, std::max(0.01, lh));
        for (auto layer : object.layers())
            this->support_layer_height_min = std::min(this->support_layer_height_min, std::max(0.01, layer->height));
        
        if (object_config.support_interface_top_layers.value == 0) {
            // 不允许界面层，全部使用基础支撑图案打印。
            this->support_material_interface_flow = this->support_material_flow;
        }
        
        // 评估对象外部轮廓与支撑结构之间的XY间隙。
        // 评估对象外部轮廓与支撑结构之间的XY间隙。
        coordf_t external_perimeter_width = 0.;
        coordf_t bridge_flow_ratio = 0;
        for (size_t region_id = 0; region_id < object.num_printing_regions(); ++ region_id) {
            const PrintRegion &region = object.printing_region(region_id);
            external_perimeter_width = std::max(external_perimeter_width, coordf_t(region.flow(object, frExternalPerimeter, slicing_params.layer_height).width()));
            bridge_flow_ratio += region.config().bridge_flow;
        }
        this->gap_xy = object_config.support_object_xy_distance.value;
        this->gap_xy_first_layer = object_config.support_object_first_layer_gap.value;
        bridge_flow_ratio /= object.num_printing_regions();

        this->support_material_bottom_interface_flow = slicing_params.soluble_interface || !object_config.thick_bridges ?
            this->support_material_interface_flow.with_flow_ratio(bridge_flow_ratio) :
            Flow::bridging_flow(bridge_flow_ratio * this->support_material_interface_flow.nozzle_diameter(), this->support_material_interface_flow.nozzle_diameter());
        
        this->can_merge_support_regions = object_config.support_filament.value == object_config.support_interface_filament.value;
        if (!this->can_merge_support_regions && (object_config.support_filament.value == 0 || object_config.support_interface_filament.value == 0)) {
            // One of the support extruders is of "don't care" type.
            auto object_extruders = object.object_extruders();
            if (object_extruders.size() == 1 &&
                *object_extruders.begin() == std::max<unsigned int>(object_config.support_filament.value, object_config.support_interface_filament.value))
                // Object is printed with the same extruder as the support.
                this->can_merge_support_regions = true;
        }


        this->base_angle = Geometry::deg2rad(float(object_config.support_angle.value));
        this->interface_angle = Geometry::deg2rad(float(object_config.support_angle.value + 90.));
        // Orca: 使用支撑熨烫时强制使用实心支撑界面
        this->interface_spacing = (this->ironing ? 0 : object_config.support_interface_spacing.value) + this->support_material_interface_flow.spacing();
        this->interface_density = std::min(1., this->support_material_interface_flow.spacing() / this->interface_spacing);
        // Orca: 使用支撑熨烫时强制使用实心支撑界面
        double raft_interface_spacing = (this->ironing ? 0 : object_config.support_interface_spacing.value) + this->raft_interface_flow.spacing();
        this->raft_interface_density = std::min(1., this->raft_interface_flow.spacing() / raft_interface_spacing);
        this->support_spacing = object_config.support_base_pattern_spacing.value + this->support_material_flow.spacing();
        this->support_density = std::min(1., this->support_material_flow.spacing() / this->support_spacing);
        if (object_config.support_interface_top_layers.value == 0) {
            // 不允许界面层，全部使用基础支撑图案打印。
            this->interface_spacing = this->support_spacing;
            this->interface_density = this->support_density;
        }

        SupportMaterialPattern  support_pattern = object_config.support_base_pattern;
        this->with_sheath = object_config.tree_support_wall_count > 0;
        this->base_fill_pattern =
            support_pattern == smpHoneycomb ? ipHoneycomb :
            this->support_density > 0.95 || this->with_sheath ? ipRectilinear : ipSupportBase;
        this->interface_fill_pattern = (this->interface_density > 0.95 ? ipRectilinear : ipSupportBase);
        this->raft_interface_fill_pattern = this->raft_interface_density > 0.95 ? ipRectilinear : ipSupportBase;
        if (object_config.support_interface_pattern == smipGrid)
            this->contact_fill_pattern = ipGrid;
        else if (object_config.support_interface_pattern == smipRectilinearInterlaced)
            this->contact_fill_pattern = ipRectilinear;
        else
            this->contact_fill_pattern =
            (object_config.support_interface_pattern == smipAuto && slicing_params.soluble_interface) ||
            object_config.support_interface_pattern == smipConcentric ?
            ipConcentric :
            (this->interface_density > 0.95 ? ipRectilinear : ipSupportBase);

        this->raft_angle_1st_layer  = 0.f;
        this->raft_angle_base       = 0.f;
        this->raft_angle_interface  = 0.f;
        if (slicing_params.base_raft_layers > 1) {
            assert(slicing_params.raft_layers() >= 4);
            // 所有筏层类型（第一层、基础层、界面层和接触层）均可用。
            this->raft_angle_1st_layer  = this->interface_angle;
            this->raft_angle_base       = this->base_angle;
            this->raft_angle_interface  = this->interface_angle;
            if ((slicing_params.interface_raft_layers & 1) == 0)
                // 对齐第一筏界面层，使对象第一层与筏接触界面垂直交叉填充。
                this->raft_angle_interface += float(0.5 * M_PI);
        } else if (slicing_params.base_raft_layers == 1 || slicing_params.interface_raft_layers > 1) {
            assert(slicing_params.raft_layers() == 2 || slicing_params.raft_layers() == 3);
            // 第一层、界面层和接触层可用。
            this->raft_angle_1st_layer  = this->base_angle;
            this->raft_angle_interface  = this->interface_angle + 0.5 * M_PI;
        } else if (slicing_params.interface_raft_layers == 1) {
            // 仅接触筏层非空，将作为第一层打印。
            assert(slicing_params.base_raft_layers == 0);
            assert(slicing_params.interface_raft_layers == 1);
            assert(slicing_params.raft_layers() == 1);
            this->raft_angle_1st_layer = float(0.5 * M_PI);
            this->raft_angle_interface = this->raft_angle_1st_layer;
        } else {
            // 无筏层。
            assert(slicing_params.base_raft_layers == 0);
            assert(slicing_params.interface_raft_layers == 0);
            assert(slicing_params.raft_layers() == 0);
        }

	    const auto     nozzle_diameter = print_config.nozzle_diameter.get_at(object_config.support_interface_filament - 1);
        const coordf_t extrusion_width = object_config.line_width.get_abs_value(nozzle_diameter);
        support_extrusion_width        = object_config.support_line_width.get_abs_value(nozzle_diameter);
        support_extrusion_width        = support_extrusion_width > 0 ? support_extrusion_width : extrusion_width;

        independent_layer_height = print_config.independent_support_layer_height;

        // force double walls everywhere if wall count is larger than 1        
        tree_branch_diameter_double_wall_area_scaled = object_config.tree_support_wall_count.value > 1  ? 0.1 :
                                                       object_config.tree_support_wall_count.value == 0 ? 0.25 * sqr(scaled<double>(5.0)) * M_PI :
                                                                                                          std::numeric_limits<double>::max();

        support_style = object_config.support_style;
        if (support_style != smsDefault) {
            if ((support_style == smsSnug || support_style == smsGrid) && is_tree(object_config.support_type)) support_style = smsDefault;
            if ((support_style == smsTreeSlim || support_style == smsTreeStrong || support_style == smsTreeHybrid || support_style == smsTreeOrganic) &&
                !is_tree(object_config.support_type))
                support_style = smsDefault;
        }
        if (support_style == smsDefault) {
            if (is_tree(object_config.support_type)) {
                // Orca: 默认使用有机支撑
                support_style = smsTreeOrganic;
            } else {
                support_style = smsGrid;
            }
        }
    }
	// 顶部/底部接触和界面都是可溶的。
    bool                    soluble_interface;
    // 支撑接触和界面是可溶的，但支撑基础是非可溶的。
    bool                    soluble_interface_non_soluble_base;

    // 是否有至少一个顶部接触层在支撑基础之上挤出？
    bool                    has_top_contacts;
    // 是否有至少一个底部接触层在支撑基础之下挤出？
    bool                    has_bottom_contacts;
    // 顶部界面层数（不计算接触层）。
    size_t                  num_top_interface_layers;
    // 底部界面层数（不计算接触层）。
    size_t                  num_bottom_interface_layers;
    // 顶部基础界面层数。如果不是soluble_interface_non_soluble_base则为零。
    size_t                  num_top_base_interface_layers;
    // 底部基础界面层数。如果不是soluble_interface_non_soluble_base则为零。
    size_t                  num_bottom_base_interface_layers;

    bool                    has_contacts() const { return this->has_top_contacts || this->has_bottom_contacts; }
    bool                    has_interfaces() const { return this->num_top_interface_layers + this->num_bottom_interface_layers > 0; }
    bool                    has_base_interfaces() const { return this->num_top_base_interface_layers + this->num_bottom_base_interface_layers > 0; }
    size_t                  num_top_interface_layers_only() const { return this->num_top_interface_layers - this->num_top_base_interface_layers; }
    size_t                  num_bottom_interface_layers_only() const { return this->num_bottom_interface_layers - this->num_bottom_base_interface_layers; }

	// 第一打印层的流量。
	Flow 					first_layer_flow;
	// 支撑基础的流量（既非顶部也非底部界面）。
	// 也是筏基础的流量（筏界面和接触层除外）。
	Flow 					support_material_flow;
	// 顶部界面和接触层的流量。
	Flow 					support_material_interface_flow;
	// 底部界面和接触层的流量。
	Flow 					support_material_bottom_interface_flow;
	// 筏界面和接触层的流量。
	Flow    				raft_interface_flow;
    coordf_t support_extrusion_width;
	// 是否允许合并区域？界面和基础支撑区域能否使用同一挤出机打印？
	bool 					can_merge_support_regions;

    coordf_t 				support_layer_height_min;
//	coordf_t				support_layer_height_max;

    coordf_t	gap_xy;
    coordf_t	gap_xy_first_layer;

    float    				base_angle;
    float    				interface_angle;
    coordf_t 				interface_spacing;
    coordf_t				support_expansion=0;
    // 顶部/底部界面和接触层的密度。
    coordf_t 				interface_density;
    // 筏界面和接触层的密度。
    coordf_t 				raft_interface_density;
    coordf_t 				support_spacing;
    // 基础支撑层的密度。
    coordf_t 				support_density;
    SupportMaterialStyle    support_style = smsDefault;

    // 稀疏填充的图案，包括稀疏筏层。
    InfillPattern           base_fill_pattern;
    // 顶部/底部界面和接触层的图案。
    InfillPattern           interface_fill_pattern;
    // 筏界面和接触层的图案。
    InfillPattern           raft_interface_fill_pattern;
    // 接触层的图案。
    InfillPattern 			contact_fill_pattern;
    // 稀疏（基础）层是否应使用单条轮廓线（sheath）以提高鲁棒性？
    bool                    with_sheath;
    // 面积大于此阈值的有机支撑分支将使用双线挤出。
    double                  tree_branch_diameter_double_wall_area_scaled = 0.25 * sqr(scaled<double>(5.0)) * M_PI;;

    float 					raft_angle_1st_layer;
    float 					raft_angle_base;
    float 					raft_angle_interface;

    // 为给定的SupportLayer::interface_id()生成筏界面角度
    float 					raft_interface_angle(size_t interface_id) const 
    	{ return this->raft_angle_interface + ((interface_id & 1) ? float(- M_PI / 4.) : float(+ M_PI / 4.)); }
		
    bool independent_layer_height = false;
    const double thresh_big_overhang = Slic3r::sqr(scale_(10));

	bool          ironing;
    Flow          ironing_flow; // Flow at the interface ironing.
    InfillPattern ironing_pattern;
    float         ironing_spacing;
};

} // namespace Slic3r

#endif /* slic3r_SupportParameters_hpp_ */
