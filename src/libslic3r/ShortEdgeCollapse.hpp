#ifndef SRC_LIBSLIC3R_SHORTEDGECOLLAPSE_HPP_
#define SRC_LIBSLIC3R_SHORTEDGECOLLAPSE_HPP_

#include "libslic3r/TriangleMesh.hpp"

namespace Slic3r{

// 通过折叠短边来精简模型。从非常小的边开始，逐渐增加可折叠长度，
// 直到达到目标三角形数量（算法肯定达不到目标数量，结果三角形的数量将少于目标数量）
// 该算法不检查三角形翻转、断开连接、自交或在网格处理过程中可能出现的任何其他退化情况。
void its_short_edge_collpase(indexed_triangle_set &mesh, size_t target_triangle_count);

}


#endif /* SRC_LIBSLIC3R_SHORTEDGECOLLAPSE_HPP_ */
