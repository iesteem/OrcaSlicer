#ifndef slic3r_Print_hpp_
#define slic3r_Print_hpp_

#include "Fill/FillAdaptive.hpp"
#include "Fill/FillLightning.hpp"
#include "PrintBase.hpp"

#include "BoundingBox.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "Flow.hpp"
#include "Point.hpp"
#include "Slicing.hpp"
#include "TriangleMeshSlicer.hpp"
#include "GCode/ToolOrdering.hpp"
#include "GCode/WipeTower.hpp"
#include "GCode/WipeTower2.hpp"
#include "GCode/ThumbnailData.hpp"
#include "GCode/GCodeProcessor.hpp"
#include "MultiMaterialSegmentation.hpp"
#include "MixedFilament.hpp"
#include "libslic3r.h"

#include <Eigen/Geometry>

#include <functional>
#include <set>
#include <vector>

#include "calib.hpp"

namespace Slic3r {

class GCode;
class Layer;
class ModelObject;
class Print;
class PrintObject;
class SupportLayer;
// BBS
class TreeSupportData;
class TreeSupport;
class PresetCollection;
class PresetBundle;
struct NozzleFilamentRuleMismatch;
struct ExtrusionLayers;

#define MAX_OUTER_NOZZLE_DIAMETER   4
// BBS: move from PrintObjectSlice.cpp
struct VolumeSlices
{
    ObjectID                volume_id;
    std::vector<ExPolygons> slices;
};

struct groupedVolumeSlices
{
    int                     groupId = -1;
    std::vector<ObjectID>   volume_ids;
    ExPolygons              slices;
};

// Phase A local-Z dithering planner cache.
struct LocalZInterval
{
    size_t layer_id { 0 };
    double z_lo { 0.0 };
    double z_hi { 0.0 };
    double base_height { 0.0 };
    double sublayer_height { 0.0 };
    bool   has_mixed_paint { false };
    size_t first_sublayer_idx { 0 };
    size_t sublayer_count { 0 };
};

struct SubLayerPlan
{
    size_t layer_id { 0 };
    size_t pass_index { 0 };
    bool   split_interval { false };
    double z_lo { 0.0 };
    double z_hi { 0.0 };
    double print_z { 0.0 };
    double flow_height { 0.0 };
    size_t dependency_group { 0 };
    size_t dependency_order { 0 };
    std::vector<ExPolygons> painted_masks_by_extruder;
    std::vector<ExPolygons> fixed_painted_masks_by_extruder;
    ExPolygons              base_masks;
};

enum SupportNecessaryType {
    NoNeedSupp=0,
    SharpTail,
    Cantilever,
    LargeOverhang,
};

namespace FillAdaptive {
    struct Octree;
    struct OctreeDeleter;
    using OctreePtr = std::unique_ptr<Octree, OctreeDeleter>;
};

namespace FillLightning {
    class Generator;
    struct GeneratorDeleter;
    using GeneratorPtr = std::unique_ptr<Generator, GeneratorDeleter>;
}; // namespace FillLightning

// 用于跟踪打印状态的打印步骤 ID。
// 打印步骤按此顺序应用。
enum PrintStep {
    psWipeTower,
    // 多材料打印时打印对象上的工具排序。
    // psToolOrdering 是 psWipeTower 的同义词，因为擦洗塔计算和修改工具排序，
    // 而在没有擦洗塔的情况下打印时，工具排序也会被计算。
    psToolOrdering = psWipeTower,
    psSkirtBrim,
    // G-code 导出前的最后一步，此步骤完成后，
    // 应刷新初始挤出路径预览。
    psSlicingFinished = psSkirtBrim,
    psGCodeExport,
    psConflictCheck,
    psCount
};

enum PrintObjectStep {
    posSlice, posPerimeters,posEstimateCurledExtrusions, posPrepareInfill,
    posInfill, posIroning, posSupportMaterial, posSimplifyPath, posSimplifySupportPath,
    // BBS
    posDetectOverhangsForLift,
    posSimplifyWall, posSimplifyInfill,
    posCount,
};

// PrintRegion 对象表示一组共享相同配置（包括相同分配的挤出机）的要打印的体积
class PrintRegion
{
public:
    PrintRegion() = default;
    PrintRegion(const PrintRegionConfig &config);
    PrintRegion(const PrintRegionConfig &config, const size_t config_hash, int print_object_region_id = -1) : m_config(config), m_config_hash(config_hash), m_print_object_region_id(print_object_region_id) {}
    PrintRegion(PrintRegionConfig &&config);
    PrintRegion(PrintRegionConfig &&config, const size_t config_hash, int print_object_region_id = -1) : m_config(std::move(config)), m_config_hash(config_hash), m_print_object_region_id(print_object_region_id) {}
    ~PrintRegion() = default;

// 不修改 PrintRegion 状态的方法：
public:
    const PrintRegionConfig&    config() const throw() { return m_config; }
    size_t                      config_hash() const throw() { return m_config_hash; }
    // 此 PrintRegion 在 Print::m_print_regions 列表中的标识符。
    int                         print_region_id() const throw() { return m_print_region_id; }
    int                         print_object_region_id() const throw() { return m_print_object_region_id; }
	// 此区域和角色的基于 1 的挤出机标识符。
	unsigned int 				extruder(FlowRole role) const;
    Flow                        flow(const PrintObject &object, FlowRole role, double layer_height, bool first_layer = false) const;
    // 参与挤出此区域的喷嘴平均直径。
    coordf_t                    nozzle_dmr_avg(const PrintConfig &print_config) const;
    // 参与挤出此区域的喷嘴平均直径。
    coordf_t                    bridging_height_avg(const PrintConfig &print_config) const;

    // 收集用于打印此区域物体的基于 0 的挤出机索引。
	void                        collect_object_printing_extruders(const Print &print, std::vector<unsigned int> &object_extruders) const;
	static void                 collect_object_printing_extruders(const PrintConfig &print_config, const PrintRegionConfig &region_config, const bool has_brim, std::vector<unsigned int> &object_extruders);

// Methods modifying the PrintRegion's state:
public:
    void                        set_config(const PrintRegionConfig &config) { m_config = config; m_config_hash = m_config.hash(); }
    void                        set_config(PrintRegionConfig &&config) { m_config = std::move(config); m_config_hash = m_config.hash(); }
    void                        config_apply_only(const ConfigBase &other, const t_config_option_keys &keys, bool ignore_nonexistent = false)
                                        { m_config.apply_only(other, keys, ignore_nonexistent); m_config_hash = m_config.hash(); }
private:
    friend Print;
    friend void print_region_ref_inc(PrintRegion&);
    friend void print_region_ref_reset(PrintRegion&);
    friend int  print_region_ref_cnt(const PrintRegion&);

