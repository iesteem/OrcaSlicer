// Tree supports by Thomas Rahm, losely based on Tree Supports by CuraEngine. Thomas Rahm开发的树状支撑，大致基于CuraEngine的树状支撑。
// Original source of Thomas Rahm's tree supports: Thomas Rahm树状支撑的原始来源：
// https://github.com/ThomasRahm/CuraEngine
//
// Original CuraEngine copyright: 原始CuraEngine版权：
// Copyright (c) 2021 Ultimaker B.V.
// CuraEngine is released under the terms of the AGPLv3 or higher. CuraEngine根据AGPLv3或更高版本的条款发布。

#ifndef slic3r_TreeModelVolumes_hpp
#define slic3r_TreeModelVolumes_hpp

#include <mutex>
#include <unordered_map>

#include <boost/functional/hash.hpp>

#include "TreeSupportCommon.hpp"

#include "../Point.hpp"
#include "../Polygon.hpp"
#include "../PrintConfig.hpp"

namespace Slic3r
{

class BuildVolume;
class PrintObject;

namespace TreeSupport3D
{

static constexpr const double  SUPPORT_TREE_EXPONENTIAL_FACTOR = 1.5;
#define SUPPORT_TREE_EXPONENTIAL_THRESHOLD  scaled<coord_t>(1. * SUPPORT_TREE_EXPONENTIAL_FACTOR)
#define SUPPORT_TREE_COLLISION_RESOLUTION  scaled<coord_t>(0.5)
static constexpr const bool    SUPPORT_TREE_AVOID_SUPPORT_BLOCKER = true;

class TreeModelVolumes
{
public:
    TreeModelVolumes() = default;
    explicit TreeModelVolumes(const PrintObject &print_object, const BuildVolume &build_volume,
        coord_t max_move, coord_t max_move_slow, size_t current_mesh_idx, 
#ifdef SLIC3R_TREESUPPORTS_PROGRESS
        double progress_multiplier, 
        double progress_offset, 
#endif // SLIC3R_TREESUPPORTS_PROGRESS
        const std::vector<Polygons> &additional_excluded_areas = {});
    TreeModelVolumes(TreeModelVolumes&&) = default;
    TreeModelVolumes& operator=(TreeModelVolumes&&) = default;

    TreeModelVolumes(const TreeModelVolumes&) = delete;
    TreeModelVolumes& operator=(const TreeModelVolumes&) = delete;

    void clear() { 
        this->clear_all_but_object_collision();
        m_collision_cache.clear();
        m_placeable_areas_cache.clear();
    }
    void clear_all_but_object_collision() { 
        //m_collision_cache.clear_all_but_radius0();
        m_collision_cache_holefree.clear();
        m_avoidance_cache.clear();
        m_avoidance_cache_slow.clear();
        m_avoidance_cache_to_model.clear();
        m_avoidance_cache_to_model_slow.clear();
        m_placeable_areas_cache.clear_all_but_radius0();
        m_avoidance_cache_holefree.clear();
        m_avoidance_cache_holefree_to_model.clear();
        m_wall_restrictions_cache.clear();
        m_wall_restrictions_cache_min.clear();
    }

    enum class AvoidanceType : int8_t
    {
        Slow,
        FastSafe,
        Fast,
        Count
    };

    /*!
     * \brief Precalculate avoidances and collisions up to max_layer. 预计算到max_layer的避让和碰撞。
     *
     * Knowledge about branch angle is used to only calculate avoidances and collisions that may actually be needed. 使用分支角度的知识，仅计算实际需要的避让和碰撞。
     * Not calling precalculate() will cause the class to lazily calculate avoidances and collisions as needed, which will be a lot slower on systems with more then one or two cores! 不调用precalculate()将导致类按需惰性计算避让和碰撞，这在多核系统上会慢得多！
     */
    void precalculate(const PrintObject& print_object, const coord_t max_layer, std::function<void()> throw_on_cancel);

