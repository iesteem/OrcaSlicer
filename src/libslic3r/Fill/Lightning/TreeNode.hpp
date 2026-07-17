//Copyright (c) 2021 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef LIGHTNING_TREE_NODE_H
#define LIGHTNING_TREE_NODE_H

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../../EdgeGrid.hpp"
#include "../../Polygon.hpp"
#include "SVG.hpp"

//#define LIGHTNING_TREE_NODE_DEBUG_OUTPUT

namespace Slic3r::FillLightning
{

inline coord_t locator_cell_size() { return scaled<coord_t>(4.); }

class Node;

using NodeSPtr = std::shared_ptr<Node>;

// 注意：如所写，此结构仅对单层有效，必须为下一层更新。
// 注意：使用一些单独的闭包来实现此结构的原因：
//       - 在开发过程中保持清晰的分界线
//       - 可能实现多种距离场策略

/*!
 * 闪电树的单个顶点，决定要打印的路径以形成闪电填充的结构。
 *
 * 本质上这些顶点只是与 2D 中其他位置链接的位置。
 * 节点具有父母和孩子的层次结构，形成一棵树。
 * 该类还有一些特定于闪电填充的辅助函数，例如拉直围绕此节点的路径。
 */
class Node : public std::enable_shared_from_this<Node>
{
public:
    // 私有/受保护构造函数和 'make_shared' 的解决方法：https://stackoverflow.com/a/27832765
    template<typename ...Arg> NodeSPtr static create(Arg&&...arg)
    {
        struct EnableMakeShared : public Node
        {
            explicit EnableMakeShared(Arg&&...arg) : Node(std::forward<Arg>(arg)...) {}
        };
        return std::make_shared<EnableMakeShared>(std::forward<Arg>(arg)...);
    }

    /*!
     * 获取此节点在此层上表示的位置，即要打印路径的顶点。
     * \return 此节点表示的位置。
     */
    const Point& getLocation() const { return m_p; }

    /*!
     * 更改此节点在此层上表示的位置。
     * \param p 节点需要表示的位置。
     */
    void setLocation(const Point& p) { m_p = p; }

    /*!
     * 构造一个新的 ``Node`` 实例并将其添加为此节点的子节点。
     * \param p 新节点的位置。
     * \return 指向新节点的共享指针。
     */
    NodeSPtr addChild(const Point& p);

    /*!
     * 将现有的 ``Node`` 添加为此节点的子节点。
     * \param new_child 必须添加为子节点的节点。
     * \return 始终返回 \p new_child。
     */
    NodeSPtr addChild(NodeSPtr& new_child);

    /*!
     * 将此节点的子树传播到下一层。
     *
     * 创建此树的副本，将其重新对齐到新层边界 \p next_outlines 并进行缩减（即修剪和拉直）。
     * 此节点及其所有后代节点的副本将添加到 \p next_trees 向量中。
     * \param next_trees 用于下一层的树节点集合。
     * \param next_outlines 下层轮廓，确保树保持在填充区域边界内。
     * \param prune_distance 叶节点可以移动的最大距离，同时仍能支撑当前节点。
     * \param smooth_magnitude 线可以移动以拉直树路径的最大距离，同时仍能支撑当前路径。
     * \param max_remove_colinear_dist 拉直可以移除共线点的线段的最大距离。
     */
    void propagateToNextLayer
    (
        std::vector<NodeSPtr>& next_trees,
        const Polygons& next_outlines,
        const EdgeGrid::Grid& outline_locator,
        coord_t prune_distance,
        coord_t smooth_magnitude,
        coord_t max_remove_colinear_dist
    ) const;

    /*!
     * 对此节点子树中的每个线段执行给定函数。
     *
     * 该函数接受两个 `Point` 参数。这些参数将首先填入高阶节点（更靠近根），
     * 然后填入树下节点（更靠近叶）作为第二个参数。从此节点的父节点到此节点本身的线段不包括在内。
     * 访问线段的顺序是深度优先。
     * \param visitor 要为节点子树中每个分支执行的函数。
     */
    void visitBranches(const std::function<void(const Point&, const Point&)>& visitor) const;

