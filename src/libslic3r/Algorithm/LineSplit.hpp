﻿#ifndef SRC_LIBSLIC3R_ALGORITHM_LINE_SPLIT_HPP_
#define SRC_LIBSLIC3R_ALGORITHM_LINE_SPLIT_HPP_

#include "ClipperZUtils.hpp"

namespace Slic3r {
namespace Algorithm {

struct SplitLineJunction
{
    Point p;

    // 如果该点与下一个点之间的线段位于裁剪多边形内部（或在其边缘上），则为 true
    bool clipped;

    // 来自原始输入的索引。
    // - 如果该连接点出现在源多边形/折线中，则为该点在源中的索引；
    // - 如果该点是由相交产生的新点，则为 -(1+参与此相交的源线段第一个点的索引)；
    // - 如果该连接点来自裁剪多边形，则将其视为新点。
    int64_t src_idx;

    SplitLineJunction(const Point& p, bool clipped, int64_t src_idx)
        : p(p)
        , clipped(clipped)
        , src_idx(src_idx) {}

    bool is_src() const { return src_idx >= 0; }
    size_t get_src_index() const
    {
        if (is_src()) {
            return src_idx;
        } else {
            return -src_idx - 1;
        }
    }
};

using SplittedLine = std::vector<SplitLineJunction>;

SplittedLine do_split_line(const ClipperZUtils::ZPath& path, const ExPolygons& clip, bool closed);

// 返回分割后的线段，如果未发现相交则返回空
template<class PathType>
SplittedLine split_line(const PathType& path, const ExPolygons& clip, bool closed)
{
    if (path.empty()) {
        return {};
    }

    // 将输入路径转换为开放的 ZPath
    ClipperZUtils::ZPath p;
    p.reserve(path.size() + closed ? 1 : 0);
    ClipperLib_Z::cInt z = 0;
    for (const auto& point : path) {
        p.emplace_back(point.x(), point.y(), z);
        z++;
    }
    if (closed) {
        // 在末尾复制第一个点，使闭合路径变为开放
        p.emplace_back(p.front());
        p.back().z() = z;
    }

    return do_split_line(p, clip, closed);
}

} // Algorithm
} // Slic3r

#endif /* SRC_LIBSLIC3R_ALGORITHM_LINE_SPLIT_HPP_ */
