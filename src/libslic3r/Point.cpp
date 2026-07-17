#include "Point.hpp"
#include "Line.hpp"
#include "MultiPoint.hpp"
#include "Int128.hpp"
#include "BoundingBox.hpp"
#include <algorithm>

namespace Slic3r {

std::vector<Vec3f> transform(const std::vector<Vec3f>& points, const Transform3f& t)
{
    unsigned int vertices_count = (unsigned int)points.size();
    if (vertices_count == 0)
        return std::vector<Vec3f>();

    unsigned int data_size = 3 * vertices_count * sizeof(float);

    Eigen::MatrixXf src(3, vertices_count);
    ::memcpy((void*)src.data(), (const void*)points.data(), data_size);

    Eigen::MatrixXf dst(3, vertices_count);
    dst = t * src.colwise().homogeneous();

    std::vector<Vec3f> ret_points(vertices_count, Vec3f::Zero());
    ::memcpy((void*)ret_points.data(), (const void*)dst.data(), data_size);
    return ret_points;
}

Pointf3s transform(const Pointf3s& points, const Transform3d& t)
{
    unsigned int vertices_count = (unsigned int)points.size();
    if (vertices_count == 0)
        return Pointf3s();

    unsigned int data_size = 3 * vertices_count * sizeof(double);

    Eigen::MatrixXd src(3, vertices_count);
    ::memcpy((void*)src.data(), (const void*)points.data(), data_size);

    Eigen::MatrixXd dst(3, vertices_count);
    dst = t * src.colwise().homogeneous();

    Pointf3s ret_points(vertices_count, Vec3d::Zero());
    ::memcpy((void*)ret_points.data(), (const void*)dst.data(), data_size);
    return ret_points;
}

void Point::rotate(double angle, const Point &center)
{
    Vec2d  cur = this->cast<double>();
    double s   = ::sin(angle);
    double c   = ::cos(angle);
    Vec2d  d   = cur - center.cast<double>();
    this->x() = fast_round_up<coord_t>(center.x() + c * d.x() - s * d.y());
    this->y() = fast_round_up<coord_t>(center.y() + s * d.x() + c * d.y());
}

/* 三个点构成逆时针转向如果 ccw > 0，顺时针如果
 * ccw < 0，共线如果 ccw = 0，因为 ccw 是一个行列式，
 * 给出了由 p1、p2 和该点形成的三角形的有符号面积。
 * 换句话说，它是 p1-p2 和 p1-this 的二维叉积，即
 * 它们三维叉积的 z 分量。
 * 我们返回 double 因为它必须足够大以容纳 2*max(|坐标|)^2
 */
double Point::ccw(const Point &p1, const Point &p2) const
{
    // static_assert(sizeof(coord_t) == 4, "Point::ccw() requires a 32 bit coord_t");
    // return cross2((p2 - p1).cast<int64_t>(), (*this - p1).cast<int64_t>());
   return cross2((p2 - p1).cast<double>(), (*this - p1).cast<double>());
}

double Point::ccw(const Line &line) const
{
    return this->ccw(line.a, line.b);
}

// 返回 this-p1 和 this-p2 之间的 CCW 角度
// 即假设从 p1 到 p2 绕 this 做 CCW 旋转
double Point::ccw_angle(const Point &p1, const Point &p2) const
{
    //FIXME this calculates an atan2 twice! Project one vector into the other!
    double angle = atan2(p1.x() - (*this).x(), p1.y() - (*this).y())
                 - atan2(p2.x() - (*this).x(), p2.y() - (*this).y());
    // 我们只想要返回正角度
    return angle <= 0 ? angle + 2*PI : angle;
}

Point Point::projection_onto(const MultiPoint &poly) const
{
    Point running_projection = poly.first_point();
    double running_min = (running_projection - *this).cast<double>().norm();
    
    Lines lines = poly.lines();
    for (Lines::const_iterator line = lines.begin(); line != lines.end(); ++line) {
        Point point_temp = this->projection_onto(*line);
        if ((point_temp - *this).cast<double>().norm() < running_min) {
	        running_projection = point_temp;
	        running_min = (running_projection - *this).cast<double>().norm();
        }
    }
    return running_projection;
}

Point Point::projection_onto(const Line &line) const
{
    if (line.a == line.b) return line.a;
    
    /*
        (从 VisiLibity 移植，作者 Karl J. Obermeyer)
        point_temp 在由 line_segment_temp 确定的直线上的投影
        可以表示为仿射组合
        形式为
        Point 的投影 = theta*line_segment_temp.first + (1.0-theta)*line_segment_temp.second。
        如果 theta 在区间 [0,1] 之外，则 Line_Segment 的一个端点
        必须最接近调用 Point。
    */
    double lx = (double)(line.b(0) - line.a(0));
    double ly = (double)(line.b(1) - line.a(1));
    double theta = ( (double)(line.b(0) - (*this)(0))*lx + (double)(line.b(1)- (*this)(1))*ly ) 
          / ( sqr<double>(lx) + sqr<double>(ly) );
    
    if (0.0 <= theta && theta <= 1.0)
        return (theta * line.a.cast<coordf_t>() + (1.0-theta) * line.b.cast<coordf_t>()).cast<coord_t>();
    
    // 否则选择最近的端点。
    return ((line.a - *this).cast<double>().squaredNorm() < (line.b - *this).cast<double>().squaredNorm()) ? line.a : line.b;
}

bool has_duplicate_points(Points &&pts)
{
    std::sort(pts.begin(), pts.end());
    for (size_t i = 1; i < pts.size(); ++ i)
        if (pts[i - 1] == pts[i])
            return true;
    return false;
}

Points collect_duplicates(Points pts /* Copy */)
{
    std::sort(pts.begin(), pts.end());
    Points duplicits;
    const Point *prev = &pts.front();
    for (size_t i = 1; i < pts.size(); ++i) {
        const Point *act = &pts[i];
        if (*prev == *act) {
            // 重复点
            if (!duplicits.empty() && duplicits.back() == *act)
                continue; // 仅唯一重复点
            duplicits.push_back(*act);
        }
        prev = act;
    }
    return duplicits;
}

template<bool IncludeBoundary>
BoundingBox get_extents(const Points &pts)
{ 
    BoundingBox out;
    BoundingBox::construct<IncludeBoundary>(out, pts.begin(), pts.end());
    return out;
}
template BoundingBox get_extents<false>(const Points &pts);
template BoundingBox get_extents<true>(const Points &pts);

// 如果 IncludeBoundary，则即使对于单个点也定义边界框。
// 否则，仅当边界框具有正面积时才定义。
template<bool IncludeBoundary>
BoundingBox get_extents(const VecOfPoints &pts)
{
    BoundingBox bbox;
    for (const Points &p : pts)
        bbox.merge(get_extents<IncludeBoundary>(p));
    return bbox;
}
template BoundingBox get_extents<false>(const VecOfPoints &pts);
template BoundingBox get_extents<true>(const VecOfPoints &pts);

BoundingBoxf get_extents(const std::vector<Vec2d> &pts)
{
    BoundingBoxf bbox;
    for (const Vec2d &p : pts)
        bbox.merge(p);
    return bbox;
}


int Point::nearest_point_index(const Points &points) const
{
    PointConstPtrs p;
    p.reserve(points.size());
    for (Points::const_iterator it = points.begin(); it != points.end(); ++it)
        p.push_back(&*it);
    return this->nearest_point_index(p);
}

int Point::nearest_point_index(const PointConstPtrs &points) const
{
    int idx = -1;
    double distance = -1;  // double because long is limited to 2147483647 on some platforms and it's not enough
    
    for (PointConstPtrs::const_iterator it = points.begin(); it != points.end(); ++it) {
        /* 如果候选点的 X 距离大于之前最佳候选点的总距离，
           我们知道不需要它 */
        double d = sqr<double>((*this)(0) - (*it)->x());
        if (distance != -1 && d > distance) continue;
        
        /* 如果候选点的 Y 距离大于之前最佳候选点的总距离，
           我们知道不需要它 */
        d += sqr<double>((*this)(1) - (*it)->y());
        if (distance != -1 && d > distance) continue;
        
        idx = it - points.begin();
        distance = d;
        
        if (distance < EPSILON) break;
    }
    
    return idx;
}

int Point::nearest_point_index(const PointPtrs &points) const
{
    PointConstPtrs p;
    p.reserve(points.size());
    for (PointPtrs::const_iterator it = points.begin(); it != points.end(); ++it)
        p.push_back(*it);
    return this->nearest_point_index(p);
}

bool Point::nearest_point(const Points &points, Point* point) const
{
    int idx = this->nearest_point_index(points);
    if (idx == -1) return false;
    *point = points.at(idx);
    return true;
}

std::ostream& operator<<(std::ostream &stm, const Vec2d &pointf)
{
    return stm << pointf(0) << "," << pointf(1);
}

namespace int128 {

int orient(const Vec2crd &p1, const Vec2crd &p2, const Vec2crd &p3)
{
    Slic3r::Vector v1(p2 - p1);
    Slic3r::Vector v2(p3 - p1);
    return Int128::sign_determinant_2x2_filtered(v1.x(), v1.y(), v2.x(), v2.y());
}

int cross(const Vec2crd &v1, const Vec2crd &v2)
{
    return Int128::sign_determinant_2x2_filtered(v1.x(), v1.y(), v2.x(), v2.y());
}

}

}
