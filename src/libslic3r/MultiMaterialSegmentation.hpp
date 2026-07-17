#ifndef slic3r_MultiMaterialSegmentation_hpp_
#define slic3r_MultiMaterialSegmentation_hpp_

#include <utility>
#include <vector>

namespace Slic3r {

class ExPolygon;
class ModelVolume;
class PrintObject;
class FacetsAnnotation;

using ExPolygons = std::vector<ExPolygon>;

struct ColoredLine
{
    Line line;
    int  color;
    int  poly_idx       = -1;
    int  local_line_idx = -1;
};

using ColoredLines = std::vector<ColoredLine>;

enum class IncludeTopAndBottomLayers {
    Yes,
    No
};

struct ModelVolumeFacetsInfo {
    const FacetsAnnotation &facets_annotation;
    // 指示模型体积是否被绘制。
    const bool              is_painted;
    // 指示默认挤出机（TriangleStateType::NONE）是否应被体积挤出机替换。
    const bool              replace_default_extruder;
};

// 返回基于在分割小工具中绘制的分割结果。
std::vector<std::vector<ExPolygons>> segmentation_by_painting(const PrintObject                                               &print_object,
                                                              const std::function<ModelVolumeFacetsInfo(const ModelVolume &)> &extract_facets_info,
                                                              size_t                                                           num_facets_states,
                                                              float                                                            segmentation_max_width,
                                                              float                                                            segmentation_interlocking_depth,
                                                              bool                                                             segmentation_interlocking_beam,
                                                              IncludeTopAndBottomLayers                                        include_top_and_bottom_layers,
                                                              const std::function<void()>                                     &throw_on_cancel_callback);

// 返回基于在多材料分割小工具中绘制的多材料分割结果
std::vector<std::vector<ExPolygons>> multi_material_segmentation_by_painting(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

// 返回基于在蒙皮分割小工具中绘制的蒙皮分割结果
std::vector<std::vector<ExPolygons>> fuzzy_skin_segmentation_by_painting(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

} // namespace Slic3r

namespace boost::polygon {
template<> struct geometry_concept<Slic3r::ColoredLine>
{
    typedef segment_concept type;
};

template<> struct segment_traits<Slic3r::ColoredLine>
{
    typedef coord_t       coordinate_type;
    typedef Slic3r::Point point_type;

    static inline point_type get(const Slic3r::ColoredLine &line, const direction_1d &dir)
    {
        return dir.to_int() ? line.line.b : line.line.a;
    }
};
} // namespace boost::polygon

#endif // slic3r_MultiMaterialSegmentation_hpp_
