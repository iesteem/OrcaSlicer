// Tree supports by Thomas Rahm, losely based on Tree Supports by CuraEngine. Thomas Rahm开发的树状支撑，大致基于CuraEngine的树状支撑。
// Original source of Thomas Rahm's tree supports: Thomas Rahm树状支撑的原始来源：
// https://github.com/ThomasRahm/CuraEngine
//
// Original CuraEngine copyright: 原始CuraEngine版权：
// Copyright (c) 2021 Ultimaker B.V.
// CuraEngine is released under the terms of the AGPLv3 or higher. CuraEngine根据AGPLv3或更高版本的条款发布。

#ifndef slic3r_TreeSupport_hpp
#define slic3r_TreeSupport_hpp

#include "SupportLayer.hpp"
#include "TreeModelVolumes.hpp"
#include "TreeSupportCommon.hpp"

#include "../BoundingBox.hpp"
#include "../Point.hpp"
#include "../Utils.hpp"

#include <boost/container/small_vector.hpp>


// #define TREE_SUPPORT_SHOW_ERRORS

#ifdef SLIC3R_TREESUPPORTS_PROGRESS
    // The various stages of the process can be weighted differently in the progress bar. 流程的各个阶段在进度条中可以有不同的权重。
    // These weights are obtained experimentally using a small sample size. Sensible weights can differ drastically based on the assumed default settings and model. 这些权重通过小样本实验获得。合理的权重可能因假定的默认设置和模型而有很大差异。
    #define TREE_PROGRESS_TOTAL 10000
    #define TREE_PROGRESS_PRECALC_COLL TREE_PROGRESS_TOTAL * 0.1
    #define TREE_PROGRESS_PRECALC_AVO TREE_PROGRESS_TOTAL * 0.4
    #define TREE_PROGRESS_GENERATE_NODES TREE_PROGRESS_TOTAL * 0.1
    #define TREE_PROGRESS_AREA_CALC TREE_PROGRESS_TOTAL * 0.3
    #define TREE_PROGRESS_DRAW_AREAS TREE_PROGRESS_TOTAL * 0.1
    #define TREE_PROGRESS_GENERATE_BRANCH_AREAS TREE_PROGRESS_DRAW_AREAS / 3
    #define TREE_PROGRESS_SMOOTH_BRANCH_AREAS TREE_PROGRESS_DRAW_AREAS / 3
    #define TREE_PROGRESS_FINALIZE_BRANCH_AREAS TREE_PROGRESS_DRAW_AREAS / 3
#endif // SLIC3R_TREESUPPORTS_PROGRESS

namespace Slic3r
{

// Forward declarations 前向声明
class Print;
class PrintObject;
struct SlicingParameters;

namespace TreeSupport3D
{


struct AreaIncreaseSettings
{
    AreaIncreaseSettings(
        TreeModelVolumes::AvoidanceType type = TreeModelVolumes::AvoidanceType::Fast, coord_t increase_speed = 0, 
        bool increase_radius = false, bool no_error = false, bool use_min_distance = false, bool move = false) :
        increase_speed{ increase_speed }, type{ type }, increase_radius{ increase_radius }, no_error{ no_error }, use_min_distance{ use_min_distance }, move{ move } {}

    coord_t         increase_speed;
    // Packing for smaller memory footprint of SupportElementState && SupportElementMerging 打包以减少SupportElementState和SupportElementMerging的内存占用
    TreeModelVolumes::AvoidanceType type;
    bool            increase_radius  : 1;
    bool            no_error         : 1;
    bool            use_min_distance : 1;
    bool            move             : 1;
    bool operator==(const AreaIncreaseSettings& other) const
    {
        return type             == other.type               &&
               increase_speed   == other.increase_speed     &&
               increase_radius  == other.increase_radius    &&
               no_error         == other.no_error           &&
               use_min_distance == other.use_min_distance   &&
               move             == other.move;
    }
};

#define TREE_SUPPORTS_TRACK_LOST

// C++17 does not support in place initializers of bit values, thus a constructor zeroing the bits is provided. C++17不支持位域成员的就地初始化，因此提供了一个将位清零的构造函数。
struct SupportElementStateBits {
    SupportElementStateBits() :
        to_buildplate(false),
        to_model_gracious(false),
        use_min_xy_dist(false),
        supports_roof(false),
        can_use_safe_radius(false),
        skip_ovalisation(false),
#ifdef TREE_SUPPORTS_TRACK_LOST
        lost(false),
        verylost(false),
#endif // TREE_SUPPORTS_TRACK_LOST
        deleted(false),
        marked(false)
        {}

    /*!
     * \brief The element trys to reach the buildplate 该元素尝试到达构建板
     */
    bool to_buildplate : 1;