    PrintRegionConfig  m_config;
    size_t             m_config_hash;
    int                m_print_region_id { -1 };
    int                m_print_object_region_id { -1 };
    int                m_ref_cnt { 0 };
};

inline bool operator==(const PrintRegion &lhs, const PrintRegion &rhs) { return lhs.config_hash() == rhs.config_hash() && lhs.config() == rhs.config(); }
inline bool operator!=(const PrintRegion &lhs, const PrintRegion &rhs) { return ! (lhs == rhs); }

template<typename T>
class ConstVectorOfPtrsAdaptor {
public:
    // Returning a non-const pointer to const pointers to T.
    T * const *             begin() const { return m_data->data(); }
    T * const *             end()   const { return m_data->data() + m_data->size(); }
    const T*                front() const { return m_data->front(); }
    // BBS
    const T*                back()  const { return m_data->back(); }
    size_t                  size()  const { return m_data->size(); }
    bool                    empty() const { return m_data->empty(); }
    const T*                operator[](size_t i) const { return (*m_data)[i]; }
    const T*                at(size_t i) const { return m_data->at(i); }
    std::vector<const T*>   vector() const { return std::vector<const T*>(this->begin(), this->end()); }
protected:
    ConstVectorOfPtrsAdaptor(const std::vector<T*> *data) : m_data(data) {}
private:
    const std::vector<T*> *m_data;
};

typedef std::vector<Layer*>       LayerPtrs;
typedef std::vector<const Layer*> ConstLayerPtrs;
class ConstLayerPtrsAdaptor : public ConstVectorOfPtrsAdaptor<Layer> {
    friend PrintObject;
    ConstLayerPtrsAdaptor(const LayerPtrs *data) : ConstVectorOfPtrsAdaptor<Layer>(data) {}
};

typedef std::vector<SupportLayer*>        SupportLayerPtrs;
typedef std::vector<const SupportLayer*>  ConstSupportLayerPtrs;
class ConstSupportLayerPtrsAdaptor : public ConstVectorOfPtrsAdaptor<SupportLayer> {
    friend PrintObject;
    ConstSupportLayerPtrsAdaptor(const SupportLayerPtrs *data) : ConstVectorOfPtrsAdaptor<SupportLayer>(data) {}
};

// PrintObject 的单个实例。
// 由于单个 ModelObject 可能生成多个 PrintObject（其实例在绕 Z 轴旋转上不同），
// ModelObject 的实例将分布在这些多个 PrintObject 之间。
struct PrintInstance
{
    // 父 PrintObject
    PrintObject 		*print_object;
    // 为其创建此 print_object 的 ModelObject 的源 ModelInstance。
	const ModelInstance *model_instance;
	// 将此实例的中心平移到世界坐标的偏移量。
	Point 				 shift;

    BoundingBoxf3   get_bounding_box();
    Polygon get_convex_hull_2d();
    // SoftFever
    //
    // 实例 ID
    size_t               id;
    // Orca: marlin/rrf 取消物体功能使用的唯一 ID
    size_t               unique_id;

    //BBS: instance_shift is too large because of multi-plate, apply without plate offset.
    Point shift_without_plate_offset() const;
};

typedef std::vector<PrintInstance> PrintInstances;

class PrintObjectRegions
{
public:
    // ModelVolume 的边界框，转换到 PrintObject 的工作空间，
    // 可能被层范围修改器裁剪。
    // 只有 Nx16 大小的 Eigen 类型被向量化。此边界框不会被向量化。
    static_assert(sizeof(Eigen::AlignedBox<float, 3>) == 24, "Eigen::AlignedBox<float, 3> is not being vectorized, thus it does not need to be aligned");
    using BoundingBox = Eigen::AlignedBox<float, 3>;
    struct VolumeExtents {
        ObjectID             volume_id;
        BoundingBox          bbox;
    };

    struct VolumeRegion
    {
        // 关联的 ModelVolume 的 ID。
        const ModelVolume   *model_volume { nullptr };
        // 父 VolumeRegion 的索引。
        int                  parent { -1 };
        // 指向 PrintObjectRegions::all_regions 的指针，负体积为 null。
        PrintRegion         *region { nullptr };
        // 指向 VolumeExtents::bbox 的指针。
        const BoundingBox   *bbox { nullptr };
        // 加速相同区域的合并。
        const VolumeRegion  *prev_same_region { nullptr };
    };

    struct PaintedRegion
    {
        // 基于 1 的挤出机标识符。
        unsigned int     extruder_id;
        // 父 VolumeRegion 的索引。
        int              parent { -1 };
        // 指向 PrintObjectRegions::all_regions 的指针。
        PrintRegion     *region { nullptr };
    };

    struct LayerRangeRegions;

    struct FuzzySkinPaintedRegion
    {
        enum class ParentType { VolumeRegion, PaintedRegion };

        ParentType   parent_type { ParentType::VolumeRegion };
        // 父 VolumeRegion 或 PaintedRegion 的索引。
        int          parent { -1 };
        // 指向 PrintObjectRegions::all_regions 的指针。
        PrintRegion *region { nullptr };

        PrintRegion *parent_print_object_region(const LayerRangeRegions &layer_range) const;
        int          parent_print_object_region_id(const LayerRangeRegions &layer_range) const;
    };

    // PrintObject 上的一个切片（可能是整个 PrintObject）以及 ModelVolume 列表及其边界框，
    // 可能被 layer_height_range 裁剪。
    struct LayerRangeRegions
    {
        t_layer_height_range        layer_height_range;
        // 层范围的配置，如果只有一个范围且没有配置覆盖则为 null。
        // 配置由关联的 ModelObject 拥有。
        const DynamicPrintConfig*   config { nullptr };
        // 按 ModelVolume::id() 排序的体积。
        std::vector<VolumeExtents>  volumes;

        // 按源 ModelVolume 的顺序排序，因此反映了区域裁剪、修改器覆盖等的顺序。
        std::vector<VolumeRegion>           volume_regions;
        std::vector<PaintedRegion>          painted_regions;
        std::vector<FuzzySkinPaintedRegion> fuzzy_skin_painted_regions;

        bool has_volume(const ObjectID id) const {
            auto it = lower_bound_by_predicate(this->volumes.begin(), this->volumes.end(), [id](const VolumeExtents &l) { return l.volume_id < id; });
            return it != this->volumes.end() && it->volume_id == id;
        }
    };

    std::vector<std::unique_ptr<PrintRegion>>   all_regions;
    std::vector<LayerRangeRegions>              layer_ranges;
    // 此 ModelObject 到关联 PrintObject 之一的变换（所有从单个 ModelObject 派生的 PrintObject 仅通过 Z 旋转不同）。
    // 此变换用于计算 VolumeExtents。
    Transform3d                                 trafo_bboxes;
    std::vector<ObjectID>                       cached_volume_ids;

