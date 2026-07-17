#ifndef slic3r_Layer_hpp_
#define slic3r_Layer_hpp_

#include "libslic3r.h"
#include "BoundingBox.hpp"
#include "Flow.hpp"
#include "SurfaceCollection.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "BoundingBox.hpp"
namespace Slic3r {

class ExPolygon;
using ExPolygons = std::vector<ExPolygon>;
class Layer;
using LayerPtrs = std::vector<Layer*>;
class LayerRegion;
using LayerRegionPtrs = std::vector<LayerRegion*>;
class PrintRegion;
class PrintObject;

namespace FillAdaptive {
    struct Octree;
};

namespace FillLightning {
    class Generator;
};

class LayerRegion
{
public:
    Layer*                      layer()         { return m_layer; }
    const Layer*                layer() const   { return m_layer; }
    const PrintRegion&          region() const  { return *m_region; }

    const SurfaceCollection& get_slices() const { return slices; }

    // 通过切片原始几何体生成的表面集合
    // 按类型分为顶部/底部/内部
    SurfaceCollection           slices;
    // 在切片被分割为顶部/底部/内部之前备份的切片。
    // 仅为多区域图层或具有象脚补偿的图层备份。
    //FIXME 审查是否通过始终保留 raw_slices 来简化代码。
    ExPolygons                  raw_slices;

    // 填充间隙的挤出路径/环的集合
    // 这些填充由周长生成器生成。
    // 它们本身不会被打印，而是在填充生成期间被复制到 this->fills。
    ExtrusionEntityCollection   thin_fills;

    // 未指定的填充多边形，用于悬垂检测（"确保垂直壁厚功能"）
    // 以及重新开始填充。
    ExPolygons                  fill_expolygons;
    // 用于填充生成的表面集合
    SurfaceCollection           fill_surfaces;
    // BBS: 未指定的填充多边形，用于当我们不想要填充/周长重叠时的交互
    ExPolygons                  fill_no_overlap_expolygons;

    // 表示桥接区域的 expolygons 集合（因此不需要
    // 支撑材料）
//    Polygons                    bridged;

    // 表示无支撑桥接边缘的折线集合
    Polylines          			unsupported_bridge_edges;

    // 构建所有周长的有序挤出路径/环集合
    // （此集合仅包含 ExtrusionEntityCollection 对象）
    ExtrusionEntityCollection   perimeters;

    // 填充表面的有序挤出路径集合
    // （此集合仅包含 ExtrusionEntityCollection 对象）
    ExtrusionEntityCollection   fills;

    unsigned int extruder(FlowRole role) const;
    Flow    flow(FlowRole role) const;
    Flow    flow(FlowRole role, double layer_height) const;
    Flow    flow(FlowRole role, double layer_height, bool use_initial_layer_width) const;
    Flow    bridging_flow(FlowRole role, bool thick_bridge = false) const;

    void    slices_to_fill_surfaces_clipped();
    void    prepare_fill_surfaces();
    //BBS
    void    make_perimeters(const SurfaceCollection &slices, const LayerRegionPtrs &compatible_regions, SurfaceCollection* fill_surfaces, ExPolygons* fill_no_overlap);
    void    process_external_surfaces(const Layer *lower_layer, const Polygons *lower_layer_covered);
    double  infill_area_threshold() const;
    // 通过裁剪多边形来裁剪表面。由第一层的象脚补偿使用。
    void    trim_surfaces(const Polygons &trimming_polygons);
    // 单步象脚补偿，由第一层的象脚补偿使用。
    // 通过裁剪多边形（按象脚补偿步骤收缩）来裁剪表面，但不要过度收缩狭窄部分以免没有周长适合。
    void    elephant_foot_compensation_step(const float elephant_foot_compensation_perimeter_step, const Polygons &trimming_polygons);