    /*!
     * \brief Provides the areas that have to be avoided by the tree's branches to prevent collision with the model on this layer. 提供树状分支必须避让的区域，以防止在此层上与模型碰撞。
     *
     * The result is a 2D area that would cause nodes of radius \p radius to collide with the model. 结果是一个2D区域，半径为\p radius的节点与该区域相交会导致与模型碰撞。
     *
     * \param radius The radius of the node of interest 关注节点的半径
     * \param layer_idx The layer of interest 关注的层
     * \param min_xy_dist Is the minimum xy distance used. 是否使用最小XY距离。
     * \return Polygons object 多边形对象
     */
    const Polygons& getCollision(const coord_t radius, LayerIndex layer_idx, bool min_xy_dist) const;

    // Get a collision area at a given layer for a radius that is a lower or equial to the key radius. 获取指定层上半径小于或等于关键半径的碰撞区域。
    // It is expected that the collision area is precalculated for a given layer at least for the radius zero. 期望指定层的碰撞区域至少已为半径零预计算。
    // Used for pushing tree supports away from object during the final Organic optimization step. 用于在最终的有机优化步骤中将树状支撑推离物体。
    std::optional<std::pair<coord_t, std::reference_wrapper<const Polygons>>> get_collision_lower_bound_area(LayerIndex layer_id, coord_t max_radius) const;

    /*!
     * \brief Provides the areas that have to be avoided by the tree's branches in order to reach the build plate. 提供树状分支为了到达构建板必须避让的区域。
     *
     * The result is a 2D area that would cause nodes of radius \p radius to collide with the model or be unable to reach the build platform. 结果是一个2D区域，半径为\p radius的节点与该区域相交会导致与模型碰撞或无法到达构建平台。
     *
     * The input collision areas are inset by the maximum move distance and propagated upwards. 输入的碰撞区域按最大移动距离收缩并向上传播。
     *
     * \param radius The radius of the node of interest 关注节点的半径
     * \param layer_idx The layer of interest 关注的层
     * \param type Is the propagation with the maximum move distance slow required. 是否需要使用最大移动距离进行慢速传播。
     * \param to_model Does the avoidance allow good connections with the model. 避让是否允许与模型良好连接。
     * \param min_xy_dist is the minimum xy distance used. 是否使用最小XY距离。
     * \return Polygons object 多边形对象
     */
    const Polygons& getAvoidance(coord_t radius, LayerIndex layer_idx, AvoidanceType type, bool to_model, bool min_xy_dist) const;
    /*!
     * \brief Provides the area represents all areas on the model where the branch does completely fit on the given layer. 提供模型上分支在给定层完全适合的所有区域。
     * \param radius The radius of the node of interest 关注节点的半径
     * \param layer_idx The layer of interest 关注的层
     * \return Polygons object 多边形对象
     */
    const Polygons& getPlaceableAreas(coord_t radius, LayerIndex layer_idx, std::function<void()> throw_on_cancel) const;
    /*!
     * \brief Provides the area that represents the walls, as in the printed area, of the model. This is an abstract representation not equal with the outline. See calculateWallRestrictions for better description. 提供代表模型壁（即打印区域）的区域。这是一个抽象表示，不等同于轮廓。详见calculateWallRestrictions。
     * \param radius The radius of the node of interest. 关注节点的半径。
     * \param layer_idx The layer of interest. 关注的层。
     * \param min_xy_dist is the minimum xy distance used. 是否使用最小XY距离。
     * \return Polygons object 多边形对象
     */
    const Polygons& getWallRestriction(coord_t radius, LayerIndex layer_idx, bool min_xy_dist) const;
    /*!
     * \brief Round \p radius upwards to either a multiple of m_radius_sample_resolution or a exponentially increasing value 将\p radius向上取整为m_radius_sample_resolution的倍数或指数增长的值
     *
     *	It also adds the difference between the minimum xy distance and the regular one. 还会加上最小XY距离与常规XY距离之间的差值。
     *
     * \param radius The radius of the node of interest 关注节点的半径
     * \param min_xy_dist is the minimum xy distance used. 是否使用最小XY距离。
     * \return The rounded radius 取整后的半径
     */
    coord_t ceilRadius(const coord_t radius, const bool min_xy_dist) const {
        assert(radius >= 0);
        return min_xy_dist ? 
            this->ceilRadius(radius) :
            // special case as if a radius 0 is requested it could be to ensure correct xy distance. As such it is beneficial if the collision is as close to the configured values as possible. 特殊情况：如果请求半径为0，可能是为了确保正确的XY距离。因此，使碰撞尽可能接近配置值是有利的。
            radius > 0 ? this->ceilRadius(radius + m_current_min_xy_dist_delta) : m_current_min_xy_dist_delta;
    }
    /*!
     * \brief Round \p radius upwards to the maximum that would still round up to the same value as the provided one. 将\p radius向上取整为仍能取整到与提供的值相同的最大值。
     *
     * \param radius The radius of the node of interest 关注节点的半径
     * \param min_xy_dist is the minimum xy distance used. 是否使用最小XY距离。
     * \return The maximum radius, resulting in the same rounding. 产生相同取整结果的最大半径。
     */
    coord_t getRadiusNextCeil(coord_t radius, bool min_xy_dist) const {
        assert(radius > 0);
        return min_xy_dist ?
            this->ceilRadius(radius) :
            this->ceilRadius(radius + m_current_min_xy_dist_delta) - m_current_min_xy_dist_delta;
    }

