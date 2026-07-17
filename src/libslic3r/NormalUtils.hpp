#ifndef slic3r_NormalUtils_hpp_
#define slic3r_NormalUtils_hpp_

#include "Point.hpp"
#include "Model.hpp"

namespace Slic3r {

/// <summary>
/// 静态函数集合，用于创建法线
/// </summary>
class NormalUtils
{
public:
    using Normal = Vec3f;
    using Normals = std::vector<Normal>;
    NormalUtils() = delete; // only static functions

    enum class VertexNormalType {
        AverageNeighbor,
        AngleWeighted,
        NelsonMaxWeighted
    };

    /// <summary>
    /// 为顶点索引定义的三角形创建法线
    /// </summary>
    /// <param name="indices">顶点索引</param>
    /// <param name="vertices">顶点向量</param>
    /// <returns>三角形法线（归一化到大小为 1）</returns>
    static Normal create_triangle_normal(
        const stl_triangle_vertex_indices &indices,
        const std::vector<stl_vertex> &    vertices);

    /// <summary>
    /// 为每个顶点创建法线
    /// </summary>
    /// <param name="its">索引和顶点</param>
    /// <returns>法线向量</returns>
    static Normals create_triangle_normals(const indexed_triangle_set &its);

    /// <summary>
    /// 通过对相邻三角形法线求平均为每个顶点创建法线
    /// </summary>
    /// <param name="its">三角形索引和顶点</param>
    /// <param name="type">法线计算类型</param>
    /// <returns>每个顶点的法线</returns>
    static Normals create_normals(
        const indexed_triangle_set &its,
        VertexNormalType type = VertexNormalType::NelsonMaxWeighted);
    static Normals create_normals_average_neighbor(const indexed_triangle_set &its);
    static Normals create_normals_angle_weighted(const indexed_triangle_set &its);
    static Normals create_normals_nelson_weighted(const indexed_triangle_set &its);

    /// <summary>
    /// 计算三角形边的角度。
    /// </summary>
    /// <param name="i">索引索引，定义角度点</param>
    /// <param name="indice">顶点地址</param>
    /// <param name="vertices">顶点数据</param>
    /// <returns>角度 [弧度]</returns>
    static float indice_angle(int                            i,
                              const Vec3i32 &                indice,
                              const std::vector<stl_vertex> &vertices);
};

} // namespace Slic3r
#endif // slic3r_NormalUtils_hpp_
