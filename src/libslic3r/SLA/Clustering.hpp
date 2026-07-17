#ifndef SLA_CLUSTERING_HPP
#define SLA_CLUSTERING_HPP

#include <vector>

#include <libslic3r/Point.hpp>
#include <libslic3r/SLA/SpatIndex.hpp>

namespace Slic3r { namespace sla {

using ClusterEl = std::vector<unsigned>;
using ClusteredPoints = std::vector<ClusterEl>;

// 根据给定距离对一组点进行聚类。
ClusteredPoints cluster(const std::vector<unsigned>& indices,
                        std::function<Vec3d(unsigned)> pointfn,
                        double dist,
                        unsigned max_points);

ClusteredPoints cluster(const Eigen::MatrixXd& points,
                        double dist,
                        unsigned max_points);

ClusteredPoints cluster(
    const std::vector<unsigned>& indices,
    std::function<Vec3d(unsigned)> pointfn,
    std::function<bool(const PointIndexEl&, const PointIndexEl&)> predicate,
    unsigned max_points);

// 此函数返回输入点索引向量 'clust' 中质心的位置。
template<class DistFn, class PointFn>
long cluster_centroid(const ClusterEl &clust, PointFn pointfn, DistFn df)
{
    switch(clust.size()) {
    case 0: /* 空聚类 */ return -1;
    case 1: /* 只有一个元素 */ return 0;
    case 2: /* 如果有两个元素，则没有中心 */ return 0;
    default: ;
    }

    // 该函数通过计算每个点到聚类中所有其他点的平均距离来工作。
    // 我们创建一个与聚类大小相同的选择器位掩码。位掩码将有两位为真，
    // 其余项为假，我们将遍历位掩码的所有排列（两个点的组合）。
    // 获取两个点的距离并将距离加到平均值中。
    // 平均距离最小的点获胜。

    // 复杂度应为 O(n^2)，但我们主要将此函数仅应用于小聚类（约3个元素）

    std::vector<bool> sel(clust.size(), false);   // 创建全零位掩码
    std::fill(sel.end() - 2, sel.end(), true);    // 插入两个1
    std::vector<double> avgs(clust.size(), 0.0);  // 存储平均距离

    do {
        std::array<size_t, 2> idx;
        for(size_t i = 0, j = 0; i < clust.size(); i++)
            if(sel[i]) idx[j++] = i;

        double d = df(pointfn(clust[idx[0]]),
                      pointfn(clust[idx[1]]));

        // 将距离加到两个关联点的总和中
        for(auto i : idx) avgs[i] += d;

        // 继续处理位掩码的下一个排列（包含两个1）
    } while(std::next_permutation(sel.begin(), sel.end()));

    // 除以聚类中的点数以获得平均值（可能冗余）
    for(auto& a : avgs) a /= clust.size();

    // 获取最小的平均距离并返回索引
    auto minit = std::min_element(avgs.begin(), avgs.end());
    return long(minit - avgs.begin());
}


}} // namespace Slic3r::sla

#endif // CLUSTERING_HPP