    Polygon m_bed_area;

private:
    // Caching polygons for a range of layers. 缓存一定层范围内的多边形。
    class LayerPolygonCache {
    public:
        void allocate(LayerIndex aidx_begin, LayerIndex aidx_end) {
            m_idx_begin = aidx_begin;
            m_idx_end = aidx_end;
            m_polygons.assign(aidx_end - aidx_begin, {});
        }

        LayerIndex begin() const { return m_idx_begin; }
        LayerIndex end()   const { return m_idx_end; }
        size_t     size()  const { return m_polygons.size(); }

        bool      has(LayerIndex idx) const { return idx >= m_idx_begin && idx < m_idx_end; }
        Polygons& operator[](LayerIndex idx) { assert(idx >= m_idx_begin && idx < m_idx_end); return m_polygons[idx - m_idx_begin]; }
        std::vector<Polygons>& polygons_mutable() { return m_polygons; }

    private:
        std::vector<Polygons> m_polygons;
        LayerIndex            m_idx_begin;
        LayerIndex            m_idx_end;
    };

    /*!
     * \brief Convenience typedef for the keys to the caches 缓存键的便捷类型定义
     */
    using RadiusLayerPair             = std::pair<coord_t, LayerIndex>;
    class RadiusLayerPolygonCache {
        // Map from radius to Polygons. Cache of one layer collision regions. 从半径到多边形的映射。一层碰撞区域的缓存。
        using LayerData = std::map<coord_t, Polygons>;
        // Vector of layers, at each layer map of radius to Polygons. 层向量，每层存储半径到多边形的映射。
        // Reference to Polygons returned shall be stable to insertion. 返回的多边形引用应在插入后保持稳定。
        using Layers = std::vector<LayerData>;
    public:
        RadiusLayerPolygonCache() = default;
        RadiusLayerPolygonCache(RadiusLayerPolygonCache &&rhs) : m_data(std::move(rhs.m_data)) {}
        RadiusLayerPolygonCache& operator=(RadiusLayerPolygonCache &&rhs) { m_data = std::move(rhs.m_data); return *this; }

        RadiusLayerPolygonCache(const RadiusLayerPolygonCache&) = delete;
        RadiusLayerPolygonCache& operator=(const RadiusLayerPolygonCache&) = delete;

