// 论文: https://people.eecs.berkeley.edu/~jrs/meshpapers/GarlandHeckbert2.pdf
// 总结: https://users.csc.calpoly.edu/~zwood/teaching/csc570/final06/jseeba/
// 灵感来源: https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification

#include <cstdint>
#include <functional>
#include "TriangleMesh.hpp"

namespace Slic3r {

/// <summary>
/// 通过二次度量简化网格
/// </summary>
/// <param name="its">输入/输出 要简化的三角网格。</param>
/// <param name="triangle_count">期望的三角形数量。</param>
/// <param name="max_error">简化的最大二次度量。
/// 当为nullptr时使用最大浮点数
/// 输出: 上次用于折叠边的ErrorValue</param>
/// <param name="throw_on_cancel">可停止计算过程。</param>
/// <param name="statusfn">向用户反馈进度。值 1 - 100</param>
void its_quadric_edge_collapse(
    indexed_triangle_set &    its,
    uint32_t                  triangle_count  = 0,
    float *                   max_error       = nullptr,
    std::function<void(void)> throw_on_cancel = nullptr,
    std::function<void(int)>  statusfn        = nullptr);

} // namespace Slic3r
