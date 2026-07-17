#include "BoundingBox.hpp"
#include "ClipperUtils.hpp"
#include "Exception.hpp"
#include "Polygon.hpp"
#include "Polyline.hpp"

namespace Slic3r {

double Polygon::length() const
{
    double l = 0;
    if (this->points.size() > 1) {
        l = (this->points.back() - this->points.front()).cast<double>().norm();
        for (size_t i = 1; i < this->points.size(); ++ i)
            l += (this->points[i] - this->points[i - 1]).cast<double>().norm();
    }
    return l;
}

Lines Polygon::lines() const
{
    return to_lines(*this);
}

Polyline Polygon::split_at_vertex(const Point &point) const
{
    // find index of point
    for (const Point &pt : this->points)
        if (pt == point)
            return this->split_at_index(int(&pt - &this->points.front()));
    throw Slic3r::InvalidArgument("Point not found");
    return Polyline();
}

// Split a closed polygon into an open polyline, with the split point duplicated at both ends.
Polyline Polygon::split_at_index(int index) const
{
    Polyline polyline;
    polyline.points.reserve(this->points.size() + 1);
    for (Points::const_iterator it = this->points.begin() + index; it != this->points.end(); ++it)
        polyline.points.push_back(*it);
    for (Points::const_iterator it = this->points.begin(); it != this->points.begin() + index + 1; ++it)
        polyline.points.push_back(*it);
    return polyline;
}

double Polygon::area(const Points &points)
{
    double a = 0.;
    if (points.size() >= 3) {
        Vec2d p1 = points.back().cast<double>();
        for (const Point &p : points) {
            Vec2d p2 = p.cast<double>();
            a += cross2(p1, p2);
            p1 = p2;
        }
    }
    return 0.5 * a;
}

double Polygon::area() const
{
    return Polygon::area(points);
}

bool Polygon::is_counter_clockwise() const
{
    return ClipperLib::Orientation(this->points);
}

bool Polygon::is_clockwise() const
{
    return !this->is_counter_clockwise();
}

bool Polygon::make_counter_clockwise()
{
    if (!this->is_counter_clockwise()) {
        this->reverse();
        return true;
    }
    return false;
}

bool Polygon::make_clockwise()
{
    if (this->is_counter_clockwise()) {
        this->reverse();
        return true;
    }
    return false;
}

void Polygon::douglas_peucker(double tolerance)
{
    this->points.push_back(this->points.front());
    Points p = MultiPoint::_douglas_peucker(this->points, tolerance);
    p.pop_back();
    this->points = std::move(p);
}

void Polygon::round_to_grid(double grid_size)
{
    if (grid_size <= 0.)
        return;
    for (Point &p : this->points) {
        p.x() = coord_t(std::round(double(p.x()) / grid_size) * grid_size);
        p.y() = coord_t(std::round(double(p.y()) / grid_size) * grid_size);
    }
}

Polygons Polygon::simplify(double tolerance) const
{
    // Works on CCW polygons only, CW contour will be reoriented to CCW by Clipper's simplify_polygons()!
    assert(this->is_counter_clockwise());

    // 重复第一个点在末尾，以便对整个多边形应用 Douglas-Peucker
    Points points = this->points;
    points.push_back(points.front());
    Polygon p(MultiPoint::_douglas_peucker(points, tolerance));
    p.points.pop_back();
    
    Polygons pp;
    pp.push_back(p);
    return simplify_polygons(pp);
}

// 仅在凸多边形上调用此函数，否则将返回无效结果
void Polygon::triangulate_convex(Polygons* polygons) const
{
    for (Points::const_iterator it = this->points.begin() + 2; it != this->points.end(); ++it) {
        Polygon p;
        p.points.reserve(3);
        p.points.push_back(this->points.front());
        p.points.push_back(*(it-1));
        p.points.push_back(*it);
        
        // 这应该被替换为对 merge_collinear_segments() 方法的更高效调用
        if (p.area() > 0) polygons->push_back(p);
    }
}

// 质心
// 来源：https://en.wikipedia.org/wiki/Centroid
Point Polygon::centroid() const
{
    double area_sum = 0.;
    Vec2d  c(0., 0.);
    if (points.size() >= 3) {
        Vec2d p1 = points.back().cast<double>();
        for (const Point &p : points) {
            Vec2d p2 = p.cast<double>();
            double a = cross2(p1, p2);
            area_sum += a;
            c += (p1 + p2) * a;
            p1 = p2;
        }
    }
    return Point(Vec2d(c / (3. * area_sum)));
}

bool Polygon::intersection(const Line &line, Point *intersection) const
{
    if (this->points.size() < 2)
        return false;
    if (Line(this->points.front(), this->points.back()).intersection(line, intersection))
        return true;
    for (size_t i = 1; i < this->points.size(); ++ i)
        if (Line(this->points[i - 1], this->points[i]).intersection(line, intersection))
            return true;
    return false;
}

bool Polygon::first_intersection(const Line& line, Point* intersection) const
{
    if (this->points.size() < 2)
        return false;

    bool   found = false;
    double dmin  = 0.;
    Line l(this->points.back(), this->points.front());
    for (size_t i = 0; i < this->points.size(); ++ i) {
        l.b = this->points[i];
        Point ip;
        if (l.intersection(line, &ip)) {
            if (! found) {
                found = true;
                dmin = (line.a - ip).cast<double>().squaredNorm();
                *intersection = ip;
            } else {
                double d = (line.a - ip).cast<double>().squaredNorm();
                if (d < dmin) {
                    dmin = d;
                    *intersection = ip;
                }
            }
        }
        l.a = l.b;
    }
    return found;
}

bool Polygon::intersections(const Line &line, Points *intersections) const
{
    if (this->points.size() < 2)
        return false;

    size_t intersections_size = intersections->size();
    Line l(this->points.back(), this->points.front());
    for (size_t i = 0; i < this->points.size(); ++ i) {
        l.b = this->points[i];
        Point intersection;
        if (l.intersection(line, &intersection))
            intersections->emplace_back(std::move(intersection));
        l.a = l.b;
    }
    return intersections->size() > intersections_size;
}
bool Polygon::overlaps(const Polygons& other) const
{
    if (this->empty() || other.empty())
        return false;
    Polylines pl_out = intersection_pl(to_polylines(other), *this);

    // 参见单元测试 SCENARIO("Clipper diff with polyline", "[Clipper]")
    // 了解 intersection_pl 产生交集的情况。
    return !pl_out.empty() ||
        // 如果 *this 完全在 other 内部，则 pl_out 为空，但 expolygons 重叠。测试这种情况。
        std::any_of(other.begin(), other.end(), [this](auto& poly) {return poly.contains(this->points.front()); });
}
// 借助 FilterFn 从 poly 中筛选点到输出。
// 筛选函数接收两个向量：
// v1: 当前点 - 前一个点
// v2: 下一个点 - 当前点
// 如果该点要被复制到输出，则返回 true。
template<typename FilterFn>
Points filter_points_by_vectors(const Points &poly, FilterFn filter)
{
    // 最后一点是第一个访问的点。
    Point p1 = poly.back();
    // p1 的前一个向量。
    Vec2d v1 = (p1 - *(poly.end() - 2)).cast<double>();

    Points out;
    for (Point p2 : poly) {
        // p2 是当前访问点 p1 的下一个点。
        Vec2d v2 = (p2 - p1).cast<double>();
        if (filter(v1, v2))
            out.emplace_back(p1);
        v1 = v2;
        p1 = p2;
    }
    
    return out;
}

template<typename ConvexConcaveFilterFn>
Points filter_convex_concave_points_by_angle_threshold(const Points &poly, double angle_threshold, ConvexConcaveFilterFn convex_concave_filter)
{
    assert(angle_threshold >= 0.);
    if (angle_threshold > EPSILON) {
        double cos_angle  = cos(angle_threshold);
        return filter_points_by_vectors(poly, [convex_concave_filter, cos_angle](const Vec2d &v1, const Vec2d &v2){
            return convex_concave_filter(v1, v2) && v1.normalized().dot(v2.normalized()) < cos_angle;
        });
    } else {
        return filter_points_by_vectors(poly, [convex_concave_filter](const Vec2d &v1, const Vec2d &v2){
            return convex_concave_filter(v1, v2);
        });
    }
}

Points Polygon::convex_points(double angle_threshold) const
{
    return filter_convex_concave_points_by_angle_threshold(this->points, angle_threshold, [](const Vec2d &v1, const Vec2d &v2){ return cross2(v1, v2) > 0.; });
}

Points Polygon::concave_points(double angle_threshold) const
{
    return filter_convex_concave_points_by_angle_threshold(this->points, angle_threshold, [](const Vec2d &v1, const Vec2d &v2){ return cross2(v1, v2) < 0.; });
}

// 点到多边形的投影。
Point Polygon::point_projection(const Point &point) const
{
    Point proj = point;
    double dmin = std::numeric_limits<double>::max();
    if (! this->points.empty()) {
        for (size_t i = 0; i < this->points.size(); ++ i) {
            const Point &pt0 = this->points[i];
            const Point &pt1 = this->points[(i + 1 == this->points.size()) ? 0 : i + 1];
            double d = (point - pt0).cast<double>().norm();
            if (d < dmin) {
                dmin = d;
                proj = pt0;
            }
            d = (point - pt1).cast<double>().norm();
            if (d < dmin) {
                dmin = d;
                proj = pt1;
            }
            Vec2d v1(coordf_t(pt1(0) - pt0(0)), coordf_t(pt1(1) - pt0(1)));
            coordf_t div = v1.squaredNorm();
            if (div > 0.) {
                Vec2d v2(coordf_t(point(0) - pt0(0)), coordf_t(point(1) - pt0(1)));
                coordf_t t = v1.dot(v2) / div;
                if (t > 0. && t < 1.) {
                    Point foot(coord_t(floor(coordf_t(pt0(0)) + t * v1(0) + 0.5)), coord_t(floor(coordf_t(pt0(1)) + t * v1(1) + 0.5)));
                    d = (point - foot).cast<double>().norm();
                    if (d < dmin) {
                        dmin = d;
                        proj = foot;
                    }
                }
            }
        }
    }
    return proj;
}

std::vector<float> Polygon::parameter_by_length() const
{
    // 根据长度参数化多边形。
    std::vector<float> lengths(points.size()+1, 0.);
    for (size_t i = 1; i < points.size(); ++ i)
        lengths[i] = lengths[i-1] + (points[i] - points[i-1]).cast<float>().norm();
    lengths.back() = lengths[lengths.size()-2] + (points.front() - points.back()).cast<float>().norm();
    return lengths;
}

void Polygon::densify(float min_length, std::vector<float>* lengths_ptr)
{
    std::vector<float> lengths_local;
    std::vector<float>& lengths = lengths_ptr ? *lengths_ptr : lengths_local;

    if (! lengths_ptr) {
        // 未提供长度参数化。自行计算。
        lengths = this->parameter_by_length();
    }

    assert(points.size() == lengths.size() - 1);

    for (size_t j=1; j<=points.size(); ++j) {
        bool last = j == points.size();
        int i = last ? 0 : j;

        if (lengths[j] - lengths[j-1] > min_length) {
            Point diff = points[i] - points[j-1];
            float diff_len = lengths[j] - lengths[j-1];
            float r = (min_length/diff_len);
            Point new_pt = points[j-1] + Point(r*diff[0], r*diff[1]);
            points.insert(points.begin() + j, new_pt);
            lengths.insert(lengths.begin() + j, lengths[j-1] + min_length);
        }
    }
    assert(points.size() == lengths.size() - 1);
}

Polygon Polygon::transform(const Transform3d& trafo) const
{
    unsigned int vertices_count = (unsigned int)points.size();
    Polygon dstpoly;
    dstpoly.points.resize(vertices_count);
    if (vertices_count == 0)
        return dstpoly;

    unsigned int data_size = 3 * vertices_count * sizeof(float);

    Eigen::MatrixXd src(3, vertices_count);
    for (size_t i = 0; i < vertices_count; i++)
    {
        src.col(i) = Vec3d{ double(points[i].x()), double(points[i].y()),0. };
    }

    Eigen::MatrixXd dst(3, vertices_count);
    dst = trafo * src.colwise().homogeneous();

    for (size_t i = 0; i < vertices_count; i++)
    {
        dstpoly.points[i] = { dst(0,i),dst(1,i) };
    }
    return dstpoly;
}

BoundingBox get_extents(const Polygon &poly) 
{ 
    return poly.bounding_box();
}

BoundingBox get_extents(const Polygons &polygons)
{
    BoundingBox bb;
    if (! polygons.empty()) {
        bb = get_extents(polygons.front());
        for (size_t i = 1; i < polygons.size(); ++ i)
            bb.merge(get_extents(polygons[i]));
    }
    return bb;
}

BoundingBox get_extents_rotated(const Polygon &poly, double angle) 
{ 
    return get_extents_rotated(poly.points, angle);
}

BoundingBox get_extents_rotated(const Polygons &polygons, double angle)
{
    BoundingBox bb;
    if (! polygons.empty()) {
        bb = get_extents_rotated(polygons.front().points, angle);
        for (size_t i = 1; i < polygons.size(); ++ i)
            bb.merge(get_extents_rotated(polygons[i].points, angle));
    }
    return bb;
}

extern std::vector<BoundingBox> get_extents_vector(const Polygons &polygons)
{
    std::vector<BoundingBox> out;
    out.reserve(polygons.size());
    for (Polygons::const_iterator it = polygons.begin(); it != polygons.end(); ++ it)
        out.push_back(get_extents(*it));
    return out;
}

// 多边形必须有效（至少三个点），共线点和重复点已被移除。
bool polygon_is_convex(const Points &poly)
{
    if (poly.size() < 3)
        return false;

    Point p0 = poly[poly.size() - 2];
    Point p1 = poly[poly.size() - 1];
    for (size_t i = 0; i < poly.size(); ++ i) {
        Point p2 = poly[i];
        auto det = cross2((p1 - p0).cast<int64_t>(), (p2 - p1).cast<int64_t>());
        if (det < 0)
            return false;
        p0 = p1;
        p1 = p2;
    }
    return true;
}

bool has_duplicate_points(const Polygons &polys)
{
#if 1
    // 全局检查。
    Points allpts;
    allpts.reserve(count_points(polys));
    for (const Polygon &poly : polys)
        allpts.insert(allpts.end(), poly.points.begin(), poly.points.end());
    return has_duplicate_points(std::move(allpts));
#else
    // 按轮廓检查。
    for (const Polygon &poly : polys)
        if (has_duplicate_points(poly))
            return true;
    return false;
#endif
}

bool remove_same_neighbor(Polygon &polygon)
{
    Points &points = polygon.points;
    if (points.empty())
        return false;
    auto last = std::unique(points.begin(), points.end());

    // 移除首尾相邻重复
    if (const Point &last_point = *(last - 1); last_point == points.front()) {
        --last;
    }

    // 无重复
    if (last == points.end())
        return false;

    points.erase(last, points.end());
    return true;
}

bool remove_same_neighbor(Polygons &polygons)
{
    if (polygons.empty())
        return false;
    bool exist = false;
    for (Polygon &polygon : polygons)
        exist |= remove_same_neighbor(polygon);
    // 移除空多边形
    polygons.erase(std::remove_if(polygons.begin(), polygons.end(), [](const Polygon &p) { return p.points.size() <= 2; }), polygons.end());
    return exist;
}

static inline bool is_stick(const Point &p1, const Point &p2, const Point &p3)
{
    Point v1 = p2 - p1;
    Point v2 = p3 - p2;
    int64_t dir = int64_t(v1(0)) * int64_t(v2(0)) + int64_t(v1(1)) * int64_t(v2(1));
    if (dir > 0)
        // p3 没有折回 p1。不移除 p2。
        return false;
    double l2_1 = double(v1(0)) * double(v1(0)) + double(v1(1)) * double(v1(1));
    double l2_2 = double(v2(0)) * double(v2(0)) + double(v2(1)) * double(v2(1));
    if (dir == 0)
        // p1, p2, p3 可能形成一个垂直角，或者存在零边长度。
        // 如果 p2 与 p1 或 p2 重合，则移除 p2。
        return l2_1 == 0 || l2_2 == 0;
    // p3 在 p2 之后折回 p1。p1, p2, p3 是否共线？
    // 计算从 p3 到线段 (p1, p2) 或从 p1 到线段 (p2, p3) 的距离，
    // 取较长的线段
    double cross = double(v1(0)) * double(v2(1)) - double(v2(0)) * double(v1(1));
    double dist2 = cross * cross / std::max(l2_1, l2_2);
    return dist2 < EPSILON * EPSILON;
}

bool remove_sticks(Polygon &poly)
{
    bool modified = false;
    size_t j = 1;
    for (size_t i = 1; i + 1 < poly.points.size(); ++ i) {
        if (! is_stick(poly[j-1], poly[i], poly[i+1])) {
            // 保留该点。
            if (j < i)
                poly.points[j] = poly.points[i];
            ++ j;
        }
    }
    if (++ j < poly.points.size()) {
        poly.points[j-1] = poly.points.back();
        poly.points.erase(poly.points.begin() + j, poly.points.end());
        modified = true;
    }
    while (poly.points.size() >= 3 && is_stick(poly.points[poly.points.size()-2], poly.points.back(), poly.points.front())) {
        poly.points.pop_back();
        modified = true;
    }
    while (poly.points.size() >= 3 && is_stick(poly.points.back(), poly.points.front(), poly.points[1]))
        poly.points.erase(poly.points.begin());
    return modified;
}

bool remove_sticks(Polygons &polys)
{
    bool modified = false;
    size_t j = 0;
    for (size_t i = 0; i < polys.size(); ++ i) {
        modified |= remove_sticks(polys[i]);
        if (polys[i].points.size() >= 3) {
            if (j < i) 
                std::swap(polys[i].points, polys[j].points);
            ++ j;
        }
    }
    if (j < polys.size())
        polys.erase(polys.begin() + j, polys.end());
    return modified;
}

bool remove_degenerate(Polygons &polys)
{
    bool modified = false;
    size_t j = 0;
    for (size_t i = 0; i < polys.size(); ++ i) {
        if (polys[i].points.size() >= 3) {
            if (j < i) 
                std::swap(polys[i].points, polys[j].points);
            ++ j;
        } else
            modified = true;
    }
    if (j < polys.size())
        polys.erase(polys.begin() + j, polys.end());
    return modified;
}

bool remove_small(Polygons &polys, double min_area)
{
    bool modified = false;
    size_t j = 0;
    for (size_t i = 0; i < polys.size(); ++ i) {
        if (std::abs(polys[i].area()) >= min_area) {
            if (j < i) 
                std::swap(polys[i].points, polys[j].points);
            ++ j;
        } else
            modified = true;
    }
    if (j < polys.size())
        polys.erase(polys.begin() + j, polys.end());
    return modified;
}

void remove_collinear(Polygon &poly)
{
    if (poly.points.size() > 2) {
        // 复制点并在适当位置追加第 1 个和最后一个点以覆盖边界
        Points pp;
        pp.reserve(poly.points.size()+2);
        pp.push_back(poly.points.back());
        pp.insert(pp.begin()+1, poly.points.begin(), poly.points.end());
        pp.push_back(poly.points.front());
        // 删除旧的点向量。将在循环中重新填充
        poly.points.clear();

        size_t i = 0;
        size_t k = 0;
        while (i < pp.size()-2) {
            k = i+1;
            const Point &p1 = pp[i];
            while (k < pp.size()-1) {
                const Point &p2 = pp[k];
                const Point &p3 = pp[k+1];
                Line l(p1, p3);
                if(l.distance_to(p2) < SCALED_EPSILON) {
                    k++;
                } else {
                    if(i > 0) poly.points.push_back(p1); // implicitly removes the first point we appended above
                    i = k;
                    break;
                }
            }
            if(k > pp.size()-2) break; // all remaining points are collinear and can be skipped
        }
        poly.points.push_back(pp[i]);
    }
}

void remove_collinear(Polygons &polys)
{
	for (Polygon &poly : polys)
		remove_collinear(poly);
}

Polygons polygons_simplify(const Polygons &source_polygons, double tolerance, bool strictly_simple /* = true */)
{
    Polygons out;
    out.reserve(source_polygons.size());
    for (const Polygon &source_polygon : source_polygons) {
        // 在开放多段线上运行 Douglas/Peucker 简化算法（通过在多段线末尾重复第一个点），
        Points simplified = MultiPoint::_douglas_peucker(to_polyline(source_polygon).points, tolerance);
        // 然后移除最后一个（重复的）点。
        simplified.pop_back();
        // 通过 ClipperLib 简化抽取后的轮廓。
        bool ccw = ClipperLib::Area(simplified) > 0.;
        for (Points &path : ClipperLib::SimplifyPolygons(ClipperUtils::SinglePathProvider(simplified), ClipperLib::pftNonZero, strictly_simple)) {
            if (! ccw)
                // ClipperLib 可能将负面积轮廓重新定向为正面积。将孔反转回 CW。
                std::reverse(path.begin(), path.end());
            out.emplace_back(std::move(path));
        }
    }
    return out;
}

// 多边形是否匹配？如果匹配，它们必须具有相同的拓扑结构，
// 但它们的轮廓可能已旋转。
bool polygons_match(const Polygon &l, const Polygon &r)
{
    if (l.size() != r.size())
        return false;
    auto it_l = std::find(l.points.begin(), l.points.end(), r.points.front());
    if (it_l == l.points.end())
        return false;
    auto it_r = r.points.begin();
    for (; it_l != l.points.end(); ++ it_l, ++ it_r)
        if (*it_l != *it_r)
            return false;
    it_l = l.points.begin();
    for (; it_r != r.points.end(); ++ it_l, ++ it_r)
        if (*it_l != *it_r)
            return false;
    return true;
}

bool overlaps(const Polygons& polys1, const Polygons& polys2)
{
    for (const Polygon& poly1 : polys1) {
        if (poly1.overlaps(polys2))
            return true;
    }
    return false;
}

bool contains(const Polygon &polygon, const Point &p, bool border_result)
{
    if (const int poly_count_inside = ClipperLib::PointInPolygon(p, polygon.points); 
        poly_count_inside == -1)
        return border_result;
    else
        return (poly_count_inside % 2) == 1;
}

bool contains(const Polygons &polygons, const Point &p, bool border_result)
{
    int poly_count_inside = 0;
    for (const Polygon &poly : polygons) {
        const int is_inside_this_poly = ClipperLib::PointInPolygon(p, poly.points);
        if (is_inside_this_poly == -1)
            return border_result;
        poly_count_inside += is_inside_this_poly;
    }
    return (poly_count_inside % 2) == 1;
}

Polygon make_circle(double radius, double error)
{
    double angle = 2. * acos(1. - error / radius);
    size_t num_segments = size_t(ceil(2. * M_PI / angle));
    return make_circle_num_segments(radius, num_segments);
}

Polygon make_circle_num_segments(double radius, size_t num_segments)
{
    Polygon out;
    out.points.reserve(num_segments);
    double angle_inc = 2.0 * M_PI / num_segments;
    for (size_t i = 0; i < num_segments; ++ i) {
        const double angle = angle_inc * i;
        out.points.emplace_back(coord_t(cos(angle) * radius), coord_t(sin(angle) * radius));
    }
    return out;
}
}