        void insert(std::vector<std::pair<RadiusLayerPair, Polygons>> &&in) {
            std::lock_guard<std::mutex> guard(m_mutex);
            for (auto &d : in)
                this->get_allocate_layer_data(d.first.second).emplace(d.first.first, std::move(d.second));
        }
        // by layer 按层
        void insert(std::vector<std::pair<coord_t, Polygons>> &&in, coord_t radius) {
            std::lock_guard<std::mutex> guard(m_mutex);
            for (auto &d : in)
                this->get_allocate_layer_data(d.first).emplace(radius, std::move(d.second));
        }
        void insert(std::vector<Polygons> &&in, coord_t first_layer_idx, coord_t radius) {
            std::lock_guard<std::mutex> guard(m_mutex);
            allocate_layers(first_layer_idx + in.size());
            for (auto &d : in)
                m_data[first_layer_idx ++].emplace(radius, std::move(d));
        }
        void insert(LayerPolygonCache &&in, coord_t radius) {
            std::lock_guard<std::mutex> guard(m_mutex);
            LayerIndex i = in.begin();
            allocate_layers(i + LayerIndex(in.size()));
            for (auto &d : in.polygons_mutable())
                m_data[i ++].emplace(radius, std::move(d));
        }
        /*!
         * \brief Checks a cache for a given RadiusLayerPair and returns it if it is found 检查缓存中是否存在给定的RadiusLayerPair，如果找到则返回
         * \param key RadiusLayerPair of the requested areas. The radius will be calculated up to the provided layer. 请求区域的RadiusLayerPair。半径将计算到提供的层。
         * \return A wrapped optional reference of the requested area (if it was found, an empty optional if nothing was found) 请求区域的包装可选引用（如果找到则为引用，如果未找到则为空optional）
         */
        std::optional<std::reference_wrapper<const Polygons>> getArea(const TreeModelVolumes::RadiusLayerPair &key) const {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (key.second >= LayerIndex(m_data.size()))
                return std::optional<std::reference_wrapper<const Polygons>>{};
            const auto &layer = m_data[key.second];
            auto it = layer.find(key.first);
            return it == layer.end() ? 
                std::optional<std::reference_wrapper<const Polygons>>{} : std::optional<std::reference_wrapper<const Polygons>>{ it->second };
        }
        // Get a collision area at a given layer for a radius that is a lower or equial to the key radius. 获取指定层上半径小于或等于关键半径的碰撞区域。
        std::optional<std::pair<coord_t, std::reference_wrapper<const Polygons>>> get_lower_bound_area(const TreeModelVolumes::RadiusLayerPair &key) const {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (key.second >= LayerIndex(m_data.size()))
                return {};
            const auto &layer = m_data[key.second];
            if (layer.empty())
                return {};
            auto it = layer.lower_bound(key.first);
            if (it == layer.end() || it->first != key.first) {
                if (it == layer.begin())
                    return {};
                -- it;
            }
            return std::make_pair(it->first, std::reference_wrapper<const Polygons>(it->second));
        }
        /*!
         * \brief Get the highest already calculated layer in the cache. 获取缓存中已计算的最高层。
         * \param radius The radius for which the highest already calculated layer has to be found. 需要查找已计算最高层的半径。
         * \param map The cache in which the lookup is performed. 执行查找的缓存。
         *
         * \return A wrapped optional reference of the requested area (if it was found, an empty optional if nothing was found) 请求区域的包装可选引用（如果找到则为引用，如果未找到则为空optional）
         */
        LayerIndex getMaxCalculatedLayer(coord_t radius) const {
            std::lock_guard<std::mutex> guard(m_mutex);
            auto layer_idx = LayerIndex(m_data.size()) - 1;
            for (; layer_idx > 0; -- layer_idx)
                if (const auto &layer = m_data[layer_idx]; layer.find(radius) != layer.end())
                    break;
            // The placeable on model areas do not exist on layer 0, as there can not be model below it. As such it may be possible that layer 1 is available, but layer 0 does not exist. 模型上的可放置区域不存在于第0层，因为其下方不可能有模型。因此可能出现第1层可用但第0层不存在的情况。
            return layer_idx == 0 ? -1 : layer_idx;
        }

        // For debugging purposes, sorted by layer index, then by radius. 用于调试目的，按层索引排序，然后按半径排序。
        [[nodiscard]] std::vector<std::pair<RadiusLayerPair, std::reference_wrapper<const Polygons>>> sorted() const;

        void clear() { m_data.clear(); }
        void clear_all_but_radius0() { 
            for (LayerData &l : m_data) {
                auto begin = l.begin();
                auto end = l.end();
                if (begin != end && ++ begin != end)
                    l.erase(begin, end);
            }
        }

