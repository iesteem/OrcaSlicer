#ifndef MINIMUMSPANNINGTREE_H
#define MINIMUMSPANNINGTREE_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Point.hpp"

namespace Slic3r
{

/*!
 * \brief 实现 Prim 算法以计算最小生成树（MST）。
 *
 * 最小生成树始终从顶点的团计算得出。
 */
class MinimumSpanningTree
{
    /*!
     * \brief 表示树的一条边。
     *
     * 虽然边应该是无向的，但它们确实有起点和终点。
     */
    struct Edge {
        /**
         * The point at which this edge starts.
         */
        const Point start;

        /**
         * The point at which this edge ends.
         */
        const Point end;
    };
public:
    MinimumSpanningTree() = default;
    /*!
     * \brief 构造覆盖所有给定顶点的最小生成树。
     */
    MinimumSpanningTree(std::vector<Point> vertices);

    /*!
     * \brief 获取与指定节点相邻的节点。
     * \return 相邻节点的列表。
     */
    std::vector<Point> adjacent_nodes(Point node) const;

    /*!
     * \brief 获取树的叶子节点。
     * \return 树的所有叶子节点列表。
     */
    std::vector<Point> leaves() const;

    /*!
     * \brief 获取树的所有顶点。
     * \return 树的顶点列表。
     */
    std::vector<Point> vertices() const;

private:
    using AdjacencyGraph_t = std::unordered_map<Point, std::vector<Edge>, PointHash>;
    AdjacencyGraph_t adjacency_graph;

    /*!
     * \brief 使用 Prim 算法计算最小生成树的边。
     *
     * \param vertices 要覆盖的顶点。
     * \return 每个点对应一条或多条边的邻接图。
     */
    AdjacencyGraph_t prim(std::vector<Point> vertices) const;
};

}

#endif /* MINIMUMSPANNINGTREE_H */