    void ref_cnt_inc() { ++ m_ref_cnt; }
    void ref_cnt_dec() { if (-- m_ref_cnt == 0) delete this; }
    void clear() {
        all_regions.clear();
        layer_ranges.clear();
        cached_volume_ids.clear();
    }

private:
    friend class PrintObject;
    // 从同一 ModelObject 生成并共享区域的 PrintObject 数量。
    // ref_cnt 只能由主线程修改，因此不需要是原子的。
    size_t                                      m_ref_cnt{ 0 };
};

class PrintObject : public PrintObjectBaseWithState<Print, PrintObjectStep, posCount>
{
private: // Prevents erroneous use by other classes.
    typedef PrintObjectBaseWithState<Print, PrintObjectStep, posCount> Inherited;

public:
    // 物体大小：缩放坐标中的 XYZ。在 XY 平面中大小可能不完全贴合。
    const Vec3crd&               size() const			{ return m_size; }
    const PrintObjectConfig&     config() const         { return m_config; }
    void                         configBrimWidth(double m)      {m_config.brim_width.value = m; }
    ConstLayerPtrsAdaptor        layers() const         { return ConstLayerPtrsAdaptor(&m_layers); }
    ConstSupportLayerPtrsAdaptor support_layers() const { return ConstSupportLayerPtrsAdaptor(&m_support_layers); }
    const Transform3d&           trafo() const          { return m_trafo; }
    // 在变换后应用 center_offset() 的变换，在切片前将物体在 XY 方向上居中。
    Transform3d                  trafo_centered() const
        { Transform3d t = this->trafo(); t.pretranslate(Vec3d(- unscale<double>(m_center_offset.x()), - unscale<double>(m_center_offset.y()), 0)); return t; }
    const PrintInstances&        instances() const      { return m_instances; }
    PrintInstances &instances() { return m_instances; }

    // 任何获得 PrintObject 非常量指针的人都能修改其层。
    LayerPtrs&                   layers()               { return m_layers; }
    SupportLayerPtrs&            support_layers()       { return m_support_layers; }

    template<typename PolysType>
    static void remove_bridges_from_contacts(
        const Layer* lower_layer,
        const Layer* current_layer,
        float extrusion_width,
        PolysType* overhang_regions,
        float max_bridge_length = scale_(10),
        bool break_bridge=false);

    // 边界框用于对齐物体填充图案，并计算后接缝的吸引子。
    // 边界框可能不完全贴合。
    BoundingBox                  bounding_box() const   { return BoundingBox(Point(- m_size.x() / 2, - m_size.y() / 2), Point(m_size.x() / 2, m_size.y() / 2)); }
    // 高度用于切片、按高度排序物体以进行顺序打印以及检查顺序打印模式下的垂直间隙。
    // 高度是贴合的。
    coord_t 				     height() const         { return m_size.z(); }
    double                      max_z() const         { return m_max_z; }
    // 切片网格相对于缩放和旋转后的模型网格的居中偏移。
    const Point& 			     center_offset() const  { return m_center_offset; }

    // BBS
    void generate_support_preview();
    const std::vector<VolumeSlices>& firstLayerObjSlice() const { return firstLayerObjSliceByVolume; }
    std::vector<VolumeSlices>& firstLayerObjSliceMod() { return firstLayerObjSliceByVolume; }
    const std::vector<groupedVolumeSlices>& firstLayerObjGroups() const { return firstLayerObjSliceByGroups; }
    std::vector<groupedVolumeSlices>& firstLayerObjGroupsMod() { return firstLayerObjSliceByGroups; }

    bool                         has_brim() const       {
        return ((this->config().brim_type != btNoBrim && this->config().brim_width.value > 0.) || this->config().brim_type == btAutoBrim
            || (this->config().brim_type == btPainted && !this->model_object()->brim_points.empty()))
            && ! this->has_raft();
    }

    // BBS
    const ExtrusionEntityCollection& object_skirt() const {
        return m_skirt;
    }

    // 这是*总*层数（包括支撑层）
    // 此值不应与 Layer::id 比较，因为它们的语义不同。
    size_t 			total_layer_count() const { return this->layer_count() + this->support_layer_count(); }
    size_t 			layer_count() const { return m_layers.size(); }
    void 			clear_layers();
    const Layer* 	get_layer(int idx) const { return m_layers[idx]; }
    Layer* 			get_layer(int idx) 		 { return m_layers[idx]; }
    // 获取精确位于 print_z 的层。
    const Layer*	get_layer_at_printz(coordf_t print_z) const;
    Layer*			get_layer_at_printz(coordf_t print_z);
    // 获取近似位于 print_z 的层。
    const Layer*	get_layer_at_printz(coordf_t print_z, coordf_t epsilon) const;
    Layer*			get_layer_at_printz(coordf_t print_z, coordf_t epsilon);
    int             get_layer_idx_get_printz(coordf_t print_z, coordf_t epsilon);
    // BBS
    const Layer*    get_layer_at_bottomz(coordf_t bottom_z, coordf_t epsilon) const;
    Layer*          get_layer_at_bottomz(coordf_t bottom_z, coordf_t epsilon);

    // 获取大致在 print_z 下方的第一层。
    const Layer*	get_first_layer_bellow_printz(coordf_t print_z, coordf_t epsilon) const;

    // print_z: 层的顶部；slice_z: 层的中心。
    Layer*          add_layer(int id, coordf_t height, coordf_t print_z, coordf_t slice_z);

    // BBS
    SupportLayer* add_tree_support_layer(int id, coordf_t height, coordf_t print_z, coordf_t slice_z);
    std::shared_ptr<TreeSupportData> alloc_tree_support_preview_cache();
    void clear_tree_support_preview_cache() { m_tree_support_preview_cache.reset(); }
    const std::vector<LocalZInterval>& local_z_intervals() const { return m_local_z_intervals; }
    const std::vector<SubLayerPlan>&   local_z_sublayer_plan() const { return m_local_z_sublayer_plan; }
    void                                set_local_z_plan(std::vector<LocalZInterval> intervals, std::vector<SubLayerPlan> sublayers)
    {
        m_local_z_intervals = std::move(intervals);
        m_local_z_sublayer_plan = std::move(sublayers);
    }
    void                                clear_local_z_plan()
    {
        m_local_z_intervals.clear();
        m_local_z_sublayer_plan.clear();
    }