    /*!
     * 对此节点子树中的每个节点执行给定函数。
     *
     * 访问者函数以节点作为输入。此节点不是 const，因此可用于更改树。
     * 节点按深度优先顺序访问。此节点本身也按前序遍历访问。
     * \param visitor 要为节点子树中每个节点执行的函数。
     */
    void visitNodes(const std::function<void(NodeSPtr)>& visitor);

    /*!
     * 从未支撑点到该节点获取加权距离（给定当前支撑半径）。
     *
     * 将未支撑位置附加到节点时，并非所有节点都具有相同的优先级。
     * （欧几里得）较近的节点优先，但这还不是全部。
     * 例如，我们根据分支数量给某些节点"价电子提升"。
     * \param unsupported_location 需要计算加权距离的未支撑位置。
     * \param supporting_radius 可以在没有（填充）支撑的情况下桥接的最大距离。
     * \return 加权距离。
     */
    coord_t getWeightedDistance(const Point& unsupported_location, const coord_t& supporting_radius) const;

    /*!
     * 返回此节点是否为闪电树的根。如果没有父节点，则为根。
     * \return 如果此节点是根（无父节点）则返回 ``true``，如果是其他节点的子节点则返回 ``false``。
     */
    bool isRoot() const { return m_is_root; }

    /*!
     * 从此节点开始，反转一直到根的父子关系。
     * 如果未给定直接父节点作为参数，则效果是在当前节点处"重新生根"树。
     * 即当前节点将成为根，其（以前的）父节点（如果有）将成为其子节点之一。
     * 然后递归向上冒泡，直到到达（以前的）根，它将成为叶节点。
     * \param new_parent 根的（新）父节点，用于递归或立即将节点附加到另一棵树。
     */
    void reroot(const NodeSPtr &new_parent = nullptr);

    /*!
     * 检索距离指定位置最近的节点。
     * \param loc 指定的位置。
     * \result 从此树内最靠近该位置的位置开始的分支。
     */
    NodeSPtr closestNode(const Point& loc);

    /*!
     * 返回给定的树节点是否为此节点的后代。
     *
     * 如果给定的是此节点本身，也视为后代。
     * \param to_be_checked 要检查是否为此节点后代的节点。
     * \return 如果给定节点是后代或此节点本身则返回 ``true``，如果不在子树中则返回 ``false``。
     */
    bool hasOffspring(const NodeSPtr& to_be_checked) const;

    Node() = delete; // Don't allow empty contruction

protected:
    /*!
     * 构造一个新节点，用于插入树中或作为根。
     * \param p 此节点在 2D 层中的物理位置。将其他节点连接到此节点表示应在这两个物理位置之间绘制线段。
     */
    explicit Node(const Point& p, const std::optional<Point>& last_grounding_location = std::nullopt);

    /*!
     * 复制此节点及其整个子树。
     * \return 副本中与此节点等效的节点（新子树的根）。
     */
    NodeSPtr deepCopy() const;

    /*! 将上层的树重新连接到下层的新轮廓。
     * \return 是否保留根（false 为否，true 为是）。
     */
    bool realign(const Polygons& outlines, const EdgeGrid::Grid& outline_locator, std::vector<NodeSPtr>& rerooted_parts);

    struct RectilinearJunction
    {
        coord_t total_recti_dist; //!< rectilinear distance along the tree from the last junction above to the junction below
        Point junction_loc; //!< junction location below
    };

    /*!
     * 平滑树使其更易于打印，同时仍支撑上方的树。
     * \param magnitude 移动节点的最大允许距离。
     * \param max_remove_colinear_dist 可以从中移除共线点的（复合）线段的最大距离。
     */
    void straighten(coord_t magnitude, coord_t max_remove_colinear_dist);