    void    export_region_slices_to_svg(const char *path) const;
    void    export_region_fill_surfaces_to_svg(const char *path) const;
    // 导出到 "out/LayerRegion-name-%d.svg"，每次导出时索引递增。
    void    export_region_slices_to_svg_debug(const char *name) const;
    void    export_region_fill_surfaces_to_svg_debug(const char *name) const;

    // 是否有任何有效的挤出分配到此 LayerRegion？
    bool    has_extrusions() const { return ! this->perimeters.entities.empty() || ! this->fills.entities.empty(); }
    //BBS
    void    simplify_infill_extrusion_entity() { simplify_entity_collection(&fills); }
    void    simplify_wall_extrusion_entity() { simplify_entity_collection(&perimeters); }
private:
    void    simplify_entity_collection(ExtrusionEntityCollection* entity_collection);
    void    simplify_path(ExtrusionPath* path);
    void    simplify_multi_path(ExtrusionMultiPath* multipath);
    void    simplify_loop(ExtrusionLoop* loop);

protected:
    friend class Layer;
    friend class PrintObject;

    LayerRegion(Layer *layer, const PrintRegion *region) : m_layer(layer), m_region(region) {}
    ~LayerRegion() {}

private:
    Layer             *m_layer;
    const PrintRegion *m_region;
};

class Layer
{
public:
    // 此图层在 PrintObject::m_layers 中的顺序索引，偏移了筏层数量。
    size_t              id() const          { return m_id; }
    void                set_id(size_t id)   { m_id = id; }
    PrintObject*        object()            { return m_object; }
    const PrintObject*  object() const      { return m_object; }

    Layer              *upper_layer;
    Layer              *lower_layer;
    bool                slicing_errors;
    coordf_t            slice_z;       // 用于在未缩放坐标中切片的 Z
    coordf_t            print_z;       // 用于在未缩放坐标中打印的 Z
    coordf_t            height;        // 未缩放坐标中的图层高度
    coordf_t            bottom_z() const { return this->print_z - this->height; }

    //Extrusions 估计严重变形，在"估计卷曲挤出"步骤中评估。在快速移动时应避免这些线段。
    CurledLines         curled_lines;

    // BBS
    mutable ExPolygons          sharp_tails;
    mutable ExPolygons          cantilevers;
    mutable std::vector<float>  sharp_tails_height;

    // 通过对源几何体的多个网格（可能具有不同的挤出机ID和切片参数）进行切片生成的 expolygons 集合，并进行合并。
    // 对于第一层，如果应用了象脚补偿，此 lslice 未被补偿，因此
    // 它包含了象脚效应，从而对应于打印的第一层的形状。
    // 这些 lslices（也称为岛屿）通过最短遍历距离链接，此遍历
    // 顺序将由 G-code 生成器应用于适合这些 lslices 的挤出。
    // 这些 lslices 还用于检测连续层之间的悬垂和重叠，因此保持第一层 lslice 不被象脚补偿算法补偿非常重要。
    ExPolygons 				 lslices;
    ExPolygons 				 lslices_extrudable;  // BBS: lslices 的可挤出部分，用于树形支撑
    std::vector<BoundingBox> lslices_bboxes;

    // BBS
    ExPolygons              loverhangs;
    BoundingBox             loverhangs_bbox;
    size_t                  region_count() const { return m_regions.size(); }
    const LayerRegion*      get_region(int idx) const { return m_regions[idx]; }
    LayerRegion*            get_region(int idx) { return m_regions[idx]; }
    LayerRegion*            add_region(const PrintRegion *print_region);
    const LayerRegionPtrs&  regions() const { return m_regions; }
    // 测试是否有任何切片分配到此图层。
    bool                    empty() const;
    void                    make_slices();
    // 如果需要，备份和恢复原始切片区域。
    //FIXME 审查是否通过始终保留 raw_slices 来简化代码。
    void                    backup_untyped_slices();
    void                    restore_untyped_slices();
    // 提高在重新切片时 detect_surfaces_type() 的鲁棒性（使用类型化切片），请参见 GH issue #7442。
    void                    restore_untyped_slices_no_extra_perimeters();
    // 合并为岛的切片，将由象脚补偿使用，用收缩后的合并切片裁剪各个表面。
    ExPolygons              merged(float offset) const;
    template <class T> bool any_internal_region_slice_contains(const T &item) const {
        for (const LayerRegion *layerm : m_regions) if (layerm->slices.any_internal_contains(item)) return true;
        return false;
    }
    template <class T> bool any_bottom_region_slice_contains(const T &item) const {
        for (const LayerRegion *layerm : m_regions) if (layerm->slices.any_bottom_contains(item)) return true;
        return false;
    }