    size_t          support_layer_count() const { return m_support_layers.size(); }
    void            clear_support_layers();
    SupportLayer*   get_support_layer(int idx) { return idx<m_support_layers.size()? m_support_layers[idx]:nullptr; }
    const SupportLayer* get_support_layer_at_printz(coordf_t print_z, coordf_t epsilon) const;
    SupportLayer*   get_support_layer_at_printz(coordf_t print_z, coordf_t epsilon);
    SupportLayer*   add_support_layer(int id, int interface_id, coordf_t height, coordf_t print_z);
    SupportLayerPtrs::iterator insert_support_layer(SupportLayerPtrs::iterator pos, size_t id, size_t interface_id, coordf_t height, coordf_t print_z, coordf_t slice_z);

    // 从 model_object 的 layer_height_profile、model_object 的层高表或切片参数初始化 layer_height_profile。
    // 如果 layer_height_profile 已更改，则返回 true。
    static bool     update_layer_height_profile(const ModelObject &model_object,
                                                const SlicingParameters &slicing_parameters,
                                                std::vector<coordf_t> &layer_height_profile,
                                                const PrintObject *print_object = nullptr);

    // 收集切片参数，供可变层厚算法、交互式层高编辑器和打印过程本身使用。
    // 切片参数依赖于各种配置值（层高、第一层层高、筏垫设置、打印喷嘴直径等）。
    const SlicingParameters&    slicing_parameters() const { return m_slicing_params; }
    // Orca: XYZ shrinkage compensation has introduced the const Vec3d &object_shrinkage_compensation parameter to the function below
    static SlicingParameters    slicing_parameters(const DynamicPrintConfig &full_config, const ModelObject &model_object, float object_max_z, const Vec3d &object_shrinkage_compensation);

    size_t                      num_printing_regions() const throw() { return m_shared_regions->all_regions.size(); }
    const PrintRegion&          printing_region(size_t idx) const throw() { return *m_shared_regions->all_regions[idx].get(); }
    //FIXME 在切片前返回所有可能的区域，因此某些区域可能最终不会被切片。
    std::vector<std::reference_wrapper<const PrintRegion>> all_regions() const;
    const PrintObjectRegions*   shared_regions() const throw() { return m_shared_regions; }

    bool                        has_support()           const { return m_config.enable_support || m_config.enforce_support_layers > 0; }
    bool                        has_raft()              const { return m_config.raft_layers > 0; }
    bool                        has_support_material()  const { return this->has_support() || this->has_raft(); }
    // 检查模型对象是否使用多材料绘制工具进行了绘制。
    bool                        is_mm_painted()         const { return this->model_object()->is_mm_painted(); }
    // 检查模型对象是否使用毛绒皮肤绘制工具进行了绘制。
    bool                        is_fuzzy_skin_painted() const { return this->model_object()->is_fuzzy_skin_painted(); }

    // 返回用于打印物体的基于 0 的挤出机索引（不含裙边、支撑和其他辅助挤出）
    std::vector<unsigned int>   object_extruders() const;

    // 由 make_perimeters() 调用
    void slice();

    // 辅助函数：由支撑生成器对支撑强制执行/阻挡网格进行切片。
    std::vector<Polygons>       slice_support_volumes(const ModelVolumeType model_volume_type) const;
    std::vector<Polygons>       slice_support_blockers() const { return this->slice_support_volumes(ModelVolumeType::SUPPORT_BLOCKER); }
    std::vector<Polygons>       slice_support_enforcers() const { return this->slice_support_volumes(ModelVolumeType::SUPPORT_ENFORCER); }

    // 辅助函数：在切片上投影自定义面
    void project_and_append_custom_facets(bool seam, EnforcerBlockerType type, std::vector<Polygons>& expolys, std::vector<std::pair<Vec3f,Vec3f>>* vertical_points=nullptr) const;

    //BBS
    BoundingBox get_first_layer_bbox(float& area, float& layer_height, std::string& name);
    void         get_certain_layers(float start, float end, std::vector<LayerPtrs> &out, std::vector<BoundingBox> &boundingbox_objects);
    Points       get_instances_shift_without_plate_offset();
    PrintObject* get_shared_object() const { return m_shared_object; }
    void         set_shared_object(PrintObject *object);
    void         clear_shared_object();
    void         copy_layers_from_shared_object();
    void         copy_layers_overhang_from_shared_object();

    // BBS: Boundingbox of the first layer
    BoundingBox                 firstLayerObjectBrimBoundingBox;

    // BBS: returns 1-based indices of extruders used to print the first layer wall of objects
    std::vector<int>            object_first_layer_wall_extruders;

    // SoftFever
    size_t get_id() const { return m_id; }
    void set_id(size_t id) { m_id = id; }

  private:
    // 仅由 Print 调用。
    friend class Print;

	PrintObject(Print* print, ModelObject* model_object, const Transform3d& trafo, PrintInstances&& instances);
	~PrintObject();

    void                    config_apply(const ConfigBase &other, bool ignore_nonexistent = false) { m_config.apply(other, ignore_nonexistent); }
    void                    config_apply_only(const ConfigBase &other, const t_config_option_keys &keys, bool ignore_nonexistent = false) { m_config.apply_only(other, keys, ignore_nonexistent); }
    PrintBase::ApplyStatus  set_instances(PrintInstances &&instances);
    // 使步骤及其在 PrintObject 和 Print 中的依赖步骤失效。
    bool                    invalidate_step(PrintObjectStep step);
    // 使所有 PrintObject 和 Print 步骤失效。
    bool                    invalidate_all_steps();
    // 根据一组已更改的参数使步骤失效。
    // 可能为 PrintObjectConfig 和 PrintRegionConfig 调用。
    bool                    invalidate_state_by_config_options(
        const ConfigOptionResolver &old_config, const ConfigOptionResolver &new_config, const std::vector<t_config_option_key> &opt_keys);
    // 如果 ! m_slicing_params.valid，则重新计算。
    void                    update_slicing_parameters();

    static PrintObjectConfig object_config_from_model_object(const PrintObjectConfig &default_object_config, const ModelObject &object, size_t num_extruders);

private:
    void make_perimeters();
    void prepare_infill();
    void infill();
    void ironing();
    void generate_support_material();
    void estimate_curled_extrusions();
    void simplify_extrusion_path();

    void slice_volumes();
    //BBS
    ExPolygons _shrink_contour_holes(double contour_delta, double hole_delta, const ExPolygons& polys) const;
    // BBS
    void detect_overhangs_for_lift();
    void clear_overhangs_for_lift();

   void _transform_hole_to_polyholes();

    // 是否有任何支撑（不计算筏垫）。
    void detect_surfaces_type();
    void process_external_surfaces();
    void discover_vertical_shells();
    void bridge_over_infill();
    void clip_fill_surfaces();
    void discover_horizontal_shells();
    void combine_infill();
    void _generate_support_material();
    std::pair<FillAdaptive::OctreePtr, FillAdaptive::OctreePtr> prepare_adaptive_infill_data(
        const std::vector<std::pair<const Surface*, float>>& surfaces_w_bottom_z) const;
    FillLightning::GeneratorPtr prepare_lightning_infill_data();

