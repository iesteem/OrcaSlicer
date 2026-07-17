#ifndef MINAREABOUNDINGBOX_HPP
#define MINAREABOUNDINGBOX_HPP

#include "libslic3r/Point.hpp"

namespace Slic3r {

class Polygon;
class ExPolygon;

void remove_collinear_points(Polygon& p);
void remove_collinear_points(ExPolygon& p);

/// 一个保存旋转包围盒的类。如果使用多边形类型实例化，它将保存给定多边形的最小面积包围盒。
/// 如果输入多边形是凸的，复杂度与点数呈线性关系。否则需要执行 O(n*log(n)) 的凸包计算。
class MinAreaBoundigBox {
    Point m_axis;    
    long double m_bottom = 0.0l, m_right = 0.0l;
public:
    
    // 多边形可以是凸的或简单的（可能带孔的凸或凹多边形）
    enum PolygonLevel {
        pcConvex, pcSimple
    };
   
    // 使用 Slic3r 中各种几何数据类型的构造函数。
    // 如果预先知道凸性，可以使用 pcConvex 来跳过凸包计算。
    explicit MinAreaBoundigBox(const Polygon&, PolygonLevel = pcSimple);
    explicit MinAreaBoundigBox(const ExPolygon&, PolygonLevel = pcSimple);
    explicit MinAreaBoundigBox(const Points&, PolygonLevel = pcSimple);
    
    // 返回使包围盒与 X 轴对齐所需的弧度角度。将多边形旋转此角度即可对齐。
    double angle_to_X()  const;
    
    // 包围盒宽度
    long double width()  const;
    
    // 包围盒高度
    long double height() const;
    
    // 包围盒面积
    long double area()   const;
    
    // 旋转包围盒的轴。如果 angle_to_X 不够精确，请使用此非归一化方向向量。
    const Point& axis()  const { return m_axis; }
};

}

#endif // MINAREABOUNDINGBOX_HPP