    /*!
     * \brief Will the branch be able to rest completely on a flat surface, be it buildplate or model ? 分支是否能够完全停留在平坦表面上，无论是构建板还是模型？
     */
    bool to_model_gracious : 1;

    /*!
     * \brief Whether the min_xy_distance can be used to get avoidance or similar. Will only be true if support_xy_overrides_z=Z overrides X/Y. min_xy_distance是否可以用于获取避让等。仅在support_xy_overrides_z=Z覆盖X/Y时为真。
     */
    bool use_min_xy_dist : 1;

    /*!
     * \brief True if this Element or any parent (element above) provides support to a support roof. 如果此元素或任何父元素（上方的元素）为支撑顶面提供支撑，则为真。
     */
    bool supports_roof : 1;

    /*!
     * \brief An influence area is considered safe when it can use the holefree avoidance <=> It will not have to encounter holes on its way downward. 当影响区域可以使用无孔避让时，它被认为是安全的 <=> 它在向下扩展过程中不会遇到孔洞。
     */
    bool can_use_safe_radius : 1;

    /*!
     * \brief Skip the ovalisation to parent and children when generating the final circles. 在生成最终圆形时跳过对父级和子级的椭圆化。
     */
    bool skip_ovalisation : 1;

#ifdef TREE_SUPPORTS_TRACK_LOST
    // Likely a lost branch, debugging information. 可能是丢失的分支，调试信息。
    bool lost : 1;
    bool verylost : 1;
#endif // TREE_SUPPORTS_TRACK_LOST

    // Not valid anymore, to be deleted. 不再有效，待删除。
    bool deleted : 1;

    // General purpose flag marking a visited element. 用于标记已访问元素的通用标志。
    bool marked : 1;
};

struct SupportElementState : public SupportElementStateBits
{
    int type = 0;
    coordf_t radius = 0;
    float print_z = 0;

    /*!
     * \brief The layer this support elements wants reach 此支撑元素想要到达的层
     */
    LayerIndex  target_height;

    /*!
     * \brief The position this support elements wants to support on layer=target_height 此支撑元素想要在layer=target_height层上支撑的位置
     */
    Point       target_position;

    /*!
     * \brief The next position this support elements wants to reach. NOTE: This is mainly a suggestion regarding direction inside the influence area. 此支撑元素想要到达的下一个位置。注意：这主要是关于影响区域内方向的建议。
     */
    Point       next_position;

    /*!
     * \brief The next height this support elements wants to reach 此支撑元素想要到达的下一个高度
     */
    LayerIndex  layer_idx;

    /*!
     * \brief The Effective distance to top of this element regarding radius increases and collision calculations. 此元素关于半径增加和碰撞计算的有效到顶部距离。
     */
    uint32_t    effective_radius_height;

    /*!
     * \brief The amount of layers this element is below the topmost layer of this branch. 此元素在该分支最顶层下方的层数。
     */
    uint32_t    distance_to_top;

    /*!
     * \brief The resulting center point around which a circle will be drawn later. 最终的中心点，稍后将围绕此点绘制圆形。
     * Will be set by setPointsOnAreas 将由setPointsOnAreas设置
     */
    Point result_on_layer { std::numeric_limits<coord_t>::max(), std::numeric_limits<coord_t>::max() };
    bool  result_on_layer_is_set() const { return this->result_on_layer != Point{ std::numeric_limits<coord_t>::max(), std::numeric_limits<coord_t>::max() }; }
    void  result_on_layer_reset() { this->result_on_layer = Point{ std::numeric_limits<coord_t>::max(), std::numeric_limits<coord_t>::max() }; }
    /*!
     * \brief The amount of extra radius we got from merging branches that could have reached the buildplate, but merged with ones that can not. 从合并分支中获得的额外半径量，这些分支本可以到达构建板，但与无法到达的分支合并了。
     */
    coord_t     increased_to_model_radius; // 向模型增加的半径量，仅与合并相关

    /*!
     * \brief Counter about the times the elephant foot was increased. Can be fractions for merge reasons. 大象脚增加的计数。由于合并原因可以是分数。
     */
    double      elephant_foot_increases;

    /*!
     * \brief The element tries to not move until this dtt is reached, is set to 0 if the element had to move. 元素尝试在达到此dtt之前不移动，如果元素不得不移动，则设置为0。
     */
    uint32_t    dont_move_until;

    /*!
     * \brief Settings used to increase the influence area to its current state. 用于将影响区域增加到其当前状态的设置。
     */
    AreaIncreaseSettings last_area_increase;

    /*!
     * \brief Amount of roof layers that were not yet added, because the branch needed to move. 尚未添加的顶面层数，因为分支需要移动。
     */
    uint32_t    missing_roof_layers;