    // BBS
    SupportNecessaryType is_support_necessary();

    // 缩放坐标中的 XYZ
    Vec3crd									m_size;
    double                                  m_max_z;
    PrintObjectConfig                       m_config;
    // Z 平移 + 旋转 + 缩放/镜像。
    Transform3d                             m_trafo = Transform3d::Identity();
    // 缩放 G-code 坐标中的 Slic3r::Point 对象
    std::vector<PrintInstance>              m_instances;
    // 网格在传递给 Clipper 之前被居中，以便 Clipper 的固定坐标需要更少的位数。
    // 这是物体坐标系向 PrintObject 坐标系的调整。
    Point                                   m_center_offset;

    // 物体被分割为层范围和区域及其关联的配置。
    // 在为同一 ModelObject 创建的 PrintObject 之间共享。
    PrintObjectRegions                     *m_shared_regions { nullptr };

    SlicingParameters                       m_slicing_params;
    LayerPtrs                               m_layers;
    SupportLayerPtrs                        m_support_layers;
    std::vector<LocalZInterval>             m_local_z_intervals;
    std::vector<SubLayerPlan>               m_local_z_sublayer_plan;
    // BBS
    std::shared_ptr<TreeSupportData>        m_tree_support_preview_cache;

    // 当 LayerRegion->slices 被分割为顶部/内部/底部时，此项设置为 true
    // 以便下一次调用 make_perimeters() 在计算循环之前执行 union()
    bool                    				m_typed_slices = false;

    std::pair<FillAdaptive::OctreePtr, FillAdaptive::OctreePtr> m_adaptive_fill_octrees;
    FillLightning::GeneratorPtr m_lightning_generator;

    std::vector < VolumeSlices >            firstLayerObjSliceByVolume;
    std::vector<groupedVolumeSlices>        firstLayerObjSliceByGroups;

    // BBS: per object skirt
    ExtrusionEntityCollection               m_skirt;

    PrintObject*                            m_shared_object{ nullptr };

    
    // SoftFever
    // 
    // object id
    size_t               m_id;
    void apply_conical_overhang();

 public:
    //BBS: When printing multi-material objects, this settings will make slicer to clip the overlapping object parts one by the other.
    //(2nd part will be clipped by the 1st, 3rd part will be clipped by the 1st and 2nd etc).
    // This was a per-object setting and now we default enable it.
    static bool clip_multipart_objects;
    static bool infill_only_where_needed;
};

struct FakeWipeTower
{
    // 生成虚拟挤出
    Vec2f pos;
    float width;
    float height;
    float layer_height;
    float depth;
    std::vector<std::pair<float, float>> z_and_depth_pairs;
    float brim_width;
    float rotation_angle;
    float cone_angle;
    Vec2d plate_origin;
    std::map<float, Polylines> outer_wall;

    void set_fake_extrusion_data(Vec2f p, float w, float h, float lh, float d, float bd, Vec2d o)
    {
        pos          = p;
        width        = w;
        height       = h;
        layer_height = lh;
        depth        = d;
        brim_width   = bd;
        plate_origin = o;
    }
    void set_fake_extrusion_data(const Vec2f& p, float w, float h, float lh, float d, const std::vector<std::pair<float, float>>& zad, float bd, float ra, float ca, const Vec2d& o)
    {
        pos = p;
        width = w;
        height = h;
        layer_height = lh;
        depth = d;
        z_and_depth_pairs = zad;
        brim_width = bd;
        rotation_angle = ra;
        cone_angle = ca;
        plate_origin = o;
    }
    void set_pos(Vec2f p) { pos = p; }
    void set_pos_and_rotation(const Vec2f& p, float rotation) { pos = p; rotation_angle = rotation; }

    std::vector<ExtrusionPaths> getFakeExtrusionPathsFromWipeTower() const
    {
        int   d         = scale_(depth);
        int   w         = scale_(width);
        int   bd        = scale_(brim_width);
        Point minCorner = {scale_(pos.x()), scale_(pos.y())};
        Point maxCorner = {minCorner.x() + w, minCorner.y() + d};

        std::vector<ExtrusionPaths> paths;
        for (float h = 0.f; h < height; h += layer_height) {
            ExtrusionPath path(ExtrusionRole::erWipeTower, 0.0, 0.0, layer_height);
            path.polyline = {minCorner, {maxCorner.x(), minCorner.y()}, maxCorner, {minCorner.x(), maxCorner.y()}, minCorner};
            paths.push_back({path});

            if (h == 0.f) { // add brim
                ExtrusionPath fakeBrim(ExtrusionRole::erBrim, 0.0, 0.0, layer_height);
                Point         wtbminCorner = {minCorner - Point{bd, bd}};
                Point         wtbmaxCorner = {maxCorner + Point{bd, bd}};
                fakeBrim.polyline          = {wtbminCorner, {wtbmaxCorner.x(), wtbminCorner.y()}, wtbmaxCorner, {wtbminCorner.x(), wtbmaxCorner.y()}, wtbminCorner};
                paths.back().push_back(fakeBrim);
            }
        }
        return paths;
    }