    private:
        LayerData&          get_allocate_layer_data(LayerIndex layer_idx) {
            allocate_layers(layer_idx + 1);
            return m_data[layer_idx];
        }
        void                allocate_layers(size_t num_layers);

        Layers              m_data;
        mutable std::mutex  m_mutex;
    };


    /*!
     * \brief Provides the areas that have to be avoided by the tree's branches to prevent collision with the model on this layer. Holes are removed. 提供树状分支必须避让的区域，以防止在此层上与模型碰撞。孔洞已被移除。
     *
     * The result is a 2D area that would cause nodes of given radius to collide with the model or be inside a hole. 结果是一个2D区域，给定半径的节点与该区域相交会导致与模型碰撞或位于孔洞内。
     * A Hole is defined as an area, in which a branch with m_increase_until_radius radius would collide with the wall. 孔洞定义为：半径为m_increase_until_radius的分支会与壁碰撞的区域。
     * minimum xy distance is always used. 始终使用最小XY距离。
     * \param radius The radius of the node of interest 关注节点的半径
     * \param layer_idx The layer of interest 关注的层
     * \param min_xy_dist Is the minimum xy distance used. 是否使用最小XY距离。
     * \return Polygons object 多边形对象
     */
    const Polygons& getCollisionHolefree(coord_t radius, LayerIndex layer_idx) const;

    /*!
     * \brief Round \p radius upwards to either a multiple of m_radius_sample_resolution or a exponentially increasing value 将\p radius向上取整为m_radius_sample_resolution的倍数或指数增长的值
     *
     * \param radius The radius of the node of interest 关注节点的半径
     */
    coord_t ceilRadius(const coord_t radius) const;

    /*!
     * \brief Creates the areas that have to be avoided by the tree's branches to prevent collision with the model on this layer. 创建树状分支必须避让的区域，以防止在此层上与模型碰撞。
     *
     * The result is a 2D area that would cause nodes of given radius to collide with the model. Result is saved in the cache. 结果是一个2D区域，给定半径的节点与该区域相交会导致与模型碰撞。结果保存在缓存中。
     * \param keys RadiusLayerPairs of all requested areas. Every radius will be calculated up to the provided layer. 所有请求区域的RadiusLayerPairs。每个半径将计算到提供的层。
     */
    void calculateCollision(const std::vector<RadiusLayerPair> &keys, std::function<void()> throw_on_cancel);
    void calculateCollision(const coord_t radius, const LayerIndex max_layer_idx, std::function<void()> throw_on_cancel);
    /*!
     * \brief Creates the areas that have to be avoided by the tree's branches to prevent collision with the model on this layer. Holes are removed. 创建树状分支必须避让的区域，以防止在此层上与模型碰撞。孔洞已被移除。
     *
     * The result is a 2D area that would cause nodes of given radius to collide with the model or be inside a hole. Result is saved in the cache. 结果是一个2D区域，给定半径的节点与该区域相交会导致与模型碰撞或位于孔洞内。结果保存在缓存中。
     * A Hole is defined as an area, in which a branch with m_increase_until_radius radius would collide with the wall. 孔洞定义为：半径为m_increase_until_radius的分支会与壁碰撞的区域。
     * \param keys RadiusLayerPairs of all requested areas. Every radius will be calculated up to the provided layer. 所有请求区域的RadiusLayerPairs。每个半径将计算到提供的层。
     */
    void calculateCollisionHolefree(const std::vector<RadiusLayerPair> &keys, std::function<void()> throw_on_cancel);

    /*!
     * \brief Creates the areas that have to be avoided by the tree's branches to prevent collision with the model on this layer. Holes are removed. 创建树状分支必须避让的区域，以防止在此层上与模型碰撞。孔洞已被移除。
     *
     * The result is a 2D area that would cause nodes of given radius to collide with the model or be inside a hole. Result is saved in the cache. 结果是一个2D区域，给定半径的节点与该区域相交会导致与模型碰撞或位于孔洞内。结果保存在缓存中。
     * A Hole is defined as an area, in which a branch with m_increase_until_radius radius would collide with the wall. 孔洞定义为：半径为m_increase_until_radius的分支会与壁碰撞的区域。
     * \param key RadiusLayerPairs the requested areas. The radius will be calculated up to the provided layer. 请求区域的RadiusLayerPair。半径将计算到提供的层。
     */
    void calculateCollisionHolefree(RadiusLayerPair key)
    {
        calculateCollisionHolefree(std::vector<RadiusLayerPair>{ RadiusLayerPair(key) }, []{});
    }