    // called by increase_single_area() and increaseAreas()
    [[nodiscard]] static SupportElementState propagate_down(const SupportElementState &src)
    {
        SupportElementState dst{ src };
        ++ dst.distance_to_top;
        -- dst.layer_idx;
        // set to invalid as we are a new node on a new layer 设为无效，因为我们是新层上的新节点
        dst.result_on_layer_reset();
        dst.skip_ovalisation = false;
        return dst;
    }

    [[nodiscard]] bool locked() const { return this->distance_to_top < this->dont_move_until; }
};

/*!
 * \brief Get the Distance to top regarding the real radius this part will have. This is different from distance_to_top, which is can be used to calculate the top most layer of the branch. 获取关于此部分实际半径的到顶部距离。与distance_to_top不同，后者可用于计算分支的最顶层。
 * \param elem[in] The SupportElement one wants to know the effectiveDTT 想要知道有效DTT的支撑元素
 * \return The Effective DTT. 有效DTT。
 */
[[nodiscard]] inline size_t getEffectiveDTT(const TreeSupportSettings &settings, const SupportElementState &elem)
{
    return elem.effective_radius_height < settings.increase_radius_until_layer ? 
        (elem.distance_to_top < settings.increase_radius_until_layer ? elem.distance_to_top : settings.increase_radius_until_layer) : 
        elem.effective_radius_height;
}

/*!
 * \brief Get the Radius, that this element will have. 获取此元素将具有的半径。
 * \param elem[in] The Element. 元素。
 * \return The radius the element has. 元素拥有的半径。
 */
[[nodiscard]] inline coord_t support_element_radius(const TreeSupportSettings &settings, const SupportElementState &elem)
{ 
    return settings.getRadius(getEffectiveDTT(settings, elem), elem.elephant_foot_increases);
}

/*!
 * \brief Get the collision Radius of this Element. This can be smaller then the actual radius, as the drawAreas will cut off areas that may collide with the model. 获取此元素的碰撞半径。这可能小于实际半径，因为drawAreas会剪掉可能与模型碰撞的区域。
 * \param elem[in] The Element. 元素。
 * \return The collision radius the element has. 元素拥有的碰撞半径。
 */
[[nodiscard]] inline coord_t support_element_collision_radius(const TreeSupportSettings &settings, const SupportElementState &elem)
{
    return settings.getRadius(elem.effective_radius_height, elem.elephant_foot_increases);
}

struct SupportElement
{
    using ParentIndices =
#ifdef NDEBUG
        // To reduce memory allocation in release mode. 在发布模式下减少内存分配。
        boost::container::small_vector<int32_t, 4>;
#else // NDEBUG
        // To ease debugging. 为便于调试。
        std::vector<int32_t>;
#endif // NDEBUG

//    SupportElement(const SupportElementState &state) : SupportElementState(state) {}
    SupportElement(const SupportElementState &state, Polygons &&influence_area) : state(state), influence_area(std::move(influence_area)) {}
    SupportElement(const SupportElementState &state, ParentIndices &&parents, Polygons &&influence_area) :
        state(state), parents(std::move(parents)), influence_area(std::move(influence_area)) {}

    SupportElementState         state;

    /*!
     * \brief All elements in the layer above the current one that are supported by this element 当前层上方由此元素支撑的所有元素
     */
    ParentIndices               parents;

    /*!
     * \brief The resulting influence area. 最终的影响区域。
     * Will only be set in the results of createLayerPathing, and will be nullptr inside! 仅在createLayerPathing的结果中设置，内部将为nullptr！
     */
    Polygons                    influence_area;
};

using SupportElements = std::deque<SupportElement>;

[[nodiscard]] inline coord_t support_element_radius(const TreeSupportSettings &settings, const SupportElement &elem)
{
    return support_element_radius(settings, elem.state);
}

[[nodiscard]] inline coord_t support_element_collision_radius(const TreeSupportSettings &settings, const SupportElement &elem)
{
    return support_element_collision_radius(settings, elem.state);
}

// Organic specific: Smooth branches and produce one cummulative mesh to be sliced. 有机支撑特有：平滑分支并生成一个累积网格以供切片。
void organic_draw_branches(
    PrintObject                     &print_object,
    TreeModelVolumes                &volumes, 
    const TreeSupportSettings       &config,
    std::vector<SupportElements>    &move_bounds,

    // I/O: 输入/输出：
    SupportGeneratorLayersPtr       &bottom_contacts,
    SupportGeneratorLayersPtr       &top_contacts,
    InterfacePlacer                 &interface_placer,

    // Output: 输出：
    SupportGeneratorLayersPtr       &intermediate_layers,
    SupportGeneratorLayerStorage    &layer_storage,

    std::function<void()> throw_on_cancel);

} // namespace TreeSupport3D

void generate_tree_support_3D(PrintObject &print_object, TreeSupport* tree_support, std::function<void()> throw_on_cancel = []{});

} // namespace Slic3r

#endif /* slic3r_TreeSupport_hpp */
