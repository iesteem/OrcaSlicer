#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"

#include "FillPlanePath.hpp"

namespace Slic3r {

class InfillPolylineClipper : public InfillPolylineOutput {
public:
    InfillPolylineClipper(const BoundingBox bbox, const double scale_out) : InfillPolylineOutput(scale_out), m_bbox(bbox) {}

    void            add_point(const Vec2d &pt);
    Points&&        result() { return std::move(m_out); }
    bool            clips() const override { return true; }

private:
    enum class Side {
        Left   = 1,
        Right  = 2,
        Top    = 4,
        Bottom = 8
    };

    int sides(const Point &p) const {
        return int(p.x() < m_bbox.min.x()) * int(Side::Left) +
               int(p.x() > m_bbox.max.x()) * int(Side::Right) +
               int(p.y() < m_bbox.min.y()) * int(Side::Bottom) +
               int(p.y() > m_bbox.max.y()) * int(Side::Top);
    };

    // 用于裁剪多段线的边界框。
    BoundingBox m_bbox;

    // 对最近处理的两个点的分类。
    int         m_sides_prev;
    int         m_sides_this;
};

void InfillPolylineClipper::add_point(const Vec2d &fpt)
{
    const Point pt{ this->scaled(fpt) };

    if (m_out.size() < 2) {
        // 收集前两个点及其状态。
        (m_out.empty() ? m_sides_prev : m_sides_this) = sides(pt);
        m_out.emplace_back(pt);
    } else {
        // 分类最后插入的点，可能将其移除。
        int sides_next = sides(pt);
        if (// 此点在内部。接受它。
            m_sides_this == 0 ||
            // 要么此点在外面且前一个或后一个在里面，要么
            // 边缘可能切割了边界框的角落。
            (m_sides_prev & m_sides_this & sides_next) == 0) {
            // 保留最后一个点。
            m_sides_prev = m_sides_this;
        } else {
            // 所有三个点（当前、前一个、后一个）都在同一边的外部。
            // 忽略最后一个点。
            m_out.pop_back();
        }
        // 保存当前点。
        m_out.emplace_back(pt);
        m_sides_this = sides_next;
    }
}

void FillPlanePath::_fill_surface_single(
    const FillParams                &params, 
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction, 
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    expolygon.rotate(-direction.first);

    //FIXME Vojtech: 我们不确定用户是否希望可见表面上的填充图案在单层的所有孤岛之间对齐。
    // 可以使用 this->centered() 来对齐阿基米德弦和八角螺旋图案。
    const bool align = params.density < 0.995;

    BoundingBox snug_bounding_box = get_extents(expolygon).inflated(SCALED_EPSILON);

    // 要填充图案的区域的旋转边界框。
    BoundingBox bounding_box = align ?
        // 稀疏填充需要在层间对齐。使用物体的边界框在层间对齐填充。
        this->bounding_box.rotated(-direction.first) :
        // 实体填充不需要在层间对齐，仅围绕裁剪 expolygon 生成填充图案。
        snug_bounding_box;

    Point shift = this->centered() ? 
        bounding_box.center() :
        bounding_box.min;
    expolygon.translate(-shift.x(), -shift.y());
    bounding_box.translate(-shift.x(), -shift.y());

    Polyline polyline;
    {
        auto distance_between_lines = scaled<double>(this->spacing) / params.density;
        auto min_x = coord_t(ceil(coordf_t(bounding_box.min.x()) / distance_between_lines));
        auto min_y = coord_t(ceil(coordf_t(bounding_box.min.y()) / distance_between_lines));
        auto max_x = coord_t(ceil(coordf_t(bounding_box.max.x()) / distance_between_lines));
        auto max_y = coord_t(ceil(coordf_t(bounding_box.max.y()) / distance_between_lines));
        auto resolution = scaled<double>(params.resolution) / distance_between_lines;
        if (align) {
            // 在整个物体的边界框中填充，根据贴合边界框裁剪生成的多段线。
            snug_bounding_box.translate(-shift.x(), -shift.y());
            InfillPolylineClipper output(snug_bounding_box, distance_between_lines);
            this->generate(min_x, min_y, max_x, max_y, resolution, output);
            polyline.points = std::move(output.result());
        } else {
            // 在贴合边界框中填充，无需裁剪。
            InfillPolylineOutput output(distance_between_lines);
            this->generate(min_x, min_y, max_x, max_y, resolution, output);
            polyline.points = std::move(output.result());
        }
    }

    if (polyline.size() >= 2) {
        Polylines polylines = intersection_pl(polyline, expolygon);
        if (!polylines.empty()) {
            Polylines chained;
            if (params.dont_connect() || params.density > 0.5) {
                // ORCA: 流量校准的特殊标志
                auto is_flow_calib = params.extrusion_role == erTopSolidInfill &&
                                     this->print_object_config->has("calib_flowrate_topinfill_special_order") &&
                                     this->print_object_config->option("calib_flowrate_topinfill_special_order")->getBool() &&
                                     dynamic_cast<FillArchimedeanChords*>(this);
                if (is_flow_calib) {
                    // 我们希望螺旋部分由内向外打印
                    // 首先通过查找最长的线来找到中心螺旋线
                    auto     it            = std::max_element(polylines.begin(), polylines.end(),
                                                              [](const Polyline& a, const Polyline& b) { return a.length() < b.length(); });
                    Polyline center_spiral = std::move(*it);

                    // 确保螺旋由内向外打印
                    if (center_spiral.first_point().squaredNorm() > center_spiral.last_point().squaredNorm()) {
                        center_spiral.reverse();
                    }

                    // 链接其他多段线
                    polylines.erase(it);
                    chained = chain_polylines(std::move(polylines));

                    // 然后将中心螺旋添加回来
                    chained.push_back(std::move(center_spiral));
                } else {
                    chained = chain_polylines(std::move(polylines));
                }
            } else
                connect_infill(std::move(polylines), expolygon, chained, this->spacing, params);
            // 路径必须重新定位并旋转回来
            for (Polyline& pl : chained) {
                pl.translate(shift.x(), shift.y());
                pl.rotate(direction.first);
            }
            append(polylines_out, std::move(chained));
        }
    }
}

// 遵循阿基米德螺旋，极坐标：r=a+b\theta
template<typename Output>
static void generate_archimedean_chords(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, const double resolution, Output &output)
{
    // 要达到的半径。
    coordf_t rmax = std::sqrt(coordf_t(max_x)*coordf_t(max_x)+coordf_t(max_y)*coordf_t(max_y)) * std::sqrt(2.) + 1.5;
    // 现在展开螺旋。
    coordf_t a = 1.;
    coordf_t b = 1./(2.*M_PI);
    coordf_t theta = 0.;
    coordf_t r = 1;
    Pointfs out;
    //FIXME Vojtech: 如果用作实体填充，中心会留下间隙。
    output.add_point({ 0, 0 });
    output.add_point({ 1, 0 });
    while (r < rmax) {
        // 离散化角度，以实现低于分辨率的离散化误差。
        theta += 2. * acos(1. - resolution / r);
        r = a + b * theta;
        output.add_point({ r * cos(theta), r * sin(theta) });
    }
}

void FillArchimedeanChords::generate(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, const double resolution, InfillPolylineOutput &output)
{
    if (output.clips())
        generate_archimedean_chords(min_x, min_y, max_x, max_y, resolution, static_cast<InfillPolylineClipper&>(output));
    else
        generate_archimedean_chords(min_x, min_y, max_x, max_y, resolution, output);
}

// 改编自
// http://cpansearch.perl.org/src/KRYDE/Math-PlanePath-122/lib/Math/PlanePath/HilbertCurve.pm
//
// state=0    3--2   普通
//               |
//            0--1
//
// state=4    1--2  转置
//            |  |
//            0  3
//
// state=8
//
// state=12   3  0  旋转180度 + 转置
//            |  |
//            2--1
//
static inline Point hilbert_n_to_xy(const size_t n)
{
    static constexpr const int next_state[16] { 4,0,0,12, 0,4,4,8, 12,8,8,4, 8,12,12,0 };
    static constexpr const int digit_to_x[16] { 0,1,1,0, 0,0,1,1, 1,0,0,1, 1,1,0,0 };
    static constexpr const int digit_to_y[16] { 0,0,1,1, 0,1,1,0, 1,1,0,0, 1,0,0,1 };

    // Number of 2 bit digits.
    size_t ndigits = 0;
    {
        size_t nc = n;
        while(nc > 0) {
            nc >>= 2;
            ++ ndigits;
        }
    }
    int state = (ndigits & 1) ? 4 : 0;
    coord_t x = 0;
    coord_t y = 0;
    for (int i = (int)ndigits - 1; i >= 0; -- i) {
        int digit = (n >> (i * 2)) & 3;
        state += digit;
        x |= digit_to_x[state] << i;
        y |= digit_to_y[state] << i;
        state = next_state[state];
    }
    return Point(x, y);
}

template<typename Output>
static void generate_hilbert_curve(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, Output &output)
{
    // Minimum power of two square to fit the domain.
    size_t sz = 2;
    size_t pw = 1;
    {
        size_t sz0 = std::max(max_x + 1 - min_x, max_y + 1 - min_y);
        while (sz < sz0) {
            sz = sz << 1;
            ++ pw;
        }
    }

    size_t sz2 = sz * sz;
    output.reserve(sz2);
    for (size_t i = 0; i < sz2; ++ i) {
        Point p = hilbert_n_to_xy(i);
        output.add_point({ p.x() + min_x, p.y() + min_y });
    }
}

void FillHilbertCurve::generate(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, const double /* resolution */, InfillPolylineOutput &output)
{
    if (output.clips())
        generate_hilbert_curve(min_x, min_y, max_x, max_y, static_cast<InfillPolylineClipper&>(output));
    else
        generate_hilbert_curve(min_x, min_y, max_x, max_y, output);
}

template<typename Output>
static void generate_octagram_spiral(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, Output &output)
{
    // 要达到的半径。
    coordf_t rmax = std::sqrt(coordf_t(max_x)*coordf_t(max_x)+coordf_t(max_y)*coordf_t(max_y)) * std::sqrt(2.) + 1.5;
    // 现在展开螺旋。
    coordf_t r = 0;
    coordf_t r_inc = sqrt(2.);
    output.add_point({ 0., 0. });
    while (r < rmax) {
        r += r_inc;
        coordf_t rx = r / sqrt(2.);
        coordf_t r2 = r + rx;
        output.add_point({ r,   0. });
        output.add_point({ r2,  rx });
        output.add_point({ rx,  rx });
        output.add_point({ rx,  r2 });
        output.add_point({ 0.,  r  });
        output.add_point({-rx,  r2 });
        output.add_point({-rx,  rx });
        output.add_point({-r2,  rx });
        output.add_point({- r,  0. });
        output.add_point({-r2, -rx });
        output.add_point({-rx, -rx });
        output.add_point({-rx, -r2 });
        output.add_point({ 0., -r  });
        output.add_point({ rx, -r2 });
        output.add_point({ rx, -rx });
        output.add_point({ r2+r_inc, -rx });
    }
}

void FillOctagramSpiral::generate(coord_t min_x, coord_t min_y, coord_t max_x, coord_t max_y, const double /* resolution */, InfillPolylineOutput &output)
{
    if (output.clips())
        generate_octagram_spiral(min_x, min_y, max_x, max_y, static_cast<InfillPolylineClipper&>(output));
    else
        generate_octagram_spiral(min_x, min_y, max_x, max_y, output);
}

} // namespace Slic3r
