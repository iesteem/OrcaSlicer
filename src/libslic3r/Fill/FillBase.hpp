#ifndef slic3r_FillBase_hpp_
#define slic3r_FillBase_hpp_

#include <assert.h>
#include <memory.h>
#include <float.h>
#include <stdint.h>
#include <stdexcept>

#include <type_traits>

#include "../libslic3r.h"
#include "../BoundingBox.hpp"
#include "../Exception.hpp"
#include "../Utils.hpp"
#include "../ExPolygon.hpp"
//BBS: 新功能所需的头文件
#include "../PrintConfig.hpp"
#include "../Flow.hpp"
#include "../ExtrusionEntity.hpp"
#include "../ExtrusionEntityCollection.hpp"
#include "../ShortestPath.hpp"

namespace Slic3r {

class Surface;
enum InfillPattern : int;

namespace FillAdaptive {
    struct Octree;
};

// 填充不应失败，因此错误归类为 RuntimeError，而非 SlicingError。
class InfillFailedException : public Slic3r::RuntimeError {
public:
    InfillFailedException() : Slic3r::RuntimeError("Infill failed") {}
};

struct LockRegionParam
{
    LockRegionParam() {}
    std::map<float, ExPolygons> skin_density_params;
    std::map<float, ExPolygons> skeleton_density_params;
    std::map<Flow, ExPolygons>  skin_flow_params;
    std::map<Flow, ExPolygons>  skeleton_flow_params;
};

struct FillParams
{
    bool        full_infill() const { return density > 0.9999f; }
    // 不要连接内部周长周围的填充线。
    bool        dont_connect() const { return anchor_length_max < 0.05f; }

    // 填充密度，范围 <0, 1>
    float       density 		{ 0.f };
    int   multiline{1};

    // 沿周长的填充锚点长度。
    // 1000mm 大致是适合 32 位 coord_t 的最大长度线。
    float       anchor_length       { 1000.f };
    float       anchor_length_max   { 1000.f };

    // G 代码分辨率。
    double      resolution          { 0.0125 };

    // 不要调整间距以均匀填充空间。
    bool        dont_adjust 	{ true };

    // 单调填充 - 严格从左到右，以获得更好的顶部填充表面质量。
    bool 		monotonic		{ false };

    // 用于蜂巢填充。
    // 要求完成每个循环；
    // 在这种情况下，我们不尝试制作更多连续路径
    bool        complete 		{ false };

    // 用于同心填充，在 Classic 和 Arachne 之间切换。
    bool        use_arachne{ false };
    // 使用 Arachne 的同心填充的层高。
    coordf_t    layer_height    { 0.f };

    // 用于横向晶格
    coordf_t    lateral_lattice_angle_1    { 0.f };
    coordf_t    lateral_lattice_angle_2    { 0.f };
    InfillPattern pattern{ ipRectilinear };

    // 用于横向蜂巢
    float       infill_overhang_angle    { 60 };

    // BBS
    Flow            flow;
    ExtrusionRole   extrusion_role{ ExtrusionRole(0) };
    bool            using_internal_flow{ false };
    //BBS: 仅用于新的顶面图案
    float           no_extrusion_overlap{ 0.0 };
    const           PrintRegionConfig* config{ nullptr };
    bool            dont_sort{ false }; // 不对线条排序，仅简单连接它们
    bool            can_reverse{true};

    float           horiz_move{0.0}; //move infill to get cross zag pattern
    bool            symmetric_infill_y_axis{false};
    coord_t         symmetric_y_axis{0};
    bool            locked_zag{false};
    float           infill_lock_depth{0.0};
    float           skin_infill_depth{0.0};
};
static_assert(IsTriviallyCopyable<FillParams>::value, "FillParams class is not POD (and it should be - see constructor).");

class Fill
{
public:
    // 层的索引。
    size_t      layer_id;
    // 顶部打印表面的 Z 坐标，未缩放坐标
    coordf_t    z;
    // 未缩放坐标
    coordf_t    spacing;
    // 填充/周长重叠，未缩放坐标
    coordf_t    overlap;
    // 弧度，逆时针，0 = 东
    float       angle;
    // Orca: 是否使用模板角度
    bool        is_using_template_angle{false};
    // 缩放坐标。连接两条填充线的周长线段的最大长度。
    // 由 FillRectilinear2、FillGrid2、FillTriangles、FillStars 和 FillCubic 使用。
    // 如果为零，则链接不受限制。
    coord_t     link_max_length;
    // 缩放坐标。由同心填充图案使用，裁剪循环以创建挤出路径。
    coord_t     loop_clipping;
    // 缩放坐标。物体 2D 投影的边界框。
    BoundingBox bounding_box;

