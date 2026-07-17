#include "ClipperUtils.hpp"
#include "Geometry.hpp"
#include "ShortestPath.hpp"

// #define CLIPPER_UTILS_DEBUG

#ifdef CLIPPER_UTILS_DEBUG
#include "SVG.hpp"
#endif /* CLIPPER_UTILS_DEBUG */

// 使用Shiny侵入式分析器的性能分析支持
//#define CLIPPER_UTILS_PROFILE
#if defined(SLIC3R_PROFILE) && defined(CLIPPER_UTILS_PROFILE)
	#include <Shiny/Shiny.h>
	#define CLIPPERUTILS_PROFILE_FUNC() PROFILE_FUNC()
	#define CLIPPERUTILS_PROFILE_BLOCK(name) PROFILE_BLOCK(name)
#else
	#define CLIPPERUTILS_PROFILE_FUNC()
	#define CLIPPERUTILS_PROFILE_BLOCK(name)
#endif

namespace Slic3r {

#ifdef CLIPPER_UTILS_DEBUG
// 用于调试 Clipper 库，向 Clipper 作者提供错误报告。
bool export_clipper_input_polygons_bin(const char *path, const ClipperLib::Paths &input_subject, const ClipperLib::Paths &input_clip)
{
    FILE *pfile = fopen(path, "wb");
    if (pfile == NULL)
        return false;

    uint32_t sz = uint32_t(input_subject.size());
    fwrite(&sz, 1, sizeof(sz), pfile);
    for (size_t i = 0; i < input_subject.size(); ++i) {
        const ClipperLib::Path &path = input_subject[i];
        sz = uint32_t(path.size());
        ::fwrite(&sz, 1, sizeof(sz), pfile);
        ::fwrite(path.data(), sizeof(ClipperLib::IntPoint), sz, pfile);
    }
    sz = uint32_t(input_clip.size());
    ::fwrite(&sz, 1, sizeof(sz), pfile);
    for (size_t i = 0; i < input_clip.size(); ++i) {
        const ClipperLib::Path &path = input_clip[i];
        sz = uint32_t(path.size());
        ::fwrite(&sz, 1, sizeof(sz), pfile);
        ::fwrite(path.data(), sizeof(ClipperLib::IntPoint), sz, pfile);
    }
    ::fclose(pfile);
    return true;

err:
    ::fclose(pfile);
    return false;
}
#endif /* CLIPPER_UTILS_DEBUG */

namespace ClipperUtils {
Points EmptyPathsProvider::s_empty_points;
Points SinglePathProvider::s_end;

// 裁剪源多边形作为裁剪多边形，使用源（待裁剪）多边形周围的边界框。
// 用作昂贵的 ClipperLib 操作的优化，例如使用覆盖下方整个图层的一组多边形逐个裁剪源多边形。
template<typename PointsType> inline void clip_clipper_polygon_with_subject_bbox_templ(const PointsType &src, const BoundingBox &bbox, PointsType &out, const bool get_entire_polygons=false)
{
    using PointType = typename PointsType::value_type;

    out.clear();
    const size_t cnt = src.size();
    if (cnt < 3) return;

    enum class Side { Left = 1, Right = 2, Top = 4, Bottom = 8 };

    auto sides = [bbox](const PointType &p) {
        return int(p.x() < bbox.min.x()) * int(Side::Left) + int(p.x() > bbox.max.x()) * int(Side::Right) + int(p.y() < bbox.min.y()) * int(Side::Bottom) +
               int(p.y() > bbox.max.y()) * int(Side::Top);
    };

    int          sides_prev = sides(src.back());
    int          sides_this = sides(src.front());
    const size_t last       = cnt - 1;
    for (size_t i = 0; i < last; ++i) {
        int sides_next = sides(src[i + 1]);
        if ( // 此点在内部。接受它。
            sides_this == 0 ||
            // 此点在外侧且前一个或后一个在内侧，或
            // 线段可能切割边界框的角。
            (sides_prev & sides_this & sides_next) == 0) {
            out.emplace_back(src[i]);
            sides_prev = sides_this;
        } else {
            // 三个点（当前、前一个、后一个）都在同一侧的外部。
            // 忽略此点。
        }
        sides_this = sides_next;
    }

    // 绝不产生只有单个点的输出多边形。
    if (!out.empty()) {
        if (get_entire_polygons) {
            out=src;
        } else {
            if (int sides_next = sides(out.front());
            // 最后一个点在内侧。接受它。
            sides_this == 0 ||
            // 此点在外侧且前一个或后一个在内侧，或
            // 线段可能切割边界框的角。
            (sides_prev & sides_this & sides_next) == 0)
            out.emplace_back(src.back());
        }
    }
}

void clip_clipper_polygon_with_subject_bbox(const Points &src, const BoundingBox &bbox, Points &out, const bool get_entire_polygons) { clip_clipper_polygon_with_subject_bbox_templ(src, bbox, out, get_entire_polygons); }
void clip_clipper_polygon_with_subject_bbox(const ZPoints &src, const BoundingBox &bbox, ZPoints &out) { clip_clipper_polygon_with_subject_bbox_templ(src, bbox, out); }

template<typename PointsType> [[nodiscard]] PointsType clip_clipper_polygon_with_subject_bbox_templ(const PointsType &src, const BoundingBox &bbox)
{
    PointsType out;
    clip_clipper_polygon_with_subject_bbox(src, bbox, out);
    return out;
}

[[nodiscard]] Points  clip_clipper_polygon_with_subject_bbox(const Points &src, const BoundingBox &bbox) { return clip_clipper_polygon_with_subject_bbox_templ(src, bbox); }
[[nodiscard]] ZPoints clip_clipper_polygon_with_subject_bbox(const ZPoints &src, const BoundingBox &bbox) { return clip_clipper_polygon_with_subject_bbox_templ(src, bbox); }

void clip_clipper_polygon_with_subject_bbox(const Polygon &src, const BoundingBox &bbox, Polygon &out) { 
    clip_clipper_polygon_with_subject_bbox(src.points, bbox, out.points);
}

[[nodiscard]] Polygon clip_clipper_polygon_with_subject_bbox(const Polygon &src, const BoundingBox &bbox, const bool get_entire_polygons)
{
    Polygon out;
    clip_clipper_polygon_with_subject_bbox(src.points, bbox, out.points, get_entire_polygons);
    return out;
}

[[nodiscard]] Polygons clip_clipper_polygons_with_subject_bbox(const Polygons &src, const BoundingBox &bbox)
{
    Polygons out;
    out.reserve(src.size());
    for (const Polygon &p : src) out.emplace_back(clip_clipper_polygon_with_subject_bbox(p, bbox));
    out.erase(std::remove_if(out.begin(), out.end(), [](const Polygon &polygon) { return polygon.empty(); }), out.end());
    return out;
}
[[nodiscard]] Polygons clip_clipper_polygons_with_subject_bbox(const ExPolygon &src, const BoundingBox &bbox, const bool get_entire_polygons)
{
    Polygons out;
    out.reserve(src.num_contours());
    out.emplace_back(clip_clipper_polygon_with_subject_bbox(src.contour, bbox, get_entire_polygons));
    for (const Polygon &p : src.holes) out.emplace_back(clip_clipper_polygon_with_subject_bbox(p, bbox, get_entire_polygons));
    out.erase(std::remove_if(out.begin(), out.end(), [](const Polygon &polygon) { return polygon.empty(); }), out.end());
    return out;
}
[[nodiscard]] Polygons clip_clipper_polygons_with_subject_bbox(const ExPolygons &src, const BoundingBox &bbox, const bool get_entire_polygons)
{
    Polygons out;
    out.reserve(number_polygons(src));
    for (const ExPolygon &p : src) {
        Polygons temp = clip_clipper_polygons_with_subject_bbox(p, bbox, get_entire_polygons);
        out.insert(out.end(), temp.begin(), temp.end());
    }

    out.erase(std::remove_if(out.begin(), out.end(), [](const Polygon &polygon) {return polygon.empty(); }), out.end());
    return out;
}
}

static ExPolygons PolyTreeToExPolygons(ClipperLib::PolyTree &&polytree)
{
    struct Inner {
        static void PolyTreeToExPolygonsRecursive(ClipperLib::PolyNode &&polynode, ExPolygons *expolygons)
        {  
            size_t cnt = expolygons->size();
            expolygons->resize(cnt + 1);
            (*expolygons)[cnt].contour.points = std::move(polynode.Contour);
            (*expolygons)[cnt].holes.resize(polynode.ChildCount());
            for (int i = 0; i < polynode.ChildCount(); ++ i) {
                (*expolygons)[cnt].holes[i].points = std::move(polynode.Childs[i]->Contour);
                // 添加包含在（嵌套在）孔洞内的外多边形。
                for (int j = 0; j < polynode.Childs[i]->ChildCount(); ++ j)
                    PolyTreeToExPolygonsRecursive(std::move(*polynode.Childs[i]->Childs[j]), expolygons);
            }
        }

        static size_t PolyTreeCountExPolygons(const ClipperLib::PolyNode &polynode)
        {
            size_t cnt = 1;
            for (int i = 0; i < polynode.ChildCount(); ++ i) {
                for (int j = 0; j < polynode.Childs[i]->ChildCount(); ++ j)
                cnt += PolyTreeCountExPolygons(*polynode.Childs[i]->Childs[j]);
            }
            return cnt;
        }
    };

    ExPolygons retval;
    size_t cnt = 0;
    for (int i = 0; i < polytree.ChildCount(); ++ i)
        cnt += Inner::PolyTreeCountExPolygons(*polytree.Childs[i]);
    retval.reserve(cnt);
    for (int i = 0; i < polytree.ChildCount(); ++ i)
        Inner::PolyTreeToExPolygonsRecursive(std::move(*polytree.Childs[i]), &retval);
    return retval;
}

Polylines PolyTreeToPolylines(ClipperLib::PolyTree &&polytree)
{
    struct Inner {
        static void AddPolyNodeToPaths(ClipperLib::PolyNode &polynode, Polylines &out)
        {
            if (! polynode.Contour.empty())
                out.emplace_back(std::move(polynode.Contour));
            for (ClipperLib::PolyNode *child : polynode.Childs)
                AddPolyNodeToPaths(*child, out);
        }
    };

    Polylines out;
    out.reserve(polytree.Total());
    Inner::AddPolyNodeToPaths(polytree, out);
    return out;
}

#if 0
// Global test.
bool has_duplicate_points(const ClipperLib::PolyTree &polytree)
{
    struct Helper {
        static void collect_points_recursive(const ClipperLib::PolyNode &polynode, ClipperLib::Path &out) {
            // For each hole of the current expolygon:
            out.insert(out.end(), polynode.Contour.begin(), polynode.Contour.end());
            for (int i = 0; i < polynode.ChildCount(); ++ i)
                collect_points_recursive(*polynode.Childs[i], out);
        }
    };
    ClipperLib::Path pts;
    for (int i = 0; i < polytree.ChildCount(); ++ i)
        Helper::collect_points_recursive(*polytree.Childs[i], pts);
    return has_duplicate_points(std::move(pts));
}
#else
// Local test inside each of the contours.
bool has_duplicate_points(const ClipperLib::PolyTree &polytree)
{
    struct Helper {
        static bool has_duplicate_points_recursive(const ClipperLib::PolyNode &polynode) {
            if (has_duplicate_points(polynode.Contour))
                return true;
            for (int i = 0; i < polynode.ChildCount(); ++ i)
                if (has_duplicate_points_recursive(*polynode.Childs[i]))
                    return true;
            return false;
        }
    };
    ClipperLib::Path pts;
    for (int i = 0; i < polytree.ChildCount(); ++ i)
        if (Helper::has_duplicate_points_recursive(*polytree.Childs[i]))
            return true;
    return false;
}
#endif

// CCW轮廓向外偏移，CW轮廓（孔洞）向内偏移。
// 不计算输出路径的并集。
template<typename PathsProvider>
static ClipperLib::Paths raw_offset(PathsProvider &&paths, float offset, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::EndType endType = ClipperLib::etClosedPolygon)
{
    ClipperLib::ClipperOffset co;
    ClipperLib::Paths out;
    out.reserve(paths.size());
    ClipperLib::Paths out_this;
    if (joinType == jtRound)
        co.ArcTolerance = miterLimit;
    else
        co.MiterLimit = miterLimit;
    co.ShortestEdgeLength = std::abs(offset * ClipperOffsetShortestEdgeFactor);
    for (const ClipperLib::Path &path : paths) {
        co.Clear();
        // 执行重新定向轮廓，使最外轮廓具有正面积。因此即使输入路径是CW方向，
        // 输出轮廓也将是CCW方向。
        // 偏移在轮廓重新定向后应用，因此偏移值的符号被反转。
        co.AddPath(path, joinType, endType);
        bool ccw = endType == ClipperLib::etClosedPolygon ? ClipperLib::Orientation(path) : true;
        co.Execute(out_this, ccw ? offset : - offset);
        if (! ccw) {
            // 反转生成的轮廓。
            for (ClipperLib::Path &path : out_this)
                std::reverse(path.begin(), path.end());
        }
        append(out, std::move(out_this));
    }
    return out;
}

// Offset outside by 10um, one by one.
template<typename PathsProvider>
static ClipperLib::Paths safety_offset(PathsProvider &&paths)
{
    return raw_offset(std::forward<PathsProvider>(paths), ClipperSafetyOffset, DefaultJoinType, DefaultMiterLimit);
}

template<class TResult, class TSubj, class TClip>
TResult clipper_do(
    const ClipperLib::ClipType     clipType,
    TSubj &&                       subject,
    TClip &&                       clip,
    const ClipperLib::PolyFillType fillType)
{
    ClipperLib::Clipper clipper;
    clipper.AddPaths(std::forward<TSubj>(subject), ClipperLib::ptSubject, true);
    clipper.AddPaths(std::forward<TClip>(clip),    ClipperLib::ptClip,    true);
    TResult retval;
    clipper.Execute(clipType, retval, fillType, fillType);
    return retval;
}

template<class TResult, class TSubj, class TClip>
TResult clipper_do(
    const ClipperLib::ClipType     clipType,
    TSubj &&                       subject,
    TClip &&                       clip,
    const ClipperLib::PolyFillType fillType,
    const ApplySafetyOffset        do_safety_offset)
{
    // 安全偏移仅在交集和差集上允许。
    assert(do_safety_offset == ApplySafetyOffset::No || clipType != ClipperLib::ctUnion);
    return do_safety_offset == ApplySafetyOffset::Yes ? 
        clipper_do<TResult>(clipType, std::forward<TSubj>(subject), safety_offset(std::forward<TClip>(clip)), fillType) :
        clipper_do<TResult>(clipType, std::forward<TSubj>(subject), std::forward<TClip>(clip), fillType);
}

template<class TResult, class TSubj>
TResult clipper_union(
    TSubj &&                       subject,
    // fillType pftNonZero 和 pftPositive 对于"归一化隐式并集"多边形集"应该"产生相同的结果
    const ClipperLib::PolyFillType fillType = ClipperLib::pftNonZero)
{
    ClipperLib::Clipper clipper;
    clipper.AddPaths(std::forward<TSubj>(subject), ClipperLib::ptSubject, true);
    TResult retval;
    clipper.Execute(ClipperLib::ctUnion, retval, fillType, fillType);
    return retval;
}

// 使用正规则执行输入多边形的并集，转换为 ExPolygons。
//FIXME 不进行布尔运算/使用 pftEvenOdd 是否有任何好处？
inline ExPolygons ClipperPaths_to_Slic3rExPolygons(const ClipperLib::Paths &input, bool do_union)
{
    return PolyTreeToExPolygons(clipper_union<ClipperLib::PolyTree>(input, do_union ? ClipperLib::pftNonZero : ClipperLib::pftEvenOdd));
}

template<typename PathsProvider>
static ClipperLib::Paths raw_offset_polyline(PathsProvider &&paths, float offset, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::EndType end_type = ClipperLib::etOpenButt)
{
    assert(offset > 0);
    return raw_offset<PathsProvider>(std::forward<PathsProvider>(paths), offset, joinType, miterLimit, end_type);
}

template<class TResult, typename PathsProvider>
static TResult expand_paths(PathsProvider &&paths, float offset, ClipperLib::JoinType joinType, double miterLimit)
{
    // BBS
    //assert(offset > 0);
    return clipper_union<TResult>(raw_offset(std::forward<PathsProvider>(paths), offset, joinType, miterLimit));
}

// used by shrink_paths()
template<class Container> static void remove_outermost_polygon(Container & solution);
template<> void remove_outermost_polygon<ClipperLib::Paths>(ClipperLib::Paths &solution)
    { if (! solution.empty()) solution.erase(solution.begin()); }
template<> void remove_outermost_polygon<ClipperLib::PolyTree>(ClipperLib::PolyTree &solution)
    { solution.RemoveOutermostPolygon(); }

template<class TResult, typename PathsProvider>
static TResult shrink_paths(PathsProvider &&paths, float offset, ClipperLib::JoinType joinType, double miterLimit)
{
    // BBS
    //assert(offset > 0);
    TResult out;
    if (auto raw = raw_offset(std::forward<PathsProvider>(paths), - offset, joinType, miterLimit); ! raw.empty()) {
        ClipperLib::Clipper clipper;
        clipper.AddPaths(raw, ClipperLib::ptSubject, true);
        ClipperLib::IntRect r = clipper.GetBounds();
        clipper.AddPath({ { r.left - 10, r.bottom + 10 }, { r.right + 10, r.bottom + 10 }, { r.right + 10, r.top - 10 }, { r.left - 10, r.top - 10 } }, ClipperLib::ptSubject, true);
        clipper.ReverseSolution(true);
        clipper.Execute(ClipperLib::ctUnion, out, ClipperLib::pftNegative, ClipperLib::pftNegative);
        remove_outermost_polygon(out);
    }
    return out;
}

template<class TResult, typename PathsProvider>
static TResult offset_paths(PathsProvider &&paths, float offset, ClipperLib::JoinType joinType, double miterLimit)
{
    // BBS
    //assert(offset != 0);

    return offset > 0 ?
        expand_paths<TResult>(std::forward<PathsProvider>(paths),   offset, joinType, miterLimit) :
        shrink_paths<TResult>(std::forward<PathsProvider>(paths), - offset, joinType, miterLimit);
}

Slic3r::Polygons offset(const Slic3r::Polygon &polygon, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(raw_offset(ClipperUtils::SinglePathProvider(polygon.points), delta, joinType, miterLimit)); }

Slic3r::Polygons offset(const Slic3r::Polygons &polygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(offset_paths<ClipperLib::Paths>(ClipperUtils::PolygonsProvider(polygons), delta, joinType, miterLimit)); }
Slic3r::ExPolygons offset_ex(const Slic3r::Polygons &polygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return PolyTreeToExPolygons(offset_paths<ClipperLib::PolyTree>(ClipperUtils::PolygonsProvider(polygons), delta, joinType, miterLimit)); }

Slic3r::Polygons offset(const Slic3r::Polyline &polyline, const float delta, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::EndType end_type)
    { assert(delta > 0); return to_polygons(clipper_union<ClipperLib::Paths>(raw_offset_polyline(ClipperUtils::SinglePathProvider(polyline.points), delta, joinType, miterLimit, end_type))); }
Slic3r::Polygons offset(const Slic3r::Polylines &polylines, const float delta, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::EndType end_type)
    { assert(delta > 0); return to_polygons(clipper_union<ClipperLib::Paths>(raw_offset_polyline(ClipperUtils::PolylinesProvider(polylines), delta, joinType, miterLimit, end_type))); }

Polygons contour_to_polygons(const Polygon &polygon, const float line_width, ClipperLib::JoinType join_type, double miter_limit){
    assert(line_width > 1.f); return to_polygons(clipper_union<ClipperLib::Paths>(
        raw_offset(ClipperUtils::SinglePathProvider(polygon.points), line_width/2, join_type, miter_limit, ClipperLib::etClosedLine)));}
Polygons contour_to_polygons(const Polygons &polygons, const float line_width, ClipperLib::JoinType join_type, double miter_limit){
    assert(line_width > 1.f); return to_polygons(clipper_union<ClipperLib::Paths>(
        raw_offset(ClipperUtils::PolygonsProvider(polygons), line_width/2, join_type, miter_limit, ClipperLib::etClosedLine)));}

// returns number of expolygons collected (0 or 1).
static int offset_expolygon_inner(const Slic3r::ExPolygon &expoly, const float delta, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::Paths &out)
{
    // 1) 偏移外轮廓。
    ClipperLib::Paths contours;
    {
        ClipperLib::ClipperOffset co;
        if (joinType == jtRound)
            co.ArcTolerance = miterLimit;
        else
            co.MiterLimit = miterLimit;
        co.ShortestEdgeLength = std::abs(delta * ClipperOffsetShortestEdgeFactor);
        co.AddPath(expoly.contour.points, joinType, ClipperLib::etClosedPolygon);
        co.Execute(contours, delta);
    }
    if (contours.empty())
        // 无需尝试偏移孔洞。
        return 0;

    if (expoly.holes.empty()) {
        // 无需从偏移后的 expolygon 中减去孔洞，我们已完成。
        append(out, std::move(contours));
    } else {
        // 2) 逐个偏移孔洞，收集偏移后的孔洞。
        ClipperLib::Paths holes;
        {
            for (const Polygon &hole : expoly.holes) {
                ClipperLib::ClipperOffset co;
                if (joinType == jtRound)
                    co.ArcTolerance = miterLimit;
                else
                    co.MiterLimit = miterLimit;
                co.ShortestEdgeLength = std::abs(delta * ClipperOffsetShortestEdgeFactor);
                co.AddPath(hole.points, joinType, ClipperLib::etClosedPolygon);
                ClipperLib::Paths out2;
                // Execute reorients the contours so that the outer most contour has a positive area. Thus the output
                // contours will be CCW oriented even though the input paths are CW oriented.
                // Offset is applied after contour reorientation, thus the signum of the offset value is reversed.
                co.Execute(out2, - delta);
                append(holes, std::move(out2));
            }
        }

        // 3) 从轮廓中减去孔洞。
        if (holes.empty()) {
            // 偏移后没有剩余孔洞。只需复制外轮廓。
            append(out, std::move(contours));
        } else if (delta < 0) {
            // 负偏移。偏移后的孔洞可能与外轮廓相交。
            // 从偏移后的轮廓中减去偏移后的孔洞。
            if (auto output = clipper_do<ClipperLib::Paths>(ClipperLib::ctDifference, contours, holes, ClipperLib::pftNonZero); ! output.empty()) {
                append(out, std::move(output));
            } else {
                // 偏移后的孔洞已吞噬偏移后的外轮廓。
                return 0;
            }
        } else {
            // 正偏移。只要 Clipper 偏移按预期工作，偏移后的孔洞将比原始孔洞面积更小或甚至消失，
            // 因此不会产生新的交集。只需收集反转的孔洞。
            out.reserve(contours.size() + holes.size());
            append(out, std::move(contours));
            // 原地反转孔洞。
            for (size_t i = 0; i < holes.size(); ++ i)
                std::reverse(holes[i].begin(), holes[i].end());
            append(out, std::move(holes));
        }
    }

    return 1;
}

static int offset_expolygon_inner(const Slic3r::Surface &surface, const float delta, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::Paths &out)
    { return offset_expolygon_inner(surface.expolygon, delta, joinType, miterLimit, out); }
static int offset_expolygon_inner(const Slic3r::Surface *surface, const float delta, ClipperLib::JoinType joinType, double miterLimit, ClipperLib::Paths &out)
    { return offset_expolygon_inner(surface->expolygon, delta, joinType, miterLimit, out); }

ClipperLib::Paths expolygon_offset(const Slic3r::ExPolygon &expolygon, const float delta, ClipperLib::JoinType joinType, double miterLimit)
{
    ClipperLib::Paths out;
    offset_expolygon_inner(expolygon, delta, joinType, miterLimit, out);
    return out;
}

// 这是多边形偏移的安全变体，专为多个 ExPolygons 定制。
// 要求输入的 expolygons 不重叠，且每个 ExPolygon 的孔洞不与各自的外轮廓相交。
// 每个 ExPolygon 分别偏移。对于外偏移，偏移后的 ExPolygons 应在此函数外进行并集。
template<typename ExPolygonVector>
static std::pair<ClipperLib::Paths, size_t> expolygons_offset_raw(const ExPolygonVector &expolygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
{
    // 偏移后的 ExPolygons，在它们被合并之前。
    ClipperLib::Paths output;
    output.reserve(expolygons.size());
    // 实际收集到输出中的非空偏移 expolygons 有多少个？
    // 如果只有一个，则无需进行最终并集。
    size_t expolygons_collected = 0;
    for (const auto &expoly : expolygons)
        expolygons_collected += offset_expolygon_inner(expoly, delta, joinType, miterLimit, output);
    return std::make_pair(std::move(output), expolygons_collected);
}

// 见 expolygon_offsets_raw 的注释。另外，对于正偏移，轮廓会进行并集。
template<typename ExPolygonVector>
static ClipperLib::Paths expolygons_offset(const ExPolygonVector &expolygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
{
    auto [output, expolygons_collected] = expolygons_offset_raw(expolygons, delta, joinType, miterLimit);
    // 合并偏移后的 expolygons。
    return expolygons_collected > 1 && delta > 0 ?
        // 向外偏移的 expolygons 可能相交。执行并集。
        clipper_union<ClipperLib::Paths>(output) :
        // 负偏移。收缩的 expolygons 不应相互相交。只需复制输出。
        output;
}

// 见 expolygons_offset_raw 的注释。此外，多边形总是合并以转换为 polytree。
template<typename ExPolygonVector>
static ClipperLib::PolyTree expolygons_offset_pt(const ExPolygonVector &expolygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
{
    auto [output, expolygons_collected] = expolygons_offset_raw(expolygons, delta, joinType, miterLimit);
    // 合并偏移后的 expolygons
    return clipper_union<ClipperLib::PolyTree>(output);
}

Slic3r::Polygons offset(const Slic3r::ExPolygon &expolygon, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(expolygon_offset(expolygon, delta, joinType, miterLimit)); }
Slic3r::Polygons offset(const Slic3r::ExPolygons &expolygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(expolygons_offset(expolygons, delta, joinType, miterLimit)); }
Slic3r::Polygons offset(const Slic3r::Surfaces &surfaces, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(expolygons_offset(surfaces, delta, joinType, miterLimit)); }
Slic3r::Polygons offset(const Slic3r::SurfacesPtr &surfaces, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return to_polygons(expolygons_offset(surfaces, delta, joinType, miterLimit)); }
Slic3r::ExPolygons offset_ex(const Slic3r::ExPolygon &expolygon, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    //FIXME one may spare one Clipper Union call.
    { return ClipperPaths_to_Slic3rExPolygons(expolygon_offset(expolygon, delta, joinType, miterLimit)); }
Slic3r::ExPolygons offset_ex(const Slic3r::ExPolygons &expolygons, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return PolyTreeToExPolygons(expolygons_offset_pt(expolygons, delta, joinType, miterLimit)); }
Slic3r::ExPolygons offset_ex(const Slic3r::Surfaces &surfaces, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return PolyTreeToExPolygons(expolygons_offset_pt(surfaces, delta, joinType, miterLimit)); }
Slic3r::ExPolygons offset_ex(const Slic3r::SurfacesPtr &surfaces, const float delta, ClipperLib::JoinType joinType, double miterLimit)
    { return PolyTreeToExPolygons(expolygons_offset_pt(surfaces, delta, joinType, miterLimit)); }

Polygons offset2(const ExPolygons &expolygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    return to_polygons(offset_paths<ClipperLib::Paths>(expolygons_offset(expolygons, delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
ExPolygons offset2_ex(const ExPolygons &expolygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    return PolyTreeToExPolygons(offset_paths<ClipperLib::PolyTree>(expolygons_offset(expolygons, delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
ExPolygons offset2_ex(const Surfaces &surfaces, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    //FIXME it may be more efficient to offset to_expolygons(surfaces) instead of to_polygons(surfaces).
    return PolyTreeToExPolygons(offset_paths<ClipperLib::PolyTree>(expolygons_offset(surfaces, delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}

// 先向外偏移，再向内偏移产生形态学闭合。所有增量应为正数。
Slic3r::Polygons closing(const Slic3r::Polygons &polygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    return to_polygons(shrink_paths<ClipperLib::Paths>(expand_paths<ClipperLib::Paths>(ClipperUtils::PolygonsProvider(polygons), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
Slic3r::ExPolygons closing_ex(const Slic3r::Polygons &polygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    return PolyTreeToExPolygons(shrink_paths<ClipperLib::PolyTree>(expand_paths<ClipperLib::Paths>(ClipperUtils::PolygonsProvider(polygons), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
Slic3r::ExPolygons closing_ex(const Slic3r::Surfaces &surfaces, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    //FIXME it may be more efficient to offset to_expolygons(surfaces) instead of to_polygons(surfaces).
    return PolyTreeToExPolygons(shrink_paths<ClipperLib::PolyTree>(expand_paths<ClipperLib::Paths>(ClipperUtils::SurfacesProvider(surfaces), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}

// 先向内偏移再向外偏移产生形态学开运算。所有增量应为正值。
Slic3r::Polygons opening(const Slic3r::Polygons &polygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    return to_polygons(expand_paths<ClipperLib::Paths>(shrink_paths<ClipperLib::Paths>(ClipperUtils::PolygonsProvider(polygons), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
Slic3r::Polygons opening(const Slic3r::ExPolygons &expolygons, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    return to_polygons(expand_paths<ClipperLib::Paths>(shrink_paths<ClipperLib::Paths>(ClipperUtils::ExPolygonsProvider(expolygons), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}
Slic3r::Polygons opening(const Slic3r::Surfaces &surfaces, const float delta1, const float delta2, ClipperLib::JoinType joinType, double miterLimit)
{
    assert(delta1 > 0);
    assert(delta2 > 0);
    //FIXME it may be more efficient to offset to_expolygons(surfaces) instead of to_polygons(surfaces).
    return to_polygons(expand_paths<ClipperLib::Paths>(shrink_paths<ClipperLib::Paths>(ClipperUtils::SurfacesProvider(surfaces), delta1, joinType, miterLimit), delta2, joinType, miterLimit));
}

// 修复 #117：大型分形金字塔切片需要很长时间
// Clipper 库在处理重叠多边形时存在困难。
// 即，如果操作的输出是 PolyTree 类型，函数 ClipperLib::JoinCommonEdges() 的时间复杂度可能会非常糟糕。
// 此函数实现了以下解决方法：
// 1) 执行 Clipper 操作并将输出保存到 Paths。此方法在合理时间内处理重叠。
// 2) 再次运行 Clipper Union 以从 1) 的结果中提取 PolyTree。
template<typename PathProvider1, typename PathProvider2>
inline ClipperLib::PolyTree clipper_do_polytree(
    const ClipperLib::ClipType       clipType,
    PathProvider1                  &&subject,
    PathProvider2                  &&clip,
    const ClipperLib::PolyFillType   fillType)
{
    // 执行操作并将输出保存到 input_subject。
    // 此遍不生成 PolyTree，在当前 Clipper 库中，如果有重叠边，PolyTree 是非常昂贵的操作。
    if (auto output = clipper_do<ClipperLib::Paths>(clipType, subject, clip, fillType); ! output.empty())
        // 执行额外的 Union 操作以生成 PolyTree 排序。
        return clipper_union<ClipperLib::PolyTree>(output, fillType);
    return ClipperLib::PolyTree();
}
template<typename PathProvider1, typename PathProvider2>
inline ClipperLib::PolyTree clipper_do_polytree(
    const ClipperLib::ClipType       clipType,
    PathProvider1                  &&subject,
    PathProvider2                  &&clip,
    const ClipperLib::PolyFillType   fillType,
    const ApplySafetyOffset          do_safety_offset)
{
    assert(do_safety_offset == ApplySafetyOffset::No || clipType != ClipperLib::ctUnion);
    return do_safety_offset == ApplySafetyOffset::Yes ? 
        clipper_do_polytree(clipType, std::forward<PathProvider1>(subject), safety_offset(std::forward<PathProvider2>(clip)), fillType) :
        clipper_do_polytree(clipType, std::forward<PathProvider1>(subject), std::forward<PathProvider2>(clip), fillType);
}

template<class TSubj, class TClip>
static inline Polygons _clipper(ClipperLib::ClipType clipType, TSubj &&subject, TClip &&clip, ApplySafetyOffset do_safety_offset)
{
    return to_polygons(clipper_do<ClipperLib::Paths>(clipType, std::forward<TSubj>(subject), std::forward<TClip>(clip), ClipperLib::pftNonZero, do_safety_offset));
}

Slic3r::Polygons diff(const Slic3r::Polygon &subject, const Slic3r::Polygon &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::SinglePathProvider(clip.points), do_safety_offset); }
Slic3r::Polygons diff(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons diff_clipped(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset) 
    { return diff(subject, ClipperUtils::clip_clipper_polygons_with_subject_bbox(clip, get_extents(subject).inflated(SCALED_EPSILON)), do_safety_offset); }
Slic3r::ExPolygons diff_clipped(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return diff_ex(subject, ClipperUtils::clip_clipper_polygons_with_subject_bbox(clip, get_extents(subject).inflated(SCALED_EPSILON)), do_safety_offset); }
Slic3r::ExPolygons diff_clipped(const Slic3r::ExPolygons & subject, const Slic3r::ExPolygons & clip, ApplySafetyOffset do_safety_offset)
{
    return diff_ex(subject, ClipperUtils::clip_clipper_polygons_with_subject_bbox(clip, get_extents(subject).inflated(SCALED_EPSILON)), do_safety_offset);
}
Slic3r::Polygons diff(const Slic3r::Polygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons diff(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons diff(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons diff(const Slic3r::Surfaces &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctDifference, ClipperUtils::SurfacesProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::Polygon &subject, const Slic3r::Polygon &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::SinglePathProvider(clip.points), do_safety_offset); }
Slic3r::Polygons intersection_clipped(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset) 
    { return intersection(subject, ClipperUtils::clip_clipper_polygons_with_subject_bbox(clip, get_extents(subject).inflated(SCALED_EPSILON)), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::Polygons &subject, const Slic3r::ExPolygon &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::PolygonsProvider(subject), ClipperUtils::ExPolygonProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::ExPolygon &subject, const Slic3r::ExPolygon &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::ExPolygonProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::Surfaces &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::SurfacesProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::Polygons intersection(const Slic3r::Surfaces &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper(ClipperLib::ctIntersection, ClipperUtils::SurfacesProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
// BBS
Slic3r::Polygons intersection(const Slic3r::Polygons& subject, const Slic3r::Polygon& clip, ApplySafetyOffset do_safety_offset)
{
    Slic3r::Polygons clip_temp;
    clip_temp.push_back(clip);
    return intersection(subject, clip_temp, do_safety_offset);
}

Slic3r::Polygons union_(const Slic3r::Polygons &subject)
    { return _clipper(ClipperLib::ctUnion, ClipperUtils::PolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ApplySafetyOffset::No); }
Slic3r::Polygons union_(const Slic3r::ExPolygons &subject)
    { return _clipper(ClipperLib::ctUnion, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ApplySafetyOffset::No); }
Slic3r::Polygons union_(const Slic3r::Polygons &subject, const ClipperLib::PolyFillType fillType)
    { return to_polygons(clipper_do<ClipperLib::Paths>(ClipperLib::ctUnion, ClipperUtils::PolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), fillType, ApplySafetyOffset::No)); }
Slic3r::Polygons union_(const Slic3r::Polygons &subject, const Slic3r::Polygons &subject2)
    {
        // BBS
        Polygons polys = subject;
        for (const Polygon& poly : subject2)
            polys.push_back(poly);
        return union_(polys);
    }

template <typename TSubject, typename TClip>
static ExPolygons _clipper_ex(ClipperLib::ClipType clipType, TSubject &&subject,  TClip &&clip, ApplySafetyOffset do_safety_offset, ClipperLib::PolyFillType fill_type = ClipperLib::pftNonZero)
    { return PolyTreeToExPolygons(clipper_do_polytree(clipType, std::forward<TSubject>(subject), std::forward<TClip>(clip), fill_type, do_safety_offset)); }

Slic3r::ExPolygons diff_ex(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Polygons &subject, const Slic3r::Surfaces &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::SurfacesProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Polygon &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Polygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::ExPolygon &subject, const Slic3r::Polygon &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::SinglePathProvider(clip.points), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::ExPolygon &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Surfaces &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SurfacesProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Surfaces &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SurfacesProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::ExPolygons &subject, const Slic3r::Surfaces &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::SurfacesProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::Surfaces &subject, const Slic3r::Surfaces &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SurfacesProvider(subject), ClipperUtils::SurfacesProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::SurfacesPtr &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SurfacesPtrProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons diff_ex(const Slic3r::SurfacesPtr &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctDifference, ClipperUtils::SurfacesPtrProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
// BBS
inline Slic3r::ExPolygons diff_ex(const Slic3r::Polygon& subject, const Slic3r::Polygons& clip, ApplySafetyOffset do_safety_offset)
{
    Slic3r::Polygons subject_temp;
    subject_temp.push_back(subject);

    return diff_ex(subject_temp, clip, do_safety_offset);
}

inline Slic3r::ExPolygons diff_ex(const Slic3r::Polygon& subject, const Slic3r::Polygon& clip, ApplySafetyOffset do_safety_offset)
{
    Slic3r::Polygons subject_temp;
    Slic3r::Polygons clip_temp;

    subject_temp.push_back(subject);
    clip_temp.push_back(clip);
    return diff_ex(subject_temp, clip_temp, do_safety_offset);
}

Slic3r::ExPolygons intersection_ex(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygon &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygon& subject, const Slic3r::ExPolygon& clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::ExPolygonProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::Polygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::PolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygons& subject, const Slic3r::ExPolygon& clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonProvider(clip), do_safety_offset);}
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygon& subject, const Slic3r::ExPolygons& clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset);}
Slic3r::ExPolygons intersection_ex(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::Surfaces &subject, const Slic3r::Polygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::SurfacesProvider(subject), ClipperUtils::PolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::Surfaces &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::SurfacesProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::Surfaces &subject, const Slic3r::Surfaces &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::SurfacesProvider(subject), ClipperUtils::SurfacesProvider(clip), do_safety_offset); }
Slic3r::ExPolygons intersection_ex(const Slic3r::SurfacesPtr &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset)
    { return _clipper_ex(ClipperLib::ctIntersection, ClipperUtils::SurfacesPtrProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset); }
// 可通过提供 fill_type (pftEvenOdd, pftNonZero, pftPositive, pftNegative) 来"修复"异常模型（3DLabPrints 等）。
Slic3r::ExPolygons union_ex(const Slic3r::Polygons &subject, ClipperLib::PolyFillType fill_type)
    { return _clipper_ex(ClipperLib::ctUnion, ClipperUtils::PolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ApplySafetyOffset::No, fill_type); }
Slic3r::ExPolygons union_ex(const Slic3r::ExPolygons &subject)
    { return PolyTreeToExPolygons(clipper_do_polytree(ClipperLib::ctUnion, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ClipperLib::pftNonZero)); }
Slic3r::ExPolygons union_ex(const Slic3r::ExPolygons &subject, const Slic3r::Polygons &subject2)
    { return PolyTreeToExPolygons(clipper_do_polytree(ClipperLib::ctUnion, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::PolygonsProvider(subject2), ClipperLib::pftNonZero)); }
Slic3r::ExPolygons union_ex(const Slic3r::Surfaces &subject)
    { return PolyTreeToExPolygons(clipper_do_polytree(ClipperLib::ctUnion, ClipperUtils::SurfacesProvider(subject), ClipperUtils::EmptyPathsProvider(), ClipperLib::pftNonZero)); }
// BBS
Slic3r::ExPolygons union_ex(const Slic3r::ExPolygons& poly1, const Slic3r::ExPolygons& poly2, bool safety_offset_)
    {
    ExPolygons expolys = poly1;
    for (const ExPolygon& expoly : poly2)
        expolys.push_back(expoly);
    return union_ex(expolys);
}

Slic3r::ExPolygons xor_ex(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygon &clip, ApplySafetyOffset do_safety_offset) {
    return _clipper_ex(ClipperLib::ctXor, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonProvider(clip), do_safety_offset);
}
Slic3r::ExPolygons xor_ex(const Slic3r::ExPolygons &subject, const Slic3r::ExPolygons &clip, ApplySafetyOffset do_safety_offset) {
    return _clipper_ex(ClipperLib::ctXor, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::ExPolygonsProvider(clip), do_safety_offset);
}

template<typename PathsProvider1, typename PathsProvider2>
Polylines _clipper_pl_open(ClipperLib::ClipType clipType, PathsProvider1 &&subject, PathsProvider2 &&clip)
{
    ClipperLib::Clipper clipper;
    clipper.AddPaths(std::forward<PathsProvider1>(subject), ClipperLib::ptSubject, false);
    clipper.AddPaths(std::forward<PathsProvider2>(clip), ClipperLib::ptClip, true);
    ClipperLib::PolyTree retval;
    clipper.Execute(clipType, retval, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
    return PolyTreeToPolylines(std::move(retval));
}

// If the split_at_first_point() call above happens to split the polygon inside the clipping area
// we would get two consecutive polylines instead of a single one, so we go through them in order
// to recombine continuous polylines.
static void _clipper_pl_recombine(Polylines &polylines)
{
    for (size_t i = 0; i < polylines.size(); ++i) {
        for (size_t j = i+1; j < polylines.size(); ++j) {
            if (polylines[i].points.back() == polylines[j].points.front()) {
                /* If last point of i coincides with first point of j,
                   append points of j to i and delete j */
                polylines[i].points.insert(polylines[i].points.end(), polylines[j].points.begin()+1, polylines[j].points.end());
                polylines.erase(polylines.begin() + j);
                --j;
            } else if (polylines[i].points.front() == polylines[j].points.back()) {
                /* If first point of i coincides with last point of j,
                   prepend points of j to i and delete j */
                polylines[i].points.insert(polylines[i].points.begin(), polylines[j].points.begin(), polylines[j].points.end()-1);
                polylines.erase(polylines.begin() + j);
                --j;
            } else if (polylines[i].points.front() == polylines[j].points.front()) {
                /* Since Clipper does not preserve orientation of polylines, 
                   also check the case when first point of i coincides with first point of j. */
                polylines[j].reverse();
                polylines[i].points.insert(polylines[i].points.begin(), polylines[j].points.begin(), polylines[j].points.end()-1);
                polylines.erase(polylines.begin() + j);
                --j;
            } else if (polylines[i].points.back() == polylines[j].points.back()) {
                /* Since Clipper does not preserve orientation of polylines, 
                   also check the case when last point of i coincides with last point of j. */
                polylines[j].reverse();
                polylines[i].points.insert(polylines[i].points.end(), polylines[j].points.begin()+1, polylines[j].points.end());
                polylines.erase(polylines.begin() + j);
                --j;
            }
        }
    }
}

template<typename PathProvider1, typename PathProvider2>
Polylines _clipper_pl_closed(ClipperLib::ClipType clipType, PathProvider1 &&subject, PathProvider2 &&clip)
{
    // Transform input polygons into open paths.
    ClipperLib::Paths paths;
    paths.reserve(subject.size());
    for (const Points &poly : subject) {
        // Emplace polygon, duplicate the 1st point.
        paths.push_back({});
        ClipperLib::Path &path = paths.back();
        path.reserve(poly.size() + 1);
        path = poly;
        path.emplace_back(poly.front());
    }
    // perform clipping
    Polylines retval = _clipper_pl_open(clipType, paths, std::forward<PathProvider2>(clip));
    _clipper_pl_recombine(retval);
    return retval;
}

Slic3r::Polylines diff_pl(const Slic3r::Polyline& subject, const Slic3r::Polygons& clip)
    { return _clipper_pl_open(ClipperLib::ctDifference, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::PolygonsProvider(clip)); }
Slic3r::Polylines diff_pl(const Slic3r::Polylines &subject, const Slic3r::Polygons &clip)
    { return _clipper_pl_open(ClipperLib::ctDifference, ClipperUtils::PolylinesProvider(subject), ClipperUtils::PolygonsProvider(clip)); }
Slic3r::Polylines diff_pl(const Slic3r::Polyline &subject, const Slic3r::ExPolygon &clip)
    { return _clipper_pl_open(ClipperLib::ctDifference, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::ExPolygonProvider(clip)); }
Slic3r::Polylines diff_pl(const Slic3r::Polylines &subject, const Slic3r::ExPolygon &clip)
    { return _clipper_pl_open(ClipperLib::ctDifference, ClipperUtils::PolylinesProvider(subject), ClipperUtils::ExPolygonProvider(clip)); }
Slic3r::Polylines diff_pl(const Slic3r::Polylines &subject, const Slic3r::ExPolygons &clip)
    { return _clipper_pl_open(ClipperLib::ctDifference, ClipperUtils::PolylinesProvider(subject), ClipperUtils::ExPolygonsProvider(clip)); }
Slic3r::Polylines diff_pl(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip)
    { return _clipper_pl_closed(ClipperLib::ctDifference, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polylines &subject, const Slic3r::Polygon &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::PolylinesProvider(subject), ClipperUtils::SinglePathProvider(clip.points)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polyline &subject, const Slic3r::ExPolygon &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::ExPolygonProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polylines &subject, const Slic3r::ExPolygon &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::PolylinesProvider(subject), ClipperUtils::ExPolygonProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polyline &subject, const Slic3r::Polygons &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::SinglePathProvider(subject.points), ClipperUtils::PolygonsProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polylines &subject, const Slic3r::Polygons &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::PolylinesProvider(subject), ClipperUtils::PolygonsProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polylines &subject, const Slic3r::ExPolygons &clip)
    { return _clipper_pl_open(ClipperLib::ctIntersection, ClipperUtils::PolylinesProvider(subject), ClipperUtils::ExPolygonsProvider(clip)); }
Slic3r::Polylines intersection_pl(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip)
    { return _clipper_pl_closed(ClipperLib::ctIntersection, ClipperUtils::PolygonsProvider(subject), ClipperUtils::PolygonsProvider(clip)); }

Lines _clipper_ln(ClipperLib::ClipType clipType, const Lines &subject, const Polygons &clip)
{
    // convert Lines to Polylines
    Polylines polylines;
    polylines.reserve(subject.size());
    for (const Line &line : subject)
        polylines.emplace_back(Polyline(line.a, line.b));
    
    // perform operation
    polylines = _clipper_pl_open(clipType, ClipperUtils::PolylinesProvider(polylines), ClipperUtils::PolygonsProvider(clip));
    
    // convert Polylines to Lines
    Lines retval;
    for (Polylines::const_iterator polyline = polylines.begin(); polyline != polylines.end(); ++polyline)
        if (polyline->size() >= 2)
            //FIXME It may happen, that Clipper produced a polyline with more than 2 collinear points by clipping a single line with polygons. It is a very rare issue, but it happens, see GH #6933.
            retval.push_back({ polyline->front(), polyline->back() });
    return retval;
}

// Convert polygons / expolygons into ClipperLib::PolyTree using ClipperLib::pftEvenOdd, thus union will NOT be performed.
// If the contours are not intersecting, their orientation shall not be modified by union_pt().
ClipperLib::PolyTree union_pt(const Polygons &subject)
{
    return clipper_do<ClipperLib::PolyTree>(ClipperLib::ctUnion, ClipperUtils::PolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ClipperLib::pftEvenOdd);
}
ClipperLib::PolyTree union_pt(const ExPolygons &subject)
{
    return clipper_do<ClipperLib::PolyTree>(ClipperLib::ctUnion, ClipperUtils::ExPolygonsProvider(subject), ClipperUtils::EmptyPathsProvider(), ClipperLib::pftEvenOdd);
}

// Simple spatial ordering of Polynodes
ClipperLib::PolyNodes order_nodes(const ClipperLib::PolyNodes &nodes)
{
    // collect ordering points
    Points ordering_points;
    ordering_points.reserve(nodes.size());
    
    for (const ClipperLib::PolyNode *node : nodes)
        ordering_points.emplace_back(
            Point(node->Contour.front().x(), node->Contour.front().y()));

    // perform the ordering
    ClipperLib::PolyNodes ordered_nodes =
        chain_clipper_polynodes(ordering_points, nodes);

    return ordered_nodes;
}

static void traverse_pt_noholes(const ClipperLib::PolyNodes &nodes, Polygons *out)
{
    foreach_node<e_ordering::ON>(nodes, [&out](const ClipperLib::PolyNode *node) 
    {
        traverse_pt_noholes(node->Childs, out);
        out->emplace_back(node->Contour);
        if (node->IsHole()) out->back().reverse(); // ccw
    });
}

static void traverse_pt_outside_in(ClipperLib::PolyNodes &&nodes, Polygons *retval)
{
    // collect ordering points
    Points ordering_points;
    ordering_points.reserve(nodes.size());
    for (const ClipperLib::PolyNode *node : nodes)
        ordering_points.emplace_back(node->Contour.front().x(), node->Contour.front().y());

    // Perform the ordering, push results recursively.
    //FIXME pass the last point to chain_clipper_polynodes?
    for (ClipperLib::PolyNode *node : chain_clipper_polynodes(ordering_points, nodes)) {
        retval->emplace_back(std::move(node->Contour));
        if (node->IsHole()) 
            // Orient a hole, which is clockwise oriented, to CCW.
            retval->back().reverse();
        // traverse the next depth
        traverse_pt_outside_in(std::move(node->Childs), retval);
    }
}

Polygons union_pt_chained_outside_in(const Polygons &subject)
{
    Polygons retval;
    traverse_pt_outside_in(union_pt(subject).Childs, &retval);
    return retval;
}

Polygons simplify_polygons(const Polygons &subject)
{
    ClipperLib::Paths output;
    ClipperLib::Clipper c;
//    c.PreserveCollinear(true);
    //FIXME StrictlySimple is very expensive! Is it needed?
    c.StrictlySimple(true);
    c.AddPaths(ClipperUtils::PolygonsProvider(subject), ClipperLib::ptSubject, true);
    c.Execute(ClipperLib::ctUnion, output, ClipperLib::pftNonZero, ClipperLib::pftNonZero);

    // convert into Slic3r polygons
    return to_polygons(std::move(output));
}

ExPolygons simplify_polygons_ex(const Polygons &subject)
{
    ClipperLib::PolyTree polytree;
    ClipperLib::Clipper c;
//    c.PreserveCollinear(true);
    //FIXME StrictlySimple is very expensive! Is it needed?
    c.StrictlySimple(true);
    c.AddPaths(ClipperUtils::PolygonsProvider(subject), ClipperLib::ptSubject, true);
    c.Execute(ClipperLib::ctUnion, polytree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
    
    // convert into ExPolygons
    return PolyTreeToExPolygons(std::move(polytree));
}

Polygons top_level_islands(const Slic3r::Polygons &polygons)
{
    // init Clipper
    ClipperLib::Clipper clipper;
    clipper.Clear();
    // perform union
    clipper.AddPaths(ClipperUtils::PolygonsProvider(polygons), ClipperLib::ptSubject, true);
    ClipperLib::PolyTree polytree;
    clipper.Execute(ClipperLib::ctUnion, polytree, ClipperLib::pftEvenOdd, ClipperLib::pftEvenOdd); 
    // Convert only the top level islands to the output.
    Polygons out;
    out.reserve(polytree.ChildCount());
    for (int i = 0; i < polytree.ChildCount(); ++i)
        out.emplace_back(std::move(polytree.Childs[i]->Contour));
    return out;
}

// Outer offset shall not split the input contour into multiples. It is expected, that the solution will be non empty and it will contain just a single polygon.
ClipperLib::Paths fix_after_outer_offset(
	const ClipperLib::Path 		&input, 
													// combination of default prameters to correspond to void ClipperOffset::Execute(Paths& solution, double delta)
													// to produce a CCW output contour from CCW input contour for a positive offset.
	ClipperLib::PolyFillType 	 filltype, 			// = ClipperLib::pftPositive
	bool 						 reverse_result)	// = false
{
  	ClipperLib::Paths solution;
  	if (! input.empty()) {
		ClipperLib::Clipper clipper;
	  	clipper.AddPath(input, ClipperLib::ptSubject, true);
		clipper.ReverseSolution(reverse_result);
		clipper.Execute(ClipperLib::ctUnion, solution, filltype, filltype);
	}
    return solution;
}

// Inner offset may split the source contour into multiple contours, but one resulting contour shall not lie inside the other.
ClipperLib::Paths fix_after_inner_offset(
	const ClipperLib::Path 		&input, 
													// combination of default prameters to correspond to void ClipperOffset::Execute(Paths& solution, double delta)
													// to produce a CCW output contour from CCW input contour for a negative offset.
	ClipperLib::PolyFillType 	 filltype, 			// = ClipperLib::pftNegative
	bool 						 reverse_result) 	// = true
{
  	ClipperLib::Paths solution;
  	if (! input.empty()) {
		ClipperLib::Clipper clipper;
		clipper.AddPath(input, ClipperLib::ptSubject, true);
		ClipperLib::IntRect r = clipper.GetBounds();
		r.left -= 10; r.top -= 10; r.right += 10; r.bottom += 10;
		if (filltype == ClipperLib::pftPositive)
			clipper.AddPath({ ClipperLib::IntPoint(r.left, r.bottom), ClipperLib::IntPoint(r.left, r.top), ClipperLib::IntPoint(r.right, r.top), ClipperLib::IntPoint(r.right, r.bottom) }, ClipperLib::ptSubject, true);
		else
			clipper.AddPath({ ClipperLib::IntPoint(r.left, r.bottom), ClipperLib::IntPoint(r.right, r.bottom), ClipperLib::IntPoint(r.right, r.top), ClipperLib::IntPoint(r.left, r.top) }, ClipperLib::ptSubject, true);
		clipper.ReverseSolution(reverse_result);
		clipper.Execute(ClipperLib::ctUnion, solution, filltype, filltype);
		if (! solution.empty())
			solution.erase(solution.begin());
	}
	return solution;
}

ClipperLib::Path mittered_offset_path_scaled(const Points &contour, const std::vector<float> &deltas, double miter_limit)
{
	assert(contour.size() == deltas.size());

#ifndef NDEBUG
	// Verify that the deltas are either all positive, or all negative.
	bool positive = false;
	bool negative = false;
	for (float delta : deltas)
		if (delta < 0.f)
			negative = true;
		else if (delta > 0.f)
			positive = true;
	assert(! (negative && positive));
#endif /* NDEBUG */

	ClipperLib::Path out;

	if (deltas.size() > 2)
	{
		out.reserve(contour.size() * 2);

		// Clamp miter limit to 2.
		miter_limit = (miter_limit > 2.) ? 2. / (miter_limit * miter_limit) : 0.5;
		
		// perpenduclar vector
		auto   perp = [](const Vec2d &v) -> Vec2d { return Vec2d(v.y(), - v.x()); };

		// Add a new point to the output, scale by CLIPPER_OFFSET_SCALE and round to ClipperLib::cInt.
		auto   add_offset_point = [&out](Vec2d pt) {
            pt += Vec2d(0.5 - (pt.x() < 0), 0.5 - (pt.y() < 0));
			out.emplace_back(ClipperLib::cInt(pt.x()), ClipperLib::cInt(pt.y()));
		};

		// Minimum edge length, squared.
		double lmin  = *std::max_element(deltas.begin(), deltas.end()) * ClipperOffsetShortestEdgeFactor;
		double l2min = lmin * lmin;
		// Minimum angle to consider two edges to be parallel.
		// Vojtech's estimate.
//		const double sin_min_parallel = EPSILON + 1. / double(CLIPPER_OFFSET_SCALE);
		// Implementation equal to Clipper.
		const double sin_min_parallel = 1.;

		// Find the last point further from pt by l2min.
		Vec2d  pt     = contour.front().cast<double>();
		size_t iprev  = contour.size() - 1;
		Vec2d  ptprev;
		for (; iprev > 0; -- iprev) {
			ptprev = contour[iprev].cast<double>();
			if ((ptprev - pt).squaredNorm() > l2min)
				break;
		}

		if (iprev != 0) {
			size_t ilast = iprev;
			// Normal to the (pt - ptprev) segment.
			Vec2d nprev = perp(pt - ptprev).normalized();
			for (size_t i = 0; ; ) {
				// Find the next point further from pt by l2min.
				size_t j = i + 1;
				Vec2d ptnext;
				for (; j <= ilast; ++ j) {
					ptnext = contour[j].cast<double>();
					double l2 = (ptnext - pt).squaredNorm();
					if (l2 > l2min)
						break;
				}
				if (j > ilast) {
					assert(i <= ilast);
					// If the last edge is too short, merge it with the previous edge.
					i = ilast;
					ptnext = contour.front().cast<double>();
				}

				// Normal to the (ptnext - pt) segment.
				Vec2d nnext  = perp(ptnext - pt).normalized();

				double delta  = deltas[i];
				double sin_a  = std::clamp(cross2(nprev, nnext), -1., 1.);
				double convex = sin_a * delta;
				if (convex <= - sin_min_parallel) {
					// Concave corner.
					add_offset_point(pt + nprev * delta);
					add_offset_point(pt);
					add_offset_point(pt + nnext * delta);
				} else {
					double dot = nprev.dot(nnext);
					if (convex < sin_min_parallel && dot > 0.) {
						// Nearly parallel.
						add_offset_point((nprev.dot(nnext) > 0.) ? (pt + nprev * delta) : pt);
					} else {
						// Convex corner, possibly extremely sharp if convex < sin_min_parallel.
						double r = 1. + dot;
					  	if (r >= miter_limit)
							add_offset_point(pt + (nprev + nnext) * (delta / r));
					  	else {
							double dx = std::tan(std::atan2(sin_a, dot) / 4.);
							Vec2d  newpt1 = pt + (nprev - perp(nprev) * dx) * delta;
							Vec2d  newpt2 = pt + (nnext + perp(nnext) * dx) * delta;
#ifndef NDEBUG
							Vec2d vedge = 0.5 * (newpt1 + newpt2) - pt;
							double dist_norm = vedge.norm();
							assert(std::abs(dist_norm - std::abs(delta)) < SCALED_EPSILON);
#endif /* NDEBUG */
							add_offset_point(newpt1);
							add_offset_point(newpt2);
					  	}
					}
				}

				if (i == ilast)
					break;

				ptprev = pt;
				nprev  = nnext;
				pt     = ptnext;
				i = j;
			}
		}
	}

#if 0
	{
		ClipperLib::Path polytmp(out);
		unscaleClipperPolygon(polytmp);
		Slic3r::Polygon offsetted(std::move(polytmp));
		BoundingBox bbox = get_extents(contour);
		bbox.merge(get_extents(offsetted));
		static int iRun = 0;
		SVG svg(debug_out_path("mittered_offset_path_scaled-%d.svg", iRun ++).c_str(), bbox);
		svg.draw_outline(Polygon(contour), "blue", scale_(0.01));
		svg.draw_outline(offsetted, "red", scale_(0.01));
		svg.draw(contour, "blue", scale_(0.03));
		svg.draw((Points)offsetted, "blue", scale_(0.03));
	}
#endif

	return out;
}

Polygons variable_offset_inner(const ExPolygon &expoly, const std::vector<std::vector<float>> &deltas, double miter_limit)
{
#ifndef NDEBUG
	// Verify that the deltas are all non positive.
	for (const std::vector<float> &ds : deltas)
		for (float delta : ds)
			assert(delta <= 0.);
	assert(expoly.holes.size() + 1 == deltas.size());
#endif /* NDEBUG */

	// 1) Offset the outer contour.
	ClipperLib::Paths contours = fix_after_inner_offset(mittered_offset_path_scaled(expoly.contour.points, deltas.front(), miter_limit), ClipperLib::pftNegative, true);
#ifndef NDEBUG	
	for (auto &c : contours)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 2) Offset the holes one by one, collect the results.
	ClipperLib::Paths holes;
	holes.reserve(expoly.holes.size());
	for (const Polygon& hole : expoly.holes)
		append(holes, fix_after_outer_offset(mittered_offset_path_scaled(hole.points, deltas[1 + &hole - expoly.holes.data()], miter_limit), ClipperLib::pftNegative, false));
#ifndef NDEBUG	
	for (auto &c : holes)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 3) Subtract holes from the contours.
	ClipperLib::Paths output;
	if (holes.empty())
		output = std::move(contours);
	else {
		ClipperLib::Clipper clipper;
		clipper.Clear();
		clipper.AddPaths(contours, ClipperLib::ptSubject, true);
		clipper.AddPaths(holes, ClipperLib::ptClip, true);
		clipper.Execute(ClipperLib::ctDifference, output, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	}

	return to_polygons(std::move(output));
}

Polygons variable_offset_outer(const ExPolygon &expoly, const std::vector<std::vector<float>> &deltas, double miter_limit)
{
#ifndef NDEBUG
	// Verify that the deltas are all non positive.
for (const std::vector<float>& ds : deltas)
		for (float delta : ds)
			assert(delta >= 0.);
	assert(expoly.holes.size() + 1 == deltas.size());
#endif /* NDEBUG */

	// 1) Offset the outer contour.
	ClipperLib::Paths contours = fix_after_outer_offset(mittered_offset_path_scaled(expoly.contour.points, deltas.front(), miter_limit), ClipperLib::pftPositive, false);
#ifndef NDEBUG
	for (auto &c : contours)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 2) Offset the holes one by one, collect the results.
	ClipperLib::Paths holes;
	holes.reserve(expoly.holes.size());
	for (const Polygon& hole : expoly.holes)
		append(holes, fix_after_inner_offset(mittered_offset_path_scaled(hole.points, deltas[1 + &hole - expoly.holes.data()], miter_limit), ClipperLib::pftPositive, true));
#ifndef NDEBUG
	for (auto &c : holes)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 3) Subtract holes from the contours.
	ClipperLib::Paths output;
	if (holes.empty())
		output = std::move(contours);
	else {
		ClipperLib::Clipper clipper;
		clipper.Clear();
		clipper.AddPaths(contours, ClipperLib::ptSubject, true);
		clipper.AddPaths(holes, ClipperLib::ptClip, true);
		clipper.Execute(ClipperLib::ctDifference, output, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	}

	return to_polygons(std::move(output));
}

ExPolygons variable_offset_outer_ex(const ExPolygon &expoly, const std::vector<std::vector<float>> &deltas, double miter_limit)
{
#ifndef NDEBUG
	// Verify that the deltas are all non positive.
for (const std::vector<float>& ds : deltas)
		for (float delta : ds)
			assert(delta >= 0.);
	assert(expoly.holes.size() + 1 == deltas.size());
#endif /* NDEBUG */

	// 1) Offset the outer contour.
	ClipperLib::Paths contours = fix_after_outer_offset(mittered_offset_path_scaled(expoly.contour.points, deltas.front(), miter_limit), ClipperLib::pftPositive, false);
#ifndef NDEBUG
	for (auto &c : contours)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 2) Offset the holes one by one, collect the results.
	ClipperLib::Paths holes;
	holes.reserve(expoly.holes.size());
	for (const Polygon& hole : expoly.holes)
		append(holes, fix_after_inner_offset(mittered_offset_path_scaled(hole.points, deltas[1 + &hole - expoly.holes.data()], miter_limit), ClipperLib::pftPositive, true));
#ifndef NDEBUG
	for (auto &c : holes)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 3) Subtract holes from the contours.
	ExPolygons output;
	if (holes.empty()) {
		output.reserve(contours.size());
		for (ClipperLib::Path &path : contours) 
			output.emplace_back(std::move(path));
	} else {
		ClipperLib::Clipper clipper;
		clipper.AddPaths(contours, ClipperLib::ptSubject, true);
		clipper.AddPaths(holes, ClipperLib::ptClip, true);
	    ClipperLib::PolyTree polytree;
		clipper.Execute(ClipperLib::ctDifference, polytree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	    output = PolyTreeToExPolygons(std::move(polytree));
	}

	return output;
}


ExPolygons variable_offset_inner_ex(const ExPolygon &expoly, const std::vector<std::vector<float>> &deltas, double miter_limit)
{
#ifndef NDEBUG
	// Verify that the deltas are all non positive.
	for (const std::vector<float>& ds : deltas)
		for (float delta : ds)
			assert(delta <= 0.);
	assert(expoly.holes.size() + 1 == deltas.size());
#endif /* NDEBUG */

	// 1) Offset the outer contour.
	ClipperLib::Paths contours = fix_after_inner_offset(mittered_offset_path_scaled(expoly.contour.points, deltas.front(), miter_limit), ClipperLib::pftNegative, true);
#ifndef NDEBUG
	for (auto &c : contours)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 2) Offset the holes one by one, collect the results.
	ClipperLib::Paths holes;
	holes.reserve(expoly.holes.size());
	for (const Polygon& hole : expoly.holes)
		append(holes, fix_after_outer_offset(mittered_offset_path_scaled(hole.points, deltas[1 + &hole - expoly.holes.data()], miter_limit), ClipperLib::pftNegative, false));
#ifndef NDEBUG
	for (auto &c : holes)
		assert(ClipperLib::Area(c) > 0.);
#endif /* NDEBUG */

	// 3) Subtract holes from the contours.
	ExPolygons output;
	if (holes.empty()) {
		output.reserve(contours.size());
		for (ClipperLib::Path &path : contours) 
			output.emplace_back(std::move(path));
	} else {
		ClipperLib::Clipper clipper;
		clipper.AddPaths(contours, ClipperLib::ptSubject, true);
		clipper.AddPaths(holes, ClipperLib::ptClip, true);
	    ClipperLib::PolyTree polytree;
		clipper.Execute(ClipperLib::ctDifference, polytree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
	    output = PolyTreeToExPolygons(std::move(polytree));
	}

	return output;
}

Pointfs make_counter_clockwise(const Pointfs& pointfs)
{
    Pointfs ps = pointfs;
    if (Polygon::new_scale(pointfs).is_clockwise()) {
        std::reverse(ps.begin(), ps.end());
    }

    return ps;
}

}