    std::vector<ExtrusionPaths> getFakeExtrusionPathsFromWipeTower2() const
    {
        float h = height;
        float lh = layer_height;
        int   d = scale_(depth);
        int   w = scale_(width);
        int   bd = scale_(brim_width);
        Point minCorner = { -bd, -bd };
        Point maxCorner = { minCorner.x() + w + bd, minCorner.y() + d + bd };

        const auto [cone_base_R, cone_scale_x] = WipeTower2::get_wipe_tower_cone_base(width, height, depth, cone_angle);

        std::vector<ExtrusionPaths> paths;
        for (float hh = 0.f; hh < h; hh += lh) {
            
            if (hh != 0.f) {
                // The wipe tower may be getting smaller. Find the depth for this layer.
                size_t i = 0;
                for (i=0; i<z_and_depth_pairs.size()-1; ++i)
                    if (hh >= z_and_depth_pairs[i].first && hh < z_and_depth_pairs[i+1].first)
                        break;
                d = scale_(z_and_depth_pairs[i].second);
                minCorner = {0.f, -d/2 + scale_(z_and_depth_pairs.front().second/2.f)};
                maxCorner = { minCorner.x() + w, minCorner.y() + d };
            }


            ExtrusionPath path(ExtrusionRole::erWipeTower, 0.0, 0.0, lh);
            path.polyline = { minCorner, {maxCorner.x(), minCorner.y()}, maxCorner, {minCorner.x(), maxCorner.y()}, minCorner };
            paths.push_back({ path });

            // We added the border, now add several parallel lines so we can detect an object that is fully inside the tower.
            // For now, simply use fixed spacing of 3mm.
            for (coord_t y=minCorner.y()+scale_(3.); y<maxCorner.y(); y+=scale_(3.)) {
                path.polyline = { {minCorner.x(), y}, {maxCorner.x(), y} };
                paths.back().emplace_back(path);
            }

            // And of course the stabilization cone and its base...
            if (cone_base_R > 0.) {
                path.polyline.clear();
                double r = cone_base_R * (1 - hh/height);
                for (double alpha=0; alpha<2.01*M_PI; alpha+=2*M_PI/20.)
                    path.polyline.points.emplace_back(Point::new_scale(width/2. + r * std::cos(alpha)/cone_scale_x, depth/2. + r * std::sin(alpha)));
                paths.back().emplace_back(path);
                if (hh == 0.f) { // Cone brim.
                    for (float bw=brim_width; bw>0.f; bw-=3.f) {
                        path.polyline.clear();
                        for (double alpha=0; alpha<2.01*M_PI; alpha+=2*M_PI/20.) // see load_wipe_tower_preview, where the same is a bit clearer
                            path.polyline.points.emplace_back(Point::new_scale(
                                width/2. + cone_base_R * std::cos(alpha)/cone_scale_x * (1. + cone_scale_x*bw/cone_base_R),
                                depth/2. + cone_base_R * std::sin(alpha) * (1. + bw/cone_base_R))
                            );
                        paths.back().emplace_back(path);
                    }
                }
            }

            // Only the first layer has brim.
            if (hh == 0.f) {
                minCorner = minCorner + Point(bd, bd);
                maxCorner = maxCorner - Point(bd, bd);
            }
        }

        // Rotate and translate the tower into the final position.
        for (ExtrusionPaths& ps : paths) {
            for (ExtrusionPath& p : ps) {
                p.polyline.rotate(Geometry::deg2rad(rotation_angle));
                p.polyline.translate(scale_(pos.x()), scale_(pos.y()));
            }
        }

        return paths;
    }

    ExtrusionLayers getTrueExtrusionLayersFromWipeTower() const;
};

struct WipeTowerData
{
    // 以下部分将由 GCodeGenerator 消费。
    // 必须知道非顺序打印的工具排序才能计算擦洗塔。
    // 在此缓存，以便在 G-code 生成期间不需要重新计算。
    ToolOrdering                                         &tool_ordering;
    // 每打印层的工具更换缓存。
    std::unique_ptr<std::vector<WipeTower::ToolChangeResult>> priming;
    std::vector<std::vector<WipeTower::ToolChangeResult>> tool_changes;
    std::vector<std::vector<WipeTower::ToolChangeResult>> local_z_tool_changes;
    std::unique_ptr<WipeTower::ToolChangeResult>          final_purge;
    std::vector<float>                                    used_filament;
    int                                                   number_of_toolchanges;

    // 擦洗塔的深度，传递给 GLCanvas3D 以获取精确的边界框：
    float                                                 depth;
    std::vector<std::pair<float, float>>                  z_and_depth_pairs;
    std::vector<std::vector<WipeTower::box_coordinates>>  local_z_reserve_boxes;
    float                                                 brim_width;
    float                                                 height;

    void clear() {
        priming.reset(nullptr);
        tool_changes.clear();
        local_z_tool_changes.clear();
        final_purge.reset(nullptr);
        used_filament.clear();
        number_of_toolchanges = -1;
        depth = 0.f;
        local_z_reserve_boxes.clear();
        brim_width = 0.f;
    }

private:
	// 只允许 Print 内部实例化 WipeTowerData，
	// 因为此 WipeTowerData 共享对 Print::m_tool_ordering 的引用。
	friend class Print;
	WipeTowerData(ToolOrdering &tool_ordering) : tool_ordering(tool_ordering) { clear(); }
	WipeTowerData(const WipeTowerData & /* rhs */) = delete;
	WipeTowerData &operator=(const WipeTowerData & /* rhs */) = delete;
};

struct PrintStatistics
{
    PrintStatistics() { clear(); }
    std::string                     estimated_normal_print_time;
    std::string                     estimated_silent_print_time;
    double                          total_used_filament;
    double                          total_extruded_volume;
    double                          total_cost;
    int                             total_toolchanges;
    double                          total_weight;
    double                          total_wipe_tower_cost;
    double                          total_wipe_tower_filament;
    unsigned int                    initial_tool;
    std::map<size_t, double>        filament_stats;

    // 包含已填充打印统计信息的配置。
    DynamicConfig           config() const;
    // 包含已用占位符字符串填充统计键的配置。
    static DynamicConfig    placeholders();
    // 替换路径中的打印统计占位符。
    std::string             finalize_output_path(const std::string &path_in) const;

    void clear() {
        total_used_filament    = 0.;
        total_extruded_volume  = 0.;
        total_cost             = 0.;
        total_toolchanges      = 0;
        total_weight           = 0.;
        total_wipe_tower_cost  = 0.;
        total_wipe_tower_filament = 0.;
        initial_tool           = 0;
        filament_stats.clear();
    }
    static const std::string FilamentUsedG;
    static const std::string FilamentUsedGMask;
    static const std::string TotalFilamentUsedG;
    static const std::string TotalFilamentUsedGMask;
    static const std::string TotalFilamentUsedGValueMask;
    static const std::string FilamentUsedCm3;
    static const std::string FilamentUsedCm3Mask;
    static const std::string FilamentUsedMm;
    static const std::string FilamentUsedMmMask;
    static const std::string FilamentCost;
    static const std::string FilamentCostMask;
    static const std::string TotalFilamentCost;
    static const std::string TotalFilamentCostMask;
    static const std::string TotalFilamentCostValueMask;
    static const std::string TotalFilamentUsedWipeTower;
    static const std::string TotalFilamentUsedWipeTowerValueMask;
    
};

typedef std::vector<PrintObject*>       PrintObjectPtrs;
typedef std::vector<const PrintObject*> ConstPrintObjectPtrs;
class ConstPrintObjectPtrsAdaptor : public ConstVectorOfPtrsAdaptor<PrintObject> {
    friend Print;
    ConstPrintObjectPtrsAdaptor(const PrintObjectPtrs *data) : ConstVectorOfPtrsAdaptor<PrintObject>(data) {}
};

typedef std::vector<PrintRegion*>       PrintRegionPtrs;
/*
typedef std::vector<const PrintRegion*> ConstPrintRegionPtrs;
class ConstPrintRegionPtrsAdaptor : public ConstVectorOfPtrsAdaptor<PrintRegion> {
    friend Print;
    ConstPrintRegionPtrsAdaptor(const PrintRegionPtrs *data) : ConstVectorOfPtrsAdaptor<PrintRegion>(data) {}
};
*/

enum FilamentTempType {
    HighTemp=0,
    LowTemp,
    HighLowCompatible,
    Undefine
};
// 完整的打印托盘，可能包含多个物体。
class Print : public PrintBaseWithState<PrintStep, psCount>
{
private: // 防止被其他类错误使用。
    typedef PrintBaseWithState<PrintStep, psCount> Inherited;
    // Bool 指示 PrintObject 的支撑是否为顶层轮廓。
    typedef std::pair<PrintObject *, bool>         PrintObjectInfo;

public:
    Print() = default;
	virtual ~Print() { this->clear(); }