    // 两个区域是否可以打印在连续的周长中
    static bool             is_perimeter_compatible(const PrintRegion& a, const PrintRegion& b);
    void                    make_perimeters();
    // make_fills() 的无参数伪版本，仅用于 Perl 集成。
    void                    make_fills() { this->make_fills(nullptr, nullptr); }
    void                    make_fills(FillAdaptive::Octree* adaptive_fill_octree, FillAdaptive::Octree* support_fill_octree, FillLightning::Generator* lightning_generator = nullptr);
    Polylines               generate_sparse_infill_polylines_for_anchoring(FillAdaptive::Octree *adaptive_fill_octree,
                                                                           FillAdaptive::Octree *support_fill_octree,
                                                                           FillLightning::Generator* lightning_generator) const;
    void 					make_ironing();

    void                    export_region_slices_to_svg(const char *path) const;
    void                    export_region_fill_surfaces_to_svg(const char *path) const;
    // 导出到 "out/LayerRegion-name-%d.svg"，每次导出时索引递增。
    void                    export_region_slices_to_svg_debug(const char *name) const;
    void                    export_region_fill_surfaces_to_svg_debug(const char *name) const;

    // 是否有任何有效的挤出分配到此 LayerRegion？
    virtual bool            has_extrusions() const { for (auto layerm : m_regions) if (layerm->has_extrusions()) return true; return false; }

    //BBS
    void simplify_wall_extrusion_path() { for (auto layerm : m_regions) layerm->simplify_wall_extrusion_entity();}
    void simplify_infill_extrusion_path() { for (auto layerm : m_regions) layerm->simplify_infill_extrusion_entity(); }
    //BBS: 此函数计算此图层稀疏填充的最大空隙网格面积。仅为估计值
    coordf_t get_sparse_infill_max_void_area();

    // FN_HIGHER_EQUAL: 提供的对象指针具有 Z 值 >= 内部阈值。
    // 查找第一个 Z 值 >= fn_higher_equal 内部阈值的元素。
    // 如果没有找到 Z 值 >= fn_higher_equal 内部阈值的向量元素，则返回 vec.size()
    // 如果初始 idx 为 size_t(-1)，则使用二分搜索。
    // 否则向上线性搜索。
    template<typename IteratorType, typename IndexType, typename FN_HIGHER_EQUAL>
    static IndexType idx_higher_or_equal(IteratorType begin, IteratorType end, IndexType idx, FN_HIGHER_EQUAL fn_higher_equal)
    {
        auto size = int(end - begin);
        if (size == 0) {
            idx = 0;
            }
        else if (idx == IndexType(-1)) {
            // 每个线程池调用中第一批图层的第一个。使用二分搜索。
            int idx_low = 0;
            int idx_high = std::max(0, size - 1);
            while (idx_low + 1 < idx_high) {
                int idx_mid = (idx_low + idx_high) / 2;
                if (fn_higher_equal(begin[idx_mid]))
                    idx_high = idx_mid;
                else
                    idx_low = idx_mid;
                }
            idx = fn_higher_equal(begin[idx_low]) ? idx_low :
                (fn_higher_equal(begin[idx_high]) ? idx_high : size);
            }
        else {
            // 对于此批次图层中的其他图层，增量搜索，这比二分搜索更便宜。
            while (int(idx) < size && !fn_higher_equal(begin[idx]))
                ++idx;
            }
        return idx;
    }

protected:
    friend class PrintObject;
    friend std::vector<Layer*> new_layers(PrintObject*, const std::vector<coordf_t>&);
    friend std::string fix_slicing_errors(PrintObject* object, LayerPtrs&, const std::function<void()>&, int &);