    /*!
     * \brief Creates the areas that have to be avoided by the tree's branches to prevent collision with the model. 创建树状分支必须避让的区域，以防止与模型碰撞。
     *
     * The result is a 2D area that would cause nodes of radius \p radius to collide with the model. Result is saved in the cache. 结果是一个2D区域，半径为\p radius的节点与该区域相交会导致与模型碰撞。结果保存在缓存中。
     * \param keys RadiusLayerPairs of all requested areas. Every radius will be calculated up to the provided layer. 所有请求区域的RadiusLayerPairs。每个半径将计算到提供的层。
     */
    void calculateAvoidance(const std::vector<RadiusLayerPair> &keys, bool to_build_plate, bool to_model, std::function<void()> throw_on_cancel);

    /*!
     * \brief Creates the areas that have to be avoided by the tree's branches to prevent collision with the model. 创建树状分支必须避让的区域，以防止与模型碰撞。
     *
     * The result is a 2D area that would cause nodes of radius \p radius to collide with the model. Result is saved in the cache. 结果是一个2D区域，半径为\p radius的节点与该区域相交会导致与模型碰撞。结果保存在缓存中。
     * \param key RadiusLayerPair of the requested areas. It will be calculated up to the provided layer. 请求区域的RadiusLayerPair。它将计算到提供的层。
     */
    void calculateAvoidance(RadiusLayerPair key, bool to_build_plate, bool to_model)
    {
        calculateAvoidance(std::vector<RadiusLayerPair>{ RadiusLayerPair(key) }, to_build_plate, to_model, []{});
    }

    /*!
     * \brief Creates the areas where a branch of a given radius can be place on the model. 创建给定半径的分支可以放置在模型上的区域。
     * Result is saved in the cache. 结果保存在缓存中。
     * \param key RadiusLayerPair of the requested areas. It will be calculated up to the provided layer. 请求区域的RadiusLayerPair。它将计算到提供的层。
     */
    void calculatePlaceables(const coord_t radius, const LayerIndex max_required_layer, std::function<void()> throw_on_cancel);


    /*!
     * \brief Creates the areas where a branch of a given radius can be placed on the model. 创建给定半径的分支可以放置在模型上的区域。
     * Result is saved in the cache. 结果保存在缓存中。
     * \param keys RadiusLayerPair of the requested areas. The radius will be calculated up to the provided layer. 请求区域的RadiusLayerPair。半径将计算到提供的层。
     */
    void calculatePlaceables(const std::vector<RadiusLayerPair> &keys, std::function<void()> throw_on_cancel);

    /*!
     * \brief Creates the areas that can not be passed when expanding an area downwards. As such these areas are an somewhat abstract representation of a wall (as in a printed object). 创建向下扩展区域时无法通过的区域。因此这些区域是壁（如打印对象）的某种抽象表示。
     *
     * These areas are at least xy_min_dist wide. When calculating it is always assumed that every wall is printed on top of another (as in has an overlap with the wall a layer below). Result is saved in the corresponding cache. 这些区域至少为xy_min_dist宽。计算时始终假设每个壁都打印在另一个壁之上（即与下一层的壁有重叠）。结果保存在相应的缓存中。
     *
     * \param keys RadiusLayerPairs of all requested areas. Every radius will be calculated up to the provided layer. 所有请求区域的RadiusLayerPairs。每个半径将计算到提供的层。
     */
    void calculateWallRestrictions(const std::vector<RadiusLayerPair> &keys, std::function<void()> throw_on_cancel);