	PrinterTechnology	technology() const noexcept override { return ptFFF; }

    // 更改 Print / PrintObject / PrintRegion 状态的方法。
    // 以下方法与 process() 和 export_gcode() 同步，
    // 因此 process() 和 export_gcode() 可以从后台线程调用。
    // 如果以下方法需要修改由 process() 或 export_gcode() 处理的数据，
    // 则在操作之前执行取消回调以停止后台处理。
    void                clear() override;
    bool                empty() const override { return m_objects.empty(); }
    // 现有 PrintObject ID 列表，用于移除不存在的 ID 的通知。
    std::vector<ObjectID> print_object_ids() const override;

    ApplyStatus         apply(const Model &model, DynamicPrintConfig config) override;

    void                process(long long *time_cost_with_cache = nullptr, bool use_cache = false) override;
    // 根据 path_template 将 G-code 导出到文件名，返回生成的 G-code 文件的路径。
    // 如果 preview_data 不为 null，则填充 preview_data 用于 G-code 可视化（命令行 Slic3r 不使用）。
    std::string         export_gcode(const std::string& path_template, GCodeProcessorResult* result, ThumbnailsGeneratorCallback thumbnail_cb = nullptr);
    // 返回 0 表示成功
    int                 export_cached_data(const std::string& dir_path, bool with_space=false);
    int                 load_cached_data(const std::string& directory);

    // 处理状态的方法
    bool                is_step_done(PrintStep step) const { return Inherited::is_step_done(step); }
    // 如果所有物体上的物体步骤都已完成且至少有一个物体，则返回 true。
    bool                is_step_done(PrintObjectStep step) const;
    // 如果最后一步成功完成，则返回 true。
    bool                finished() const override { return this->is_step_done(psGCodeExport); }

    bool                has_infinite_skirt() const;
    bool                has_skirt() const;
    bool                has_brim() const;
    //BBS
    bool                has_auto_brim() const    {
        return std::any_of(m_objects.begin(), m_objects.end(), [](PrintObject* object) { return object->config().brim_type == btAutoBrim; });
    }

    // 如果有效则返回空字符串，否则返回错误信息。
    StringObjectException validate(StringObjectException *warning = nullptr, Polygons* collison_polygons = nullptr, std::vector<std::pair<Polygon, float>>* height_polygons = nullptr) const override;
    double              skirt_first_layer_height() const;
    Flow                brim_flow() const;
    Flow                skirt_flow() const;

    std::vector<unsigned int> object_extruders() const;
    std::vector<unsigned int> support_material_extruders() const;
    std::vector<unsigned int> extruders(bool conside_custom_gcode = false) const;
    // 按需评估与 filament_hot_bed_nozzles.json 的比较（内部调用一次 extruders(true)）。
    void                filament_rule_mismatch_flags(NozzleFilamentRuleMismatch& out_nozzle_mismatch,
                                                     bool& out_gesp,
                                                     bool& out_pei_not_pla,
                                                     bool& out_pei_tpu,
                                                     const PresetBundle* preset_bundle = nullptr) const;
    
    double              max_allowed_layer_height() const;
    bool                has_support_material() const;
    // 确保在此调用期间后台处理无法访问此 model_object！
    void                auto_assign_extruders(ModelObject* model_object) const;

    const PrintConfig&          config() const { return m_config; }
    const PrintObjectConfig&    default_object_config() const { return m_default_object_config; }
    const PrintRegionConfig& default_region_config() const { return m_default_region_config; }
    const MixedFilamentManager& mixed_filament_manager() const { return m_mixed_filament_mgr; }
    MixedFilamentManager&       mixed_filament_manager()       { return m_mixed_filament_mgr; }
    ConstPrintObjectPtrsAdaptor objects() const { return ConstPrintObjectPtrsAdaptor(&m_objects); }
    PrintObject*                get_object(size_t idx) { return const_cast<PrintObject*>(m_objects[idx]); }
    const PrintObject*          get_object(size_t idx) const { return m_objects[idx]; }
    // 通过 ObjectID 获取 PrintObject，用于在通知中心中唯一地将切片警告绑定到其源 PrintObject。
    const PrintObject*          get_object(ObjectID object_id) const {
        auto it = std::find_if(m_objects.begin(), m_objects.end(),
            [object_id](const PrintObject *obj) { return obj->id() == object_id; });
        return (it == m_objects.end()) ? nullptr : *it;
    }
    //BBS: Function to get m_brimMap;
    std::map<ObjectID, ExtrusionEntityCollection>&
        get_brimMap() { return m_brimMap; }

    // 所有打印对象中 PrintObject::copies() 的数量是多少？
    // 如果为零，则打印为空，不应执行打印。
    unsigned int                num_object_instances() const;

    // For Perl bindings.
    PrintObjectPtrs&            objects_mutable() { return m_objects; }
    PrintRegionPtrs&            print_regions_mutable() { return m_print_regions; }
    std::vector<size_t>         layers_sorted_for_object(float start, float end, std::vector<LayerPtrs> &layers_of_objects, std::vector<BoundingBox> &boundingBox_for_objects, VecOfPoints& objects_instances_shift);
    const ExtrusionEntityCollection& skirt() const { return m_skirt; }
    // 第一层挤出的凸包，用于调平和放置初始清洗线。
    // 它包含物体挤出、支撑挤出、裙边、裙板、擦洗塔。
    // 它不包含由自定义 G-code 生成的用户挤出，
    // 因此它不包含初始清洗线。
    // 它不包含 MMU/MMU2 启动（清洗）区域。
    const Polygon&                   first_layer_convex_hull() const { return m_first_layer_convex_hull; }

    const PrintStatistics&      print_statistics() const { return m_print_statistics; }
    PrintStatistics&            print_statistics() { return m_print_statistics; }

    // Wipe tower support.
    bool                        has_wipe_tower() const;
    const WipeTowerData&        wipe_tower_data(size_t filaments_cnt = 0) const;
    const ToolOrdering& 		tool_ordering() const { return m_tool_ordering; }