    Layer(size_t id, PrintObject *object, coordf_t height, coordf_t print_z, coordf_t slice_z) :
        upper_layer(nullptr), lower_layer(nullptr), slicing_errors(false),
        slice_z(slice_z), print_z(print_z), height(height),
        m_id(id), m_object(object) {}
    virtual ~Layer();

//BBS: 简化支撑路径的方法
    void    simplify_support_entity_collection(ExtrusionEntityCollection* entity_collection);
    void    simplify_support_path(ExtrusionPath* path);
    void    simplify_support_multi_path(ExtrusionMultiPath* multipath);
    void    simplify_support_loop(ExtrusionLoop* loop);

private:
    // 图层的顺序索引，从0开始，偏移了筏层数量。
    size_t              m_id;
    PrintObject        *m_object;
    LayerRegionPtrs     m_regions;
};

enum SupportInnerType {
    stInnerNormal,
    stInnerTree
};

class SupportLayer : public Layer
{
public:
    // 支撑覆盖的多边形：底部、界面和接触区域。
    // 用于在支撑挤出移动到这些 support_islands 上时抑制回抽。
    ExPolygons                  support_islands;
    // 支撑底部以及支撑界面和接触的挤出路径。
    ExtrusionEntityCollection   support_fills;
    SupportInnerType            support_type = stInnerNormal;

    // 用于树形支撑
    ExPolygons base_areas;


    // 是否有任何有效的挤出分配到此 LayerRegion？
    virtual bool                has_extrusions() const { return ! support_fills.empty(); }

    // 界面层的从0开始的索引，用于交替界面/接触层的方向。
    size_t                      interface_id() const { return m_interface_id; }

    void simplify_support_extrusion_path() { this->simplify_support_entity_collection(&support_fills); }

protected:
    friend class PrintObject;
    friend class TreeSupport;

    // 构造函数已被设为公开，以便能够在筏层和对象第一层之间插入裙边或擦拭塔的附加支撑层。
    SupportLayer(size_t id, size_t interface_id, PrintObject *object, coordf_t height, coordf_t print_z, coordf_t slice_z) :
        Layer(id, object, height, print_z, slice_z), m_interface_id(interface_id), support_type(stInnerNormal) {}
    virtual ~SupportLayer() = default;

    size_t m_interface_id;

    // 用于树形支撑
    ExPolygons                                roof_areas;
    ExPolygons                                roof_1st_layer; // 屋顶正下方的图层。使用 PolySupport 时，此图层应使用常规材料打印
    ExPolygons                                floor_areas;
    ExPolygons                                roof_gap_areas; // 支撑屋顶和悬垂之间间隙中的区域
    enum AreaType { BaseType = 0, RoofType = 1, FloorType = 2, Roof1stLayer = 3 };
    struct AreaGroup
    {
        ExPolygon *area;
        int        type;
        int interface_id = 0;
        coordf_t   dist_to_top; // 距顶部的毫米距离
        bool need_infill = false;
        bool need_extra_wall = false;
        AreaGroup(ExPolygon *a, int t, coordf_t d) : area(a), type(t), dist_to_top(d) {}
    };
    std::vector<AreaGroup>                    area_groups;
};

template<typename LayerContainer>
inline std::vector<float> zs_from_layers(const LayerContainer &layers)
{
    std::vector<float> zs;
    zs.reserve(layers.size());
    for (const Layer *l : layers)
        zs.emplace_back((float)l->slice_z);
    return zs;
}

extern BoundingBox get_extents(const LayerRegion &layer_region);
extern BoundingBox get_extents(const LayerRegionPtrs &layer_regions);

}

#endif