    /*!
     * \brief Creates the areas that can not be passed when expanding an area downwards. As such these areas are an somewhat abstract representation of a wall (as in a printed object). 创建向下扩展区域时无法通过的区域。因此这些区域是壁（如打印对象）的某种抽象表示。
     * These areas are at least xy_min_dist wide. When calculating it is always assumed that every wall is printed on top of another (as in has an overlap with the wall a layer below). Result is saved in the corresponding cache. 这些区域至少为xy_min_dist宽。计算时始终假设每个壁都打印在另一个壁之上（即与下一层的壁有重叠）。结果保存在相应的缓存中。
     * \param key RadiusLayerPair of the requested area. It well be will be calculated up to the provided layer. 请求区域的RadiusLayerPair。它将计算到提供的层。
     */
    void calculateWallRestrictions(RadiusLayerPair key)
    {
        calculateWallRestrictions(std::vector<RadiusLayerPair>{ RadiusLayerPair(key) }, []{});
    }

    /*!
     * \brief The maximum distance that the center point of a tree branch may move in consecutive layers if it has to avoid the model. 如果树状分支必须避让模型，其中心点在连续层之间可以移动的最大距离。
     */
    coord_t m_max_move;
    /*!
     * \brief The maximum distance that the centre-point of a tree branch may move in consecutive layers if it does not have to avoid the model 如果树状分支不需要避让模型，其中心点在连续层之间可以移动的最大距离
     */
    coord_t m_max_move_slow;
    /*!
     * \brief The smallest maximum resolution for simplify 用于简化的最小最大分辨率
     */
    coord_t m_min_resolution;

    bool m_precalculated = false;
    /*!
     * \brief The index to access the outline corresponding with the currently processing mesh 用于访问与当前处理网格对应的轮廓的索引
     */
    size_t m_current_outline_idx;
    /*!
     * \brief The minimum required clearance between the model and the tree branches 模型与树状分支之间的最小所需间隙
     */
    coord_t m_current_min_xy_dist;
    /*!
     * \brief The difference between the minimum required clearance between the model and the tree branches and the regular one. 模型与树状分支之间的最小所需间隙与常规间隙之间的差值。
     */
    coord_t m_current_min_xy_dist_delta;
    /*!
     * \brief Does at least one mesh allow support to rest on a model. 是否至少有一个网格允许支撑搁置在模型上。
     */
    bool m_support_rests_on_model;
#ifdef SLIC3R_TREESUPPORTS_PROGRESS
    /*!
     * \brief The progress of the precalculate function for communicating it to the progress bar. precalculate函数的进度，用于与进度条通信。
     */
    coord_t m_precalculation_progress = 0;
    /*!
     * \brief The progress multiplier of all values added progress bar. 添加到进度条的所有数值的进度乘数。
     * Required for the progress bar the behave as expected when areas have to be calculated multiple times 当区域需要多次计算时，确保进度条按预期运行。
     */
    double m_progress_multiplier;
    /*!
     * \brief The progress offset added to all values communicated to the progress bar. 添加到传递给进度条的所有数值的进度偏移量。
     * Required for the progress bar the behave as expected when areas have to be calculated multiple times 当区域需要多次计算时，确保进度条按预期运行。
     */
    double m_progress_offset;
#endif // SLIC3R_TREESUPPORTS_PROGRESS
    /*!
     * \brief Increase radius in the resulting drawn branches, even if the avoidance does not allow it. Will be cut later to still fit. 在最终绘制的分支中增加半径，即使避让不允许。稍后会被裁剪以适应。
     */
    coord_t m_increase_until_radius;

    /*!
     * \brief Polygons representing the limits of the printable area of the machine 表示机器可打印区域边界的多边形
     */
    Polygons m_machine_border;
    /*!
     * \brief Storage for layer outlines and the corresponding settings of the meshes grouped by meshes with identical setting. 存储层轮廓和网格的对应设置，按具有相同设置的网格分组。
     */
    std::vector<std::pair<TreeSupportMeshGroupSettings, std::vector<Polygons>>> m_layer_outlines;
    /*!
     * \brief Storage for areas that should be avoided, like support blocker or previous generated trees. 存储应避让的区域，如支撑阻挡器或先前生成的树。
     */
    std::vector<Polygons> m_anti_overhang;
    /*!
     * \brief Radii that can be ignored by ceilRadius as they will never be requested, sorted. ceilRadius可以忽略的半径值（因为它们永远不会被请求），已排序。
     */
    std::vector<coord_t> m_ignorable_radii;