    /*! \ref straighten(.) 的递归部分
     * \param junction_above 最后看到的具有多个子节点的上方连接点
     * \param accumulated_dist 沿树从最后一个连接点到该节点的距离
     * \param max_remove_colinear_dist2 可以从中移除共线点的（复合）线段的最大距离_平方_
     * \return 沿树从上一个上方连接点到第一个下方连接点的总距离以及下一个下方连接点的位置
     */
    RectilinearJunction straighten(coord_t magnitude, const Point& junction_above, coord_t accumulated_dist, int64_t max_remove_colinear_dist2);

    /*! 从末端（叶节点）修剪树，直到达到修剪距离。
     * \return 已修剪的距离。如果小于 \p distance，则整棵树已被修剪掉。
     */
    coord_t prune(const coord_t& distance);

public:
    /*!
     * 将树转换为多段线
     *
     * 在每个连接点随机选择一条线继续
     *
     * 线从叶节点开始，在连接点结束
     *
     * \param output 此树中所有连接成多段线的分支
     */
    void convertToPolylines(Polylines &output, coord_t line_overlap) const;

    /*! 如果这曾经是根的直接子节点，它将有一个先前的接地位置。
     *
     * 重新连接根时需要知道这一点，以便下一层支撑上一层。
     */
    const std::optional<Point>& getLastGroundingLocation() const { return m_last_grounding_location; }

    void draw_tree(SVG& svg) { for (auto& child : m_children) { svg.draw(Line(m_p, child->getLocation()), "yellow"); child->draw_tree(svg); } }

protected:
    /*!
     * 将树转换为多段线
     *
     * 在每个连接点随机选择一条线继续
     *
     * 线从叶节点开始，在连接点结束
     *
     * \param long_line 对 \p output 中多段线的引用，在递归中继续构建该多段线
     * \param output 此树中所有连接成多段线的分支
     */
    void convertToPolylines(size_t long_line_idx, Polylines &output) const;

    void removeJunctionOverlap(Polylines &polylines, coord_t line_overlap) const;

    bool m_is_root;
    Point m_p;
    std::weak_ptr<Node> m_parent;
    std::vector<NodeSPtr> m_children;

    std::optional<Point> m_last_grounding_location;  //<! The last known grounding location, see 'getLastGroundingLocation()'.

    friend BoundingBox get_extents(const NodeSPtr &root_node);
    friend BoundingBox get_extents(const std::vector<NodeSPtr> &tree_roots);

#ifdef LIGHTNING_TREE_NODE_DEBUG_OUTPUT
    friend void export_to_svg(const NodeSPtr &root_node, Slic3r::SVG &svg);
    friend void export_to_svg(const std::string &path, const Polygons &contour, const std::vector<NodeSPtr> &root_nodes);
#endif /* LIGHTNING_TREE_NODE_DEBUG_OUTPUT */
};

bool inside(const Polygons &polygons, const Point &p);
bool lineSegmentPolygonsIntersection(const Point& a, const Point& b, const EdgeGrid::Grid& outline_locator, Point& result, coord_t within_max_dist);

inline BoundingBox get_extents(const NodeSPtr &root_node)
{
    BoundingBox bbox;
    for (const NodeSPtr &children : root_node->m_children)
        bbox.merge(get_extents(children));
    bbox.merge(root_node->getLocation());
    return bbox;
}

inline BoundingBox get_extents(const std::vector<NodeSPtr> &tree_roots)
{
    BoundingBox bbox;
    for (const NodeSPtr &root_node : tree_roots)
        bbox.merge(get_extents(root_node));
    return bbox;
}

#ifdef LIGHTNING_TREE_NODE_DEBUG_OUTPUT
void export_to_svg(const NodeSPtr &root_node, SVG &svg);
void export_to_svg(const std::string &path, const Polygons &contour, const std::vector<NodeSPtr> &root_nodes);
#endif /* LIGHTNING_TREE_NODE_DEBUG_OUTPUT */

} // namespace Slic3r::FillLightning

#endif // LIGHTNING_TREE_NODE_H