    // 基于网格构建的八叉树，用于自适应立方体填充
    FillAdaptive::Octree* adapt_fill_octree = nullptr;

    // PrintConfig 和 PrintObjectConfig 由使用 Arachne 的填充使用（同心和 FillEnsuring）。
    // Orca: 也用于间隙填充功能。
    const PrintConfig       *print_config        = nullptr;
    const PrintObjectConfig *print_object_config = nullptr;

    // BBS: 同一层中所有无重叠的 expolygon
    ExPolygons  no_overlap_expolygons;

    static float infill_anchor;
    static float infill_anchor_max;

public:
    virtual ~Fill() {}
    virtual Fill* clone() const = 0;

    static Fill* new_from_type(const InfillPattern type);
    static Fill* new_from_type(const std::string &type);
    static bool  use_bridge_flow(const InfillPattern type);

    void         set_bounding_box(const Slic3r::BoundingBox &bbox) { bounding_box = bbox; }
    BoundingBox  extended_object_bounding_box() const;
    // 是否为填充使用桥接流量？
    virtual bool use_bridge_flow() const { return false; }

    // 是否不排序填充线以优化打印头路径？
    virtual bool no_sort() const { return false; }

    virtual bool is_self_crossing() = 0;

    // 如果填充在层之间具有一致的图案，则返回 true。
    virtual bool has_consistent_pattern() const { return false; }

    // 执行填充。
    virtual Polylines fill_surface(const Surface *surface, const FillParams &params);
    virtual ThickPolylines fill_surface_arachne(const Surface* surface, const FillParams& params);
    virtual void set_lock_region_param(const LockRegionParam &lock_param){};
    // BBS: 此方法用于填充 ExtrusionEntityCollection。
    // 默认调用 fill_surface
    virtual void fill_surface_extrusion(const Surface *surface, const FillParams &params, ExtrusionEntitiesPtr &out);

protected:
    Fill() :
        layer_id(size_t(-1)),
        z(0.),
        spacing(0.),
        // 填充/周长重叠。
        overlap(0.),
        // 初始角度未定义。
        angle(FLT_MAX),
        link_max_length(0),
        loop_clipping(0),
        // 初始边界框为空，因此未定义。
        bounding_box(Point(0, 0), Point(-1, -1))
        {}

    // expolygon 可能被该方法修改以避免复制。
    virtual void    _fill_surface_single(
        const FillParams                & /* params */,
        unsigned int                      /* thickness_layers */,
        const std::pair<float, Point>   & /* direction */,
        ExPolygon                         /* expolygon */,
        Polylines                       & /* polylines_out */) {}

    // 用于同心填充，使用 Arachne 生成 ThickPolyline。
    virtual void _fill_surface_single(const FillParams& params,
        unsigned int                   thickness_layers,
        const std::pair<float, Point>& direction,
        ExPolygon                      expolygon,
        ThickPolylines& thick_polylines_out) {}

    virtual float _layer_angle(size_t idx) const { return is_using_template_angle ? 0.f : (idx & 1) ? float(M_PI/2.) : 0.f; }

    virtual std::pair<float, Point> _infill_direction(const Surface *surface) const;
    
    // Orca: 专用函数，根据打印对象参数计算提供的表面的间隙填充线，并将它们追加到输出 ExtrusionEntityCollection。
    void _create_gap_fill(const Surface* surface, const FillParams& params, ExtrusionEntityCollection* out);

public:
    static void connect_infill(Polylines &&infill_ordered, const ExPolygon &boundary, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_infill(Polylines &&infill_ordered, const Polygons &boundary, const BoundingBox& bbox, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_infill(Polylines &&infill_ordered, const std::vector<const Polygon*> &boundary, const BoundingBox &bbox, Polylines &polylines_out, double spacing, const FillParams &params);

    static void chain_or_connect_infill(Polylines &&infill_ordered, const ExPolygon &boundary, Polylines &polylines_out, const double spacing, const FillParams &params);

    static void connect_base_support(Polylines &&infill_ordered, const std::vector<const Polygon*> &boundary_src, const BoundingBox &bbox, Polylines &polylines_out, const double spacing, const FillParams &params);
    static void connect_base_support(Polylines &&infill_ordered, const Polygons &boundary_src, const BoundingBox &bbox, Polylines &polylines_out, const double spacing, const FillParams &params);

    static coord_t  _adjust_solid_spacing(const coord_t width, const coord_t distance);
};
   //Fill 多线
   void multiline_fill(Polylines& polylines, const FillParams& params, float spacing);
} // namespace Slic3r

#endif // slic3r_FillBase_hpp_