    /*!
     * \brief Smallest radius a branch can have. This is the radius of a SupportElement with DTT=0. 分支可以拥有的最小半径。这是DTT=0的SupportElement的半径。
     */
    coord_t m_radius_0;

    // Z heights of the raft layers (additional layers below the object, last raft layer aligned with the bottom of the first object layer). 筏层的Z高度（物体下方的附加层，最后一层筏层与第一层物体层的底部对齐）。
    std::vector<double>         m_raft_layers;

    /*!
     * \brief Caches for the collision, avoidance and areas on the model where support can be placed safely at given radius and layer indices. 在给定半径和层索引下，碰撞、避让以及模型上可以安全放置支撑区域的缓存。
     */
    RadiusLayerPolygonCache     m_collision_cache;
    RadiusLayerPolygonCache     m_collision_cache_holefree;
    RadiusLayerPolygonCache     m_avoidance_cache;
    RadiusLayerPolygonCache     m_avoidance_cache_slow;
    RadiusLayerPolygonCache     m_avoidance_cache_to_model;
    RadiusLayerPolygonCache     m_avoidance_cache_to_model_slow;
    RadiusLayerPolygonCache     m_placeable_areas_cache;

    /*!
     * \brief Caches to avoid holes smaller than the radius until which the radius is always increased, as they are free of holes. 用于避让小于半径的孔洞的缓存，在这些区域中半径始终增加，因为它们没有孔洞。
     * Also called safe avoidances, as they are safe regarding not running into holes. 也称为安全避让，因为它们在不会遇到孔洞方面是安全的。
     */
    RadiusLayerPolygonCache     m_avoidance_cache_holefree;
    RadiusLayerPolygonCache     m_avoidance_cache_holefree_to_model;

    RadiusLayerPolygonCache& avoidance_cache(const AvoidanceType type, const bool to_model) {
        if (to_model) {
            switch (type) {
            case AvoidanceType::Fast:       return m_avoidance_cache_to_model;
            case AvoidanceType::Slow:       return m_avoidance_cache_to_model_slow;
            case AvoidanceType::Count:      assert(false);
            case AvoidanceType::FastSafe:   return m_avoidance_cache_holefree_to_model;
            }
        } else {
            switch (type) {
            case AvoidanceType::Fast:       return m_avoidance_cache;
            case AvoidanceType::Slow:       return m_avoidance_cache_slow;
            case AvoidanceType::Count:      assert(false);
            case AvoidanceType::FastSafe:   return m_avoidance_cache_holefree;
            }
        }
        assert(false);
        return m_avoidance_cache;
    }
    const RadiusLayerPolygonCache& avoidance_cache(const AvoidanceType type, const bool to_model) const {
        return const_cast<TreeModelVolumes*>(this)->avoidance_cache(type, to_model);
    }

    /*!
     * \brief Caches to represent walls not allowed to be passed over. 用于表示不允许穿过的壁的缓存。
     */
    RadiusLayerPolygonCache     m_wall_restrictions_cache;

    // A different cache for min_xy_dist as the maximal safe distance an influence area can be increased(guaranteed overlap of two walls in consecutive layer)
    // is much smaller when min_xy_dist is used. This causes the area of the wall restriction to be thinner and as such just using the min_xy_dist wall
    // restriction would be slower. 用于min_xy_dist的不同缓存，因为使用min_xy_dist时，影响区域可以增加的最大安全距离（保证连续层中两个壁重叠）要小得多。
    // 这导致壁限制区域更薄，因此仅使用min_xy_dist壁限制会更慢。
    RadiusLayerPolygonCache     m_wall_restrictions_cache_min;

#ifdef SLIC3R_TREESUPPORTS_PROGRESS
    std::unique_ptr<std::mutex> m_critical_progress { std::make_unique<std::mutex>() };
#endif // SLIC3R_TREESUPPORTS_PROGRESS
};

} // namespace TreeSupport3D
} // namespace Slic3r

#endif //slic3r_TreeModelVolumes_hpp
