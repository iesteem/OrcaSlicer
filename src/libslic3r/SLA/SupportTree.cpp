/**
 * 在此文件中，我们将实现自动 SLA 支撑树生成。
 *
 */

#include <numeric>
#include <libslic3r/SLA/SupportTree.hpp>
#include <libslic3r/SLA/SpatIndex.hpp>
#include <libslic3r/SLA/SupportTreeBuilder.hpp>
#include <libslic3r/SLA/SupportTreeBuildsteps.hpp>

#include <libslic3r/MTUtils.hpp>
#include <libslic3r/ClipperUtils.hpp>
#include <libslic3r/Model.hpp>
#include <libslic3r/TriangleMeshSlicer.hpp>

#include <libnest2d/optimizers/nlopt/genetic.hpp>
#include <libnest2d/optimizers/nlopt/subplex.hpp>
#include <boost/log/trivial.hpp>
#include <libslic3r/I18N.hpp>

//! 用于标记被本地化使用的字符串的宏，
//! 返回相同的字符串
#define L(s) Slic3r::I18N::translate(s)

namespace Slic3r {
namespace sla {

void SupportTree::retrieve_full_mesh(indexed_triangle_set &outmesh) const {
    its_merge(outmesh, retrieve_mesh(MeshType::Support));
    its_merge(outmesh, retrieve_mesh(MeshType::Pad));
}

std::vector<ExPolygons> SupportTree::slice(const std::vector<float> &grid,
                                           float                     cr) const
{
    const indexed_triangle_set &sup_mesh = retrieve_mesh(MeshType::Support);
    const indexed_triangle_set &pad_mesh = retrieve_mesh(MeshType::Pad);

    using Slices = std::vector<ExPolygons>;
    auto slices = reserve_vector<Slices>(2);

    if (!sup_mesh.empty()) {
        slices.emplace_back();
        slices.back() = slice_mesh_ex(sup_mesh, grid, cr, ctl().cancelfn);
    }

    if (!pad_mesh.empty()) {
        slices.emplace_back();

        auto bb = bounding_box(pad_mesh);
        auto maxzit = std::upper_bound(grid.begin(), grid.end(), bb.max.z());
        
        auto cap = grid.end() - maxzit;
        auto padgrid = reserve_vector<float>(size_t(cap > 0 ? cap : 0));
        std::copy(grid.begin(), maxzit, std::back_inserter(padgrid));

        slices.back() = slice_mesh_ex(pad_mesh, padgrid, cr, ctl().cancelfn);
    }

    size_t len = grid.size();
    for (const Slices &slv : slices) { len = std::min(len, slv.size()); }

    // 支撑或垫或两者都必须非空
    if (slices.empty()) return {};

    Slices &mrg = slices.front();

    for (auto it = std::next(slices.begin()); it != slices.end(); ++it) {
        for (size_t i = 0; i < len; ++i) {
            Slices &slv = *it;
            std::copy(slv[i].begin(), slv[i].end(), std::back_inserter(mrg[i]));
            slv[i] = {}; // clear and delete
        }
    }

    return mrg;
}

SupportTree::UPtr SupportTree::create(const SupportableMesh &sm,
                                      const JobController &  ctl)
{
    auto builder = make_unique<SupportTreeBuilder>();
    builder->m_ctl = ctl;
    
    if (sm.cfg.enabled) {
        // Execute 负责处理 ground_level
        SupportTreeBuildsteps::execute(*builder, sm);
        builder->merge_and_cleanup();   // 清理元数据，只保留网格。
    } else {
        // 如果之后添加了垫，它将位于正确的 Z 层级
        builder->ground_level = sm.emesh.ground_level();
    }
    
    return std::move(builder);
}

}} // namespace Slic3r::sla
