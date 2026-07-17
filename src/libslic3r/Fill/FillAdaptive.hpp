// 自适应立方体填充受到 @mboerwinkle 为 Cura 所做工作的启发。
// https://github.com/Ultimaker/CuraEngine/issues/381
// https://github.com/Ultimaker/CuraEngine/pull/401
//
// 我们的实现更精确（比 Cura 离散化更少的立方体）
// 仅分割包含三角形的立方体。
// 我们的线条提取在连接提取的线条时是时间最优的，而非 O(n^2)，
// 并且我们还实现了仅用于支持内部悬垂部分的适应性。

#ifndef slic3r_FillAdaptive_hpp_
#define slic3r_FillAdaptive_hpp_

#include "FillBase.hpp"

struct indexed_triangle_set;

namespace Slic3r {

class PrintObject;

namespace FillAdaptive
{

struct Octree;
// 为了保持 Octree 定义的不透明性，我们必须定义自定义删除器。
struct OctreeDeleter { void operator()(Octree *p); };
using  OctreePtr = std::unique_ptr<Octree, OctreeDeleter>;

// 计算以下内容的线间距：
// 1) 自适应立方体填充
// 2) 自适应内部支撑立方体填充
// 如果不需要生成特定填充类型，则返回零。
std::pair<double, double>       adaptive_fill_line_spacing(const PrintObject &print_object);

// 旋转八叉树使其一角着地。
Eigen::Quaterniond              transform_to_world();
// 上述的逆旋转。
Eigen::Quaterniond              transform_to_octree();

FillAdaptive::OctreePtr         build_octree(
    // 网格已旋转到八叉树的坐标系。
    const indexed_triangle_set  &triangle_mesh,
    // 从填充表面提取的具有 stInternalBridge 类型的悬垂三角形，
    // 已旋转到八叉树的坐标系。
    const std::vector<Vec3d>    &overhang_triangles,
    coordf_t                     line_spacing,
    // 如果为 true，则仅在内部悬垂下方增加八叉树密度。
    bool                         support_overhangs_only);

//
// FillAdaptive 类使用的某些算法受到
// Cura Engine 的 SubDivCube 类的启发
// https://github.com/Ultimaker/CuraEngine/blob/master/src/infill/SubDivCube.h
//
class Filler : public Slic3r::Fill
{
public:
    ~Filler() override {}

protected:
    Fill* clone() const override { return new Filler(*this); }
	void _fill_surface_single(
	    const FillParams                &params,
	    unsigned int                     thickness_layers,
	    const std::pair<float, Point>   &direction,
	    ExPolygon                        expolygon,
	    Polylines                       &polylines_out) override;
    // 让 G 代码导出器重新排序填充线。
    //FIXME 让 G 代码导出器重新排序自适应立方体填充的填充线
    // 可能不是最优的，因为内部填充线可能会在短填充线
    // 应该锚定的长填充线之前被挤出。
	bool no_sort() const override { return false; }
    bool is_self_crossing() override { return true; }
};

} // namespace FillAdaptive
} // namespace Slic3r

#endif // slic3r_FillAdaptive_hpp_