    bool                        enable_timelapse_print() const;

	std::string                 output_filename(const std::string &filename_base = std::string()) const override;

	std::string                 get_model_name() const;
	std::string                 get_plate_number_formatted() const;

    size_t                      num_print_regions() const throw() { return m_print_regions.size(); }
    const PrintRegion&          get_print_region(size_t idx) const  { return *m_print_regions[idx]; }
    const ToolOrdering&         get_tool_ordering() const { return m_wipe_tower_data.tool_ordering; }
    const FakeWipeTower& get_fake_wipe_tower() const { return m_fake_wipe_tower; }

    //BBS: plate's origin related functions
    void set_plate_origin(Vec3d origin) { m_origin = origin; }
    const Vec3d get_plate_origin() const { return m_origin; }
    //BBS: export gcode from previous gcode file from 3mf
    void set_gcode_file_ready();
    void set_gcode_file_invalidated();
    void export_gcode_from_previous_file(const std::string& file, GCodeProcessorResult* result, ThumbnailsGeneratorCallback thumbnail_cb = nullptr);
    //BBS: add modify_count logic
    int get_modified_count() const {return m_modified_count;}
    //BBS: add status for whether support used
    bool is_support_used() const {return m_support_used;}
    std::string get_conflict_string() const
    {
        std::string result;
        if (m_conflict_result) {
            result = "Found gcode path conflicts between object " + m_conflict_result.value()._objName1 + " and " + m_conflict_result.value()._objName2;
        }

        return result;
    }

    //BBS
    static StringObjectException sequential_print_clearance_valid(const Print &print, Polygons *polygons = nullptr, std::vector<std::pair<Polygon, float>>* height_polygons = nullptr);
    ConflictResultOpt            get_conflict_result() const { return m_conflict_result; }

    // Return 4 wipe tower corners in the world coordinates (shifted and rotated), including the wipe tower brim.
    Points first_layer_wipe_tower_corners(bool check_wipe_tower_existance=true) const;

    //SoftFever
    bool &is_BBL_printer() { return m_isBBLPrinter; }
    const bool is_BBL_printer() const { return m_isBBLPrinter; }
    CalibMode& calib_mode() { return m_calib_params.mode; }
    const CalibMode calib_mode() const { return m_calib_params.mode; }
    void set_calib_params(const Calib_Params& params);
    const Calib_Params& calib_params() const { return m_calib_params; }
    Vec2d translate_to_print_space(const Vec2d &point) const;
    // scaled point
    Vec2d translate_to_print_space(const Point &point) const;
    static FilamentTempType get_filament_temp_type(const std::string& filament_type);
    static int get_hrc_by_nozzle_type(const NozzleType& type);
    static bool check_multi_filaments_compatibility(const std::vector<std::string>& filament_types);
    // similar to check_multi_filaments_compatibility, but the input is int, and may be negative (means unset)
    static bool is_filaments_compatible(const std::vector<int>& types);
    // get the compatible filament type of a multi-material object
    // Rule:
    // 1. LowTemp+HighLowCompatible=LowTemp
    // 2. HighTemp++HighLowCompatible=HighTemp
    // 3. LowTemp+HighTemp+...=HighLowCompatible
    // Unset types are just ignored.
    static int get_compatible_filament_type(const std::set<int>& types);

    bool is_all_objects_are_short() const {
        return std::all_of(this->objects().begin(), this->objects().end(), [&](PrintObject* obj) { return obj->height() < scale_(this->config().nozzle_height.value); });
    }
    
    // Orca: 实现 prusa 的耗材收缩补偿方法
    // 返回是否所有使用的耗材具有相同的收缩补偿。
     bool has_same_shrinkage_compensations() const;
    // 返回每个轴的缩放比例，表示每个轴的收缩补偿。
     Vec3d shrinkage_compensation() const;

    std::tuple<float, float> object_skirt_offset(double margin_height = 0) const;

protected:
    // 使步骤及其在 Print 中的依赖步骤失效。
    bool                invalidate_step(PrintStep step);

private:
    //BBS
    static StringObjectException check_multi_filament_valid(const Print &print);

    bool                invalidate_state_by_config_options(const ConfigOptionResolver &new_config, const std::vector<t_config_option_key> &opt_keys);

    void                _make_skirt();
    void                _make_wipe_tower();
    void                finalize_first_layer_convex_hull();

    // 第一层挤出的物体及其支撑的孤岛。
    Polygons            first_layer_islands() const;

    PrintConfig                             m_config;
    PrintObjectConfig                       m_default_object_config;
    PrintRegionConfig                       m_default_region_config;
    MixedFilamentManager                    m_mixed_filament_mgr;
    PrintObjectPtrs                         m_objects;
    PrintRegionPtrs                         m_print_regions;
    
    //SoftFever
    bool m_isBBLPrinter;

    // 用于构建裙边环和裙板的挤出路径的有序集合。
    ExtrusionEntityCollection               m_skirt;
    // BBS: 按物体收集挤出路径以构建裙板
    std::map<ObjectID, ExtrusionEntityCollection>         m_brimMap;
    std::map<ObjectID, ExtrusionEntityCollection>         m_supportBrimMap;
    // 第一层挤出的凸包。
    // 它包含物体挤出、支撑挤出、裙边、裙板、擦洗塔。
    // 它不包含由自定义 G-code 生成的用户挤出，
    // 因此它不包含初始清洗线。
    // 它不包含 MMU/MMU2 启动（清洗）区域。
    Polygon                                 m_first_layer_convex_hull;
    Points                                  m_skirt_convex_hull;

    // 以下部分将由 GCodeGenerator 消费。
    ToolOrdering 							m_tool_ordering;
    WipeTowerData                           m_wipe_tower_data {m_tool_ordering};

    // 预估打印时间，耗材消耗量。
    PrintStatistics                         m_print_statistics;
    bool                                    m_support_used {false};

    //BBS: plate's origin
    Vec3d   m_origin;
    //BBS: modified_count
    int     m_modified_count {0};
    //BBS
    ConflictResultOpt m_conflict_result;
    FakeWipeTower     m_fake_wipe_tower;
    
    //SoftFever: calibration
    Calib_Params m_calib_params;

    // 允许 GCode 设置 Print 的 GCodeExport 步骤状态。
    friend class GCode;
    // 允许 PrintObject 访问 m_mutex 和 m_cancel_callback。
    friend class PrintObject;

public:
    //BBS: this was a print config and now seems to be useless so we move it to here
    // ORCA: parameter below is now back to being a user option (min_skirt_length)
    //static float min_skirt_length;
};


} /* slic3r_Print_hpp_ */

#endif
