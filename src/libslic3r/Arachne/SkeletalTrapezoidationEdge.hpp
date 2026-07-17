﻿//Copyright (c) 2021 Ultimaker B.V.
//CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#ifndef SKELETAL_TRAPEZOIDATION_EDGE_H
#define SKELETAL_TRAPEZOIDATION_EDGE_H

#include <memory> // 智能指针
#include <list>
#include <vector>

#include "utils/ExtrusionJunction.hpp"

namespace Slic3r::Arachne
{

class SkeletalTrapezoidationEdge
{
private:
    enum class Central { UNKNOWN = -1, NO, YES };

public:
    /*!
     * Representing the location along an edge where the anchor position of a transition should be placed.
     */
    struct TransitionMiddle
    {
        coord_t pos; // 沿边的位置，从 edge.from.p 开始测量
        int lower_bead_count;
        coord_t feature_radius; // 放置此过渡的特征半径
        TransitionMiddle(coord_t pos, int lower_bead_count, coord_t feature_radius)
            : pos(pos), lower_bead_count(lower_bead_count)
            , feature_radius(feature_radius)
        {}
    };

    /*!
     * Represents the location along an edge where the lower or upper end of a transition should be placed.
     */
    struct TransitionEnd
    {
        coord_t pos; // 沿边的位置，从 edge.from.p 开始测量，边始终是从较低R到较高R的半边方向
        int lower_bead_count;
        bool is_lower_end; // 是否是具有较低 bead 计数的过渡端
        TransitionEnd(coord_t pos, int lower_bead_count, bool is_lower_end)
            : pos(pos), lower_bead_count(lower_bead_count), is_lower_end(is_lower_end)
        {}
    };

    enum class EdgeType
    {
        NORMAL = 0, // 来自 voronoi 图
        EXTRA_VD = 1, // 引入到 voronoi 图中以生成 gMAT
        TRANSITION_END = 2 // 引入到 voronoi 图中以生成 gMAT
    };
    EdgeType type;

    SkeletalTrapezoidationEdge() : SkeletalTrapezoidationEdge(EdgeType::NORMAL) {}
    SkeletalTrapezoidationEdge(const EdgeType &type) : type(type), is_central(Central::UNKNOWN) {}

    bool isCentral() const
    {
        assert(is_central != Central::UNKNOWN);
        return is_central == Central::YES;
    }
    void setIsCentral(bool b)
    {
        is_central = b ? Central::YES : Central::NO;
    }
    bool centralIsSet() const
    {
        return is_central != Central::UNKNOWN;
    }

    bool hasTransitions(bool ignore_empty = false) const
    {
        return transitions.use_count() > 0 && (ignore_empty || ! transitions.lock()->empty());
    }
    void setTransitions(std::shared_ptr<std::list<TransitionMiddle>> storage)
    {
        transitions = storage;
    }
    std::shared_ptr<std::list<TransitionMiddle>> getTransitions()
    {
        return transitions.lock();
    }

    bool hasTransitionEnds(bool ignore_empty = false) const
    {
        return transition_ends.use_count() > 0 && (ignore_empty || ! transition_ends.lock()->empty());
    }
    void setTransitionEnds(std::shared_ptr<std::list<TransitionEnd>> storage)
    {
        transition_ends = storage;
    }
    std::shared_ptr<std::list<TransitionEnd>> getTransitionEnds()
    {
        return transition_ends.lock();
    }

    bool hasExtrusionJunctions(bool ignore_empty = false) const
    {
        return extrusion_junctions.use_count() > 0 && (ignore_empty || ! extrusion_junctions.lock()->empty());
    }
    void setExtrusionJunctions(std::shared_ptr<LineJunctions> storage)
    {
        extrusion_junctions = storage;
    }
    std::shared_ptr<LineJunctions> getExtrusionJunctions()
    {
        return extrusion_junctions.lock();
    }

private:
    Central is_central; //! 边是否显著；源段是否有锐角；-1 表示未知

    std::weak_ptr<std::list<TransitionMiddle>> transitions;
    std::weak_ptr<std::list<TransitionEnd>> transition_ends;
    std::weak_ptr<LineJunctions> extrusion_junctions;
};

} // namespace Slic3r::Arachne
#endif // SKELETAL_TRAPEZOIDATION_EDGE_H
