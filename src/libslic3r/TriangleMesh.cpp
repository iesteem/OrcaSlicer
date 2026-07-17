#include "Exception.hpp"
#include "TriangleMesh.hpp"
#include "TriangleMeshSlicer.hpp"
#include "MeshSplitImpl.hpp"
#include "ClipperUtils.hpp"
#include "Geometry.hpp"
#include "Geometry/ConvexHull.hpp"
#include "Point.hpp"
#include "Execution/ExecutionTBB.hpp"
#include "Execution/ExecutionSeq.hpp"
#include "CutUtils.hpp"
#include "Utils.hpp"
#include "Format/STL.hpp"
#include <libqhullcpp/Qhull.h>
#include <libqhullcpp/QhullFacetList.h>
#include <libqhullcpp/QhullVertexSet.h>

#include <cmath>
#include <deque>
#include <queue>
#include <vector>
#include <utility>
#include <algorithm>
#include <type_traits>

#include <boost/log/trivial.hpp>
#include <boost/nowide/cstdio.hpp>
#include <boost/predef/other/endian.h>

#include <Eigen/Core>
#include <Eigen/Dense>

#include <assert.h>

namespace Slic3r {

static void update_bounding_box(const indexed_triangle_set &its, TriangleMeshStats &out)
{
    BoundingBoxf3 bbox      = Slic3r::bounding_box(its);
    out.min                 = bbox.min.cast<float>();
    out.max                 = bbox.max.cast<float>();
    out.size                = out.max - out.min;    
}

static void fill_initial_stats(const indexed_triangle_set &its, TriangleMeshStats &out)
{
    out.number_of_facets    = its.indices.size();
    out.volume              = its_volume(its);
    update_bounding_box(its, out);

    const std::vector<Vec3i32> face_neighbors = its_face_neighbors(its);
    out.number_of_parts = its_number_of_patches(its, face_neighbors);
    out.open_edges      = its_num_open_edges(face_neighbors);
}

TriangleMesh::TriangleMesh(const std::vector<Vec3f> &vertices, const std::vector<Vec3i32> &faces) : its { faces, vertices }
{
    fill_initial_stats(this->its, m_stats);
}

TriangleMesh::TriangleMesh(std::vector<Vec3f> &&vertices, const std::vector<Vec3i32> &&faces) : its { std::move(faces), std::move(vertices) }
{
    fill_initial_stats(this->its, m_stats);
}

TriangleMesh::TriangleMesh(const indexed_triangle_set &its) : its(its)
{
    fill_initial_stats(this->its, m_stats);
}

TriangleMesh::TriangleMesh(indexed_triangle_set &&its, const RepairedMeshErrors& errors/* = RepairedMeshErrors()*/) : its(std::move(its))
{
    m_stats.repaired_errors = errors;
    fill_initial_stats(this->its, m_stats);
}

// #define SLIC3R_TRACE_REPAIR

static void trianglemesh_repair_on_import(stl_file &stl)
{
    // admesh 在修复空网格时会失败
    if (stl.stats.number_of_facets == 0)
        return;

    BOOST_LOG_TRIVIAL(debug) << "TriangleMesh::repair() started";

    // 检查精确匹配
#ifdef SLIC3R_TRACE_REPAIR
    BOOST_LOG_TRIVIAL(trace) << "\tstl_check_faces_exact";
#endif /* SLIC3R_TRACE_REPAIR */
    assert(stl_validate(&stl));
    stl_check_facets_exact(&stl);
    assert(stl_validate(&stl));
    stl.stats.facets_w_1_bad_edge = (stl.stats.connected_facets_2_edge - stl.stats.connected_facets_3_edge);
    stl.stats.facets_w_2_bad_edge = (stl.stats.connected_facets_1_edge - stl.stats.connected_facets_2_edge);
    stl.stats.facets_w_3_bad_edge = (stl.stats.number_of_facets - stl.stats.connected_facets_1_edge);

    // 检查附近匹配
    //int last_edges_fixed = 0;
    float tolerance = (float)stl.stats.shortest_edge;
    float increment = (float)stl.stats.bounding_diameter / 10000.0f;
    int iterations = 2;
    if (stl.stats.connected_facets_3_edge < int(stl.stats.number_of_facets)) {
        // 不是流形，一些三角形有未连接的边。
        for (int i = 0; i < iterations; ++ i) {
            if (stl.stats.connected_facets_3_edge < int(stl.stats.number_of_facets)) {
                // 仍然不是流形，一些三角形有未连接的边。
                //printf("Checking nearby. Tolerance= %f Iteration=%d of %d...", tolerance, i + 1, iterations);
#ifdef SLIC3R_TRACE_REPAIR
                BOOST_LOG_TRIVIAL(trace) << "\tstl_check_faces_nearby";
#endif /* SLIC3R_TRACE_REPAIR */
                stl_check_facets_nearby(&stl, tolerance);
                //printf("  Fixed %d edges.\n", stl.stats.edges_fixed - last_edges_fixed);
                //last_edges_fixed = stl.stats.edges_fixed;
                tolerance += increment;
            } else {
                break;
            }
        }
    }
    assert(stl_validate(&stl));

    // 移除未连接的面
    if (stl.stats.connected_facets_3_edge < (int)stl.stats.number_of_facets) {
#ifdef SLIC3R_TRACE_REPAIR
        BOOST_LOG_TRIVIAL(trace) << "\tstl_remove_unconnected_facets";
#endif /* SLIC3R_TRACE_REPAIR */
        stl_remove_unconnected_facets(&stl);
        assert(stl_validate(&stl));
    }

    // 填充孔洞
#if 0
    // 不填充孔洞，当前算法对于复杂孔洞弊大于利。
    // 而是让切片算法在2D切片中闭合间隙。
    if (stl.stats.connected_facets_3_edge < stl.stats.number_of_facets) {
#ifdef SLIC3R_TRACE_REPAIR
        BOOST_LOG_TRIVIAL(trace) << "\tstl_fill_holes";
#endif /* SLIC3R_TRACE_REPAIR */
        stl_fill_holes(&stl);
        stl_clear_error(&stl);
    }
#endif

    // 法线方向
#ifdef SLIC3R_TRACE_REPAIR
    BOOST_LOG_TRIVIAL(trace) << "\tstl_fix_normal_directions";
#endif /* SLIC3R_TRACE_REPAIR */
    stl_fix_normal_directions(&stl);
    assert(stl_validate(&stl));

    // 法线值
#ifdef SLIC3R_TRACE_REPAIR
    BOOST_LOG_TRIVIAL(trace) << "\tstl_fix_normal_values";
#endif /* SLIC3R_TRACE_REPAIR */
    stl_fix_normal_values(&stl);
    assert(stl_validate(&stl));

    // 始终计算体积，如果体积为负则反转所有法线
#ifdef SLIC3R_TRACE_REPAIR
    BOOST_LOG_TRIVIAL(trace) << "\tstl_calculate_volume";
#endif /* SLIC3R_TRACE_REPAIR */
    // 如果体积为负，所有面被翻转并添加到 stats.facets_reversed 中。
    stl_calculate_volume(&stl);
    assert(stl_validate(&stl));

    // 邻居
#ifdef SLIC3R_TRACE_REPAIR
    BOOST_LOG_TRIVIAL(trace) << "\tstl_verify_neighbors";
#endif /* SLIC3R_TRACE_REPAIR */
    stl_verify_neighbors(&stl);
    assert(stl_validate(&stl));

    //FIXME admesh 修复函数可能破坏面的连接性，因此在此刷新，因为切片代码依赖它。
    if (auto nr_degenerated = stl.stats.degenerate_facets; stl.stats.number_of_facets > 0 && nr_degenerated > 0)
        stl_check_facets_exact(&stl);

    BOOST_LOG_TRIVIAL(debug) << "TriangleMesh::repair() finished";
}

bool TriangleMesh::from_stl(stl_file& stl, bool repair)
{
    if (repair)
        trianglemesh_repair_on_import(stl);

#if 0
    m_stats.number_of_facets = stl.stats.number_of_facets;
    m_stats.min = stl.stats.min;
    m_stats.max = stl.stats.max;
    m_stats.size = stl.stats.size;
    m_stats.volume = stl.stats.volume;

    auto facets_w_1_bad_edge = stl.stats.connected_facets_2_edge - stl.stats.connected_facets_3_edge;
    auto facets_w_2_bad_edge = stl.stats.connected_facets_1_edge - stl.stats.connected_facets_2_edge;
    auto facets_w_3_bad_edge = stl.stats.number_of_facets - stl.stats.connected_facets_1_edge;
    m_stats.open_edges = stl.stats.backwards_edges + facets_w_1_bad_edge + facets_w_2_bad_edge * 2 + facets_w_3_bad_edge * 3;

    m_stats.repaired_errors = { stl.stats.edges_fixed,
                                stl.stats.degenerate_facets,
                                stl.stats.facets_removed,
                                stl.stats.facets_reversed,
                                stl.stats.backwards_edges };

    m_stats.number_of_parts = stl.stats.number_of_parts;
#endif

    stl_generate_shared_vertices(&stl, this->its);
    fill_initial_stats(this->its, this->m_stats);
    return true;
}

bool TriangleMesh::ReadSTLFile(const char *input_file, bool repair, ImportstlProgressFn stlFn, int custom_header_length)
{
    stl_file stl;
    if (!stl_open(&stl, input_file, stlFn, custom_header_length))
        return false;
    return from_stl(stl, repair);
}

bool TriangleMesh::write_ascii(const char* output_file)
{
    return its_write_stl_ascii(output_file, "", this->its);
}

bool TriangleMesh::write_binary(const char* output_file)
{
    return its_write_stl_binary(output_file, "", this->its);
}

float TriangleMesh::volume()
{
    if (m_stats.volume == -1)
        m_stats.volume = its_volume(this->its);
    return m_stats.volume;
}

void TriangleMesh::WriteOBJFile(const char* output_file) const
{
    its_write_obj(this->its, output_file);
}

void TriangleMesh::scale(float factor)
{
    this->scale(Vec3f(factor, factor, factor));
}

void TriangleMesh::scale(const Vec3f &versor)
{
    // 缩放范围。
    auto s = versor.array();
    m_stats.min.array() *= s;
    m_stats.max.array() *= s;
    // 缩放尺寸。
    m_stats.size.array() *= s;
    // 缩放体积。
    if (m_stats.volume > 0.0)
        m_stats.volume *= s(0) * s(1) * s(2);
    if (versor.x() == versor.y() && versor.x() == versor.z()) {
        float s = versor.x();
        for (stl_vertex &v : this->its.vertices)
            v *= s;
    } else {
        for (stl_vertex &v : this->its.vertices) {
            v.x() *= versor.x();
            v.y() *= versor.y();
            v.z() *= versor.z();
        }
    }
}

void TriangleMesh::translate(const Vec3f &displacement)
{
    if (displacement.x() != 0.f || displacement.y() != 0.f || displacement.z() != 0.f) {
        for (stl_vertex& v : this->its.vertices)
            v += displacement;
        m_stats.min += displacement;
        m_stats.max += displacement;
    }
}

void TriangleMesh::translate(float x, float y, float z)
{
    this->translate(Vec3f(x, y, z));
}

void TriangleMesh::rotate(float angle, const Axis &axis)
{
    if (angle != 0.f) {
        angle = Slic3r::Geometry::rad2deg(angle);
        switch (axis) {
        case X:  its_rotate_x(this->its, angle); break;
        case Y:  its_rotate_y(this->its, angle); break;
        case Z:  its_rotate_z(this->its, angle); break;
        default: assert(false);                  return;
        }
        update_bounding_box(this->its, this->m_stats);
    }
}

void TriangleMesh::rotate(float angle, const Vec3d& axis)
{
    if (angle != 0.f) {
        Vec3d axis_norm = axis.normalized();
        Transform3d m = Transform3d::Identity();
        m.rotate(Eigen::AngleAxisd(angle, axis_norm));
        its_transform(its, m);
        update_bounding_box(this->its, this->m_stats);
    }
}

void TriangleMesh::mirror(const Axis axis)
{
    switch (axis) {
    case X:
        for (stl_vertex &v : its.vertices)
            v.x() *= -1.f;
        break;
    case Y:
        for (stl_vertex& v : this->its.vertices)
            v.y() *= -1.0;
        break;
    case Z:
        for (stl_vertex &v : this->its.vertices)
            v.z() *= -1.0;
        break;
    default:
        assert(false);
        return;
    };
    its_flip_triangles(this->its);
    int iaxis = int(axis);
    std::swap(m_stats.min[iaxis], m_stats.max[iaxis]);
    m_stats.min[iaxis] *= -1.0;
    m_stats.max[iaxis] *= -1.0;
}

void TriangleMesh::transform(const Transform3d& t, bool fix_left_handed)
{
    its_transform(its, t);
    double det = t.matrix().block(0, 0, 3, 3).determinant();
    if (fix_left_handed && det < 0.) {
        its_flip_triangles(its);
        det = -det;
    }
    m_stats.volume *= det;
    update_bounding_box(this->its, this->m_stats);
}

void TriangleMesh::transform(const Matrix3d& m, bool fix_left_handed)
{
    its_transform(its, m);
    double det = m.block(0, 0, 3, 3).determinant();
    if (fix_left_handed && det < 0.) {
        its_flip_triangles(its);
        det = -det;
    }
    m_stats.volume *= det;
    update_bounding_box(this->its, this->m_stats);
}

void TriangleMesh::flip_triangles()
{
    its_flip_triangles(its);
    m_stats.volume = - m_stats.volume;
}

void TriangleMesh::align_to_origin()
{
    this->translate(- m_stats.min(0), - m_stats.min(1), - m_stats.min(2));
}

void TriangleMesh::rotate(double angle, Point* center)
{
    if (angle != 0.) {
        Vec2f c = center->cast<float>();
        this->translate(-c(0), -c(1), 0);
        its_rotate_z(this->its, (float)angle);
        this->translate(c(0), c(1), 0);
    }
}

/**
 * 计算网格是否可分割。
 */
bool TriangleMesh::is_splittable() const
{
    return its_is_splittable(this->its);
}

std::vector<TriangleMesh> TriangleMesh::split() const
{
    std::vector<indexed_triangle_set> itss = its_split(this->its);
    std::vector<TriangleMesh> out;
    out.reserve(itss.size());
    for (indexed_triangle_set &m : itss) {
        // TriangleMesh 构造函数应填充网格统计信息，包括体积。
        out.emplace_back(std::move(m));
        if (TriangleMesh &triangle_mesh = out.back(); triangle_mesh.volume() < 0)
            // 某些源网格部分可能方向不正确。纠正它们。
            triangle_mesh.flip_triangles();

    }
    return out;
}

void TriangleMesh::merge(const TriangleMesh &mesh)
{
    its_merge(this->its, mesh.its);
    m_stats = m_stats.merge(mesh.m_stats);
}

// 计算网格在 XY 平面上的投影，使用缩放坐标。
//FIXME 这可能非常慢！仅用于微小网格！
ExPolygons TriangleMesh::horizontal_projection() const
{
    return union_ex(project_mesh(this->its, Transform3d::Identity(), []() {}));
}

// 投影到 Z=0 平面的 3D 网格的 2D 凸包。
Polygon TriangleMesh::convex_hull() const
{
    Points pp;
    pp.reserve(this->its.vertices.size());
    for (size_t i = 0; i < this->its.vertices.size(); ++ i) {
        const stl_vertex &v = this->its.vertices[i];
        pp.emplace_back(Point::new_scale(v(0), v(1)));
    }
    return Slic3r::Geometry::convex_hull(pp);
}

BoundingBoxf3 TriangleMesh::bounding_box() const
{
    BoundingBoxf3 bb;
    bb.defined = true;
    bb.min = m_stats.min.cast<double>();
    bb.max = m_stats.max.cast<double>();
    return bb;
}

BoundingBoxf3 TriangleMesh::transformed_bounding_box(const Transform3d &trafo) const
{
    BoundingBoxf3 bbox;
    for (const stl_vertex &v : this->its.vertices)
        bbox.merge(trafo * v.cast<double>());
    return bbox;
}

BoundingBoxf3 TriangleMesh::transformed_bounding_box(const Transform3d& trafod, double world_min_z) const
{
    // 1) 分配变换后的顶点及其相对于打印床表面的位置。
    std::vector<char>           sides;
    size_t                      num_above = 0;
    Eigen::AlignedBox<float, 3> bbox;
    Transform3f                 trafo = trafod.cast<float>();
    sides.reserve(its.vertices.size());
    for (const stl_vertex &v : this->its.vertices) {
        const stl_vertex pt   = trafo * v;
        const int        sign = pt.z() > world_min_z ? 1 : pt.z() < world_min_z ? -1 : 0;
        sides.emplace_back(sign);
        if (sign >= 0) {
            // 顶点在打印床表面上或上方。测试它是否在构建体积内。
            ++ num_above;
            bbox.extend(pt);
        }
    }

    // 2) 计算三角形边与构建表面的交点。
    if (num_above < its.vertices.size()) {
        // 未完全在构建表面上，状态可能仍会因测试与构建平台相交的边而改变。
        for (const stl_triangle_vertex_indices &tri : its.indices) {
            const int s[3] = { sides[tri(0)], sides[tri(1)], sides[tri(2)] };
            if (std::min(s[0], std::min(s[1], s[2])) < 0 && std::max(s[0], std::max(s[1], s[2])) > 0) {
                // 此三角形的某些边与构建平台相交。计算交点。
                int iprev = 2;
                for (int iedge = 0; iedge < 3; ++ iedge) {
                    if (s[iprev] * s[iedge] == -1) {
                        // 边与构建表面相交。计算交点。
                        const stl_vertex p1 = trafo * its.vertices[tri(iprev)];
                        const stl_vertex p2 = trafo * its.vertices[tri(iedge)];
                        // 边穿过 z 平面。计算与平面的交点。
                        const float t = (world_min_z - p1.z()) / (p2.z() - p1.z());
                        bbox.extend(Vec3f(p1.x() + (p2.x() - p1.x()) * t, p1.y() + (p2.y() - p1.y()) * t, world_min_z));
                    }
                    iprev = iedge;
                }
            }
        }
    }

    BoundingBoxf3 out;
    if (! bbox.isEmpty()) {
        out.min = bbox.min().cast<double>();
        out.max = bbox.max().cast<double>();
        out.defined = true;
    };
    return out;
}

TriangleMesh TriangleMesh::convex_hull_3d() const
{
    TriangleMesh mesh(its_convex_hull(this->its));
    // qhull 经常产生非流形网格。
    // assert(mesh.stats().manifold());
    return mesh;
}

std::vector<ExPolygons> TriangleMesh::slice(const std::vector<double> &z) const
{
    // 将 double 转换为 float
    std::vector<float> z_f(z.begin(), z.end());
    return slice_mesh_ex(this->its, z_f, 0.0004f);
}

size_t TriangleMesh::memsize() const
{
    size_t memsize = 8 + this->its.memsize() + sizeof(this->m_stats);
    return memsize;
}

// 创建从三角形边到面的映射。
struct EdgeToFace {
    // 三角形边的第一个顶点索引。vertex_low <= vertex_high。
    int  vertex_low;
    // 三角形边的第二个顶点索引。
    int  vertex_high;
    // 三角形面的索引。
    int  face;
    // 面中边的索引，从1开始。如果边以逆序存储在(vertex_low, vertex_high)中，则为负索引。
    int  face_edge;
    bool operator==(const EdgeToFace &other) const { return vertex_low == other.vertex_low && vertex_high == other.vertex_high; }
    bool operator<(const EdgeToFace &other) const { return vertex_low < other.vertex_low || (vertex_low == other.vertex_low && vertex_high < other.vertex_high); }
};

template<typename FaceFilter, typename ThrowOnCancelCallback>
static std::vector<EdgeToFace> create_edge_map(
    const indexed_triangle_set &its, FaceFilter face_filter, ThrowOnCancelCallback throw_on_cancel)
{
    std::vector<EdgeToFace> edges_map;
    edges_map.reserve(its.indices.size() * 3);
    for (uint32_t facet_idx = 0; facet_idx < its.indices.size(); ++ facet_idx)
        if (face_filter(facet_idx))
            for (int i = 0; i < 3; ++ i) {
                edges_map.push_back({});
                EdgeToFace &e2f = edges_map.back();
                e2f.vertex_low  = its.indices[facet_idx][i];
                e2f.vertex_high = its.indices[facet_idx][(i + 1) % 3];
                e2f.face        = facet_idx;
                // 从1开始索引，始终保持严格正数。
                e2f.face_edge   = i + 1;
                if (e2f.vertex_low > e2f.vertex_high) {
                    // 对顶点排序
                    std::swap(e2f.vertex_low, e2f.vertex_high);
                    // 并将 face_edge 设为负值以表示翻转的边。
                    e2f.face_edge = - e2f.face_edge;
                }
            }
    throw_on_cancel();
    std::sort(edges_map.begin(), edges_map.end());

    return edges_map;
}

// 从面边到唯一边标识符的映射，如果不存在邻居则为-1。
// 即使相邻面翻转，两个相邻面也共享唯一的边标识符。
template<typename FaceFilter, typename ThrowOnCancelCallback>
static inline std::vector<Vec3i32> its_face_edge_ids_impl(const indexed_triangle_set &its, FaceFilter face_filter, ThrowOnCancelCallback throw_on_cancel)
{
    std::vector<Vec3i32> out(its.indices.size(), Vec3i32(-1, -1, -1));

    std::vector<EdgeToFace> edges_map = create_edge_map(its, face_filter, throw_on_cancel);

    // 为接触的三角形边分配唯一的公共边ID。
    int num_edges = 0;
    for (size_t i = 0; i < edges_map.size(); ++ i) {
        EdgeToFace &edge_i = edges_map[i];
        if (edge_i.face == -1)
            // 此边已连接到某个邻居。
            continue;
        // 未连接的边。找到其方向正确的邻居。
        size_t j;
        bool found = false;
        for (j = i + 1; j < edges_map.size() && edge_i == edges_map[j]; ++ j)
            if (edge_i.face_edge * edges_map[j].face_edge < 0 && edges_map[j].face != -1) {
                // 面以相反方向的边接触，且两个边均未连接。
                found = true;
                break;
            }
        if (! found) {
            //FIXME Vojtech: 试图找到方向相同的边。这有问题。
            // admesh 可以将同一个边ID分配给两个以上的面（这在拓扑上仍然正确），
            // 因此我们也要搜索此边的重复项，以防它已经以此方向出现过。
            for (j = i + 1; j < edges_map.size() && edge_i == edges_map[j]; ++ j)
                if (edges_map[j].face != -1) {
                    // 面以相同方向的边接触，且两个边均未连接。
                    found = true;
                    break;
                }
        }
        // 为第一个面分配边索引。
        out[edge_i.face](std::abs(edge_i.face_edge) - 1) = num_edges;
        if (found) {
            EdgeToFace &edge_j = edges_map[j];
            out[edge_j.face](std::abs(edge_j.face_edge) - 1) = num_edges;
            // 将边标记为已连接。
            edge_j.face = -1;
        }
        ++ num_edges;
        if ((i & 0x0ffff) == 0)
            throw_on_cancel();
    }

    return out;
}

std::vector<Vec3i32> its_face_edge_ids(const indexed_triangle_set &its)
{
    return its_face_edge_ids_impl(its, [](const uint32_t){ return true; }, [](){});
}

std::vector<Vec3i32> its_face_edge_ids(const indexed_triangle_set &its, std::function<void()> throw_on_cancel_callback)
{
    return its_face_edge_ids_impl(its, [](const uint32_t){ return true; }, throw_on_cancel_callback);
}

std::vector<Vec3i32> its_face_edge_ids(const indexed_triangle_set &its, const std::vector<bool> &face_mask)
{
    return its_face_edge_ids_impl(its, [&face_mask](const uint32_t idx){ return face_mask[idx]; }, [](){});
}

// 在面邻居可用的情况下，为面边分配唯一的边ID，用于在切片上连接多边形。
std::vector<Vec3i32> its_face_edge_ids(const indexed_triangle_set &its, std::vector<Vec3i32> &face_neighbors, bool assign_unbound_edges, int *num_edges)
{
    // out 元素未初始化！
    std::vector<Vec3i32> out(face_neighbors.size());
    int last_edge_id = 0;
    for (int i = 0; i < int(face_neighbors.size()); ++ i) {
        const stl_triangle_vertex_indices   &triangle  = its.indices[i];
        const Vec3i32                         &neighbors = face_neighbors[i];
        for (int j = 0; j < 3; ++ j) {
            int n = neighbors[j];
            if (n > i) {
                const stl_triangle_vertex_indices &triangle2 = its.indices[n];
                int   edge_id = last_edge_id ++;
                Vec2i32 edge    = its_triangle_edge(triangle, j);
                // 首先找到相反方向的边。
                std::swap(edge(0), edge(1));
                int   k       = its_triangle_edge_index(triangle2, edge);
                //FIXME 以下情况现实吗？face_neighbors 可能包含这样的面吗？
                // 如果包含，我们是否希望为那些相互方向错误的边生成相同的边ID？
                if (k == -1) {
                    // 其次找到相同方向的边（邻居三角形可能已被翻转）。
                    std::swap(edge(0), edge(1));
                    k = its_triangle_edge_index(triangle2, edge);
                }
                assert(k >= 0);
                out[i](j) = edge_id;
                out[n](k) = edge_id;
            } else if (n == -1) {
                out[i](j) = assign_unbound_edges ? last_edge_id ++ : -1;
            } else {
                // 三角形永远不能成为自身的邻居。
                assert(n < i);
                // 不做任何操作，邻居将在后续迭代中为我们分配边ID。
            }
        }
    }
    if (num_edges)
        *num_edges = last_edge_id;
    return out;
}

// Merge duplicate vertices, return number of vertices removed.
int its_merge_vertices(indexed_triangle_set &its, bool shrink_to_fit)
{
    // 1) 按坐标和顶点索引的字典序对顶点索引进行排序。
    auto sorted = reserve_vector<int>(its.vertices.size());
    for (int i = 0; i < int(its.vertices.size()); ++ i)
        sorted.emplace_back(i);
    std::sort(sorted.begin(), sorted.end(), [&its](int il, int ir) {
        const Vec3f &l = its.vertices[il];
        const Vec3f &r = its.vertices[ir];
        // Sort lexicographically by coordinates AND vertex index.
        return l.x() < r.x() || (l.x() == r.x() && (l.y() < r.y() || (l.y() == r.y() && (l.z() < r.z() || (l.z() == r.z() && il < ir)))));
    });

    // 2) Map duplicate vertices to the one with the lowest vertex index.
    // The vertex to stay will have a map_vertices[...] == -1 index assigned, the other vertices will point to it.
    std::vector<int> map_vertices(its.vertices.size(), -1);
    for (int i = 0; i < int(sorted.size());) {
        const int    u = sorted[i];
        const Vec3f &p = its.vertices[u];
        int j = i;
        for (++ j; j < int(sorted.size()); ++ j) {
            const int    v = sorted[j];
            const Vec3f &q = its.vertices[v];
            if (p != q)
                break;
            assert(v > u);
            map_vertices[v] = u;
        }
        i = j;
    }

    // 3) 收缩 its.vertices，用新的顶点索引更新 map_vertices。
    int k = 0;
    for (int i = 0; i < int(its.vertices.size()); ++ i) {
        if (map_vertices[i] == -1) {
            map_vertices[i] = k;
            if (k < i)
                its.vertices[k] = its.vertices[i];
            ++ k;
        } else {
            assert(map_vertices[i] < i);
            map_vertices[i] = map_vertices[map_vertices[i]];
        }
    }

    int num_erased = int(its.vertices.size()) - k;

    if (num_erased) {
        // 收缩顶点数组。
        its.vertices.erase(its.vertices.begin() + k, its.vertices.end());
        // 重新映射面索引。
        for (stl_triangle_vertex_indices &face : its.indices)
            for (int i = 0; i < 3; ++ i)
                face(i) = map_vertices[face(i)];
        // 可选地收缩以适应（重新分配）顶点。
        if (shrink_to_fit)
            its.vertices.shrink_to_fit();
    }

    return num_erased;
}

void its_flip_triangles(indexed_triangle_set &its)
{
    for (stl_triangle_vertex_indices &face : its.indices)
        std::swap(face(1), face(2));
}

int its_remove_degenerate_faces(indexed_triangle_set &its, bool shrink_to_fit)
{
    auto it = std::remove_if(its.indices.begin(), its.indices.end(), [](auto &face) {
        return face(0) == face(1) || face(0) == face(2) || face(1) == face(2);
    });

    int removed = std::distance(it, its.indices.end());
    its.indices.erase(it, its.indices.end());

    if (removed && shrink_to_fit)
        its.indices.shrink_to_fit();

    return removed;
}

int its_compactify_vertices(indexed_triangle_set &its, bool shrink_to_fit)
{
    // 首先用于标记被引用的顶点，之后用于将旧顶点索引映射到新顶点索引。
    std::vector<int> vertex_map(its.vertices.size(), 0);
    // 标记被引用的顶点。
    for (const stl_triangle_vertex_indices &face : its.indices)
        for (int i = 0; i < 3; ++ i)
            vertex_map[face(i)] = 1;
    // 压缩顶点，更新从旧顶点索引到新顶点索引的映射。
    int last = 0;
    for (int i = 0; i < int(vertex_map.size()); ++ i)
        if (vertex_map[i]) {
            if (last < i)
                its.vertices[last] = its.vertices[i];
            vertex_map[i] = last ++;
        }
    int removed = int(its.vertices.size()) - last;
    if (removed) {
        its.vertices.erase(its.vertices.begin() + last, its.vertices.end());
        // 使用新的顶点索引更新面。
        for (stl_triangle_vertex_indices &face : its.indices)
            for (int i = 0; i < 3; ++ i)
                face(i) = vertex_map[face(i)];
        // 可选地收缩顶点数组。
        if (shrink_to_fit)
            its.vertices.shrink_to_fit();
    }
    return removed;
}

bool its_store_triangle(const indexed_triangle_set &its,
                        const char *                obj_filename,
                        size_t                      triangle_index)
{
    if (its.indices.size() <= triangle_index) return false;
    Vec3i32                t = its.indices[triangle_index];
    indexed_triangle_set its2;
    its2.indices  = {{0, 1, 2}};
    its2.vertices = {its.vertices[t[0]], its.vertices[t[1]],
                     its.vertices[t[2]]};
    return its_write_obj(its2, obj_filename);
}

bool its_store_triangles(const indexed_triangle_set &its,
                         const char *                obj_filename,
                         const std::vector<size_t> & triangles)
{
    indexed_triangle_set its2;
    its2.vertices.reserve(triangles.size() * 3);
    its2.indices.reserve(triangles.size());
    std::map<size_t, size_t> vertex_map;
    for (auto ti : triangles) {
        if (its.indices.size() <= ti) return false;
        Vec3i32 t = its.indices[ti];
        Vec3i32 new_t;
        for (size_t i = 0; i < 3; ++i) {
            size_t vi = t[i];
            auto   it = vertex_map.find(vi);
            if (it != vertex_map.end()) {
                new_t[i] = it->second;
                continue;
            }
            size_t new_vi = its2.vertices.size();
            its2.vertices.push_back(its.vertices[vi]);
            vertex_map[vi] = new_vi;
            new_t[i]       = new_vi;
        }
        its2.indices.push_back(new_t);
    }
    return its_write_obj(its2, obj_filename);
}

void its_shrink_to_fit(indexed_triangle_set &its)
{
    its.indices.shrink_to_fit();
    its.vertices.shrink_to_fit();
}

template<typename TransformVertex>
void its_collect_mesh_projection_points_above(const indexed_triangle_set &its, const TransformVertex &transform_fn, const float z, Points &all_pts)
{
    all_pts.reserve(all_pts.size() + its.indices.size() * 3);
    for (const stl_triangle_vertex_indices &tri : its.indices) {
        const Vec3f pts[3] = { transform_fn(its.vertices[tri(0)]), transform_fn(its.vertices[tri(1)]), transform_fn(its.vertices[tri(2)]) };
        int iprev = 2;
        for (int iedge = 0; iedge < 3; ++ iedge) {
            const Vec3f &p1 = pts[iprev];
            const Vec3f &p2 = pts[iedge];
            if ((p1.z() < z && p2.z() > z) || (p2.z() < z && p1.z() > z)) {
                // 边穿过 z 平面。计算与平面的交点。
                float t = (z - p1.z()) / (p2.z() - p1.z());
                all_pts.emplace_back(scaled<coord_t>(p1.x() + (p2.x() - p1.x()) * t), scaled<coord_t>(p1.y() + (p2.y() - p1.y()) * t));
            }
            if (p2.z() >= z)
                all_pts.emplace_back(scaled<coord_t>(p2.x()), scaled<coord_t>(p2.y()));
            iprev = iedge;
        }
    }
}

void its_collect_mesh_projection_points_above(const indexed_triangle_set &its, const Matrix3f &m, const float z, Points &all_pts)
{
    return its_collect_mesh_projection_points_above(its, [m](const Vec3f &p){ return m * p; }, z, all_pts);
}

void its_collect_mesh_projection_points_above(const indexed_triangle_set &its, const Transform3f &t, const float z, Points &all_pts)
{
    return its_collect_mesh_projection_points_above(its, [t](const Vec3f &p){ return t * p; }, z, all_pts);
}

template<typename TransformVertex>
Polygon its_convex_hull_2d_above(const indexed_triangle_set &its, const TransformVertex &transform_fn, const float z)
{
    Points all_pts;
    its_collect_mesh_projection_points_above(its, transform_fn, z, all_pts);
    return Geometry::convex_hull(std::move(all_pts));
}

Polygon its_convex_hull_2d_above(const indexed_triangle_set &its, const Matrix3f &m, const float z)
{
    return its_convex_hull_2d_above(its, [m](const Vec3f &p){ return m * p; }, z);
}

Polygon its_convex_hull_2d_above(const indexed_triangle_set &its, const Transform3f &t, const float z)
{
    return its_convex_hull_2d_above(its, [t](const Vec3f &p){ return t * p; }, z);
}

// Generate the vertex list for a cube solid of arbitrary size in X/Y/Z.
indexed_triangle_set its_make_cube(double xd, double yd, double zd)
{
    auto x = float(xd), y = float(yd), z = float(zd);
    return {
        { {0, 1, 2}, {0, 2, 3}, {4, 5, 6}, {4, 6, 7},
          {0, 4, 7}, {0, 7, 1}, {1, 7, 6}, {1, 6, 2},
          {2, 6, 5}, {2, 5, 3}, {4, 0, 3}, {4, 3, 5} },
        { {x, y, 0}, {x, 0, 0}, {0, 0, 0}, {0, y, 0},
          {x, y, z}, {0, y, z}, {0, 0, z}, {x, 0, z} }
    };
}

indexed_triangle_set its_make_prism(float width, float length, float height)
{
    // 我们需要两个朝上的三角形
    float x = width / 2.f, y = length / 2.f;
    return {
        {
            {0, 1, 2}, // side 1
            {4, 3, 5}, // side 2
            {1, 4, 2}, {2, 4, 5}, // roof 1
            {0, 2, 5}, {0, 5, 3}, // roof 2
            {3, 4, 1}, {3, 1, 0} // bottom
        },
        {
            {-x, -y, 0.f}, {x, -y, 0.f}, {0.f, -y, height},
            {-x, y, 0.f}, {x, y, 0.f}, {0.f, y, height},
        }
    };
}

// 生成圆柱体网格并返回，使用生成的角度计算顶部网格三角形。
// 默认是360边，角度 fa 以弧度为单位。
indexed_triangle_set its_make_cylinder(double r, double h, double fa)
{
    indexed_triangle_set mesh;
    size_t n_steps    = (size_t)ceil(2. * PI / fa);
    double angle_step = 2. * PI / n_steps;

    auto &vertices = mesh.vertices;
    auto &facets   = mesh.indices;
    vertices.reserve(2 * n_steps + 2);
    facets.reserve(4 * n_steps);

    // 2个特殊顶点，顶部和底部中心，其余相对于此
    vertices.emplace_back(Vec3f(0.f, 0.f, 0.f));
    vertices.emplace_back(Vec3f(0.f, 0.f, float(h)));

    // 对于沿着近似圆顶部/底部的多边形的每条线，
    // 生成四个点和四个面片（2个用于壁面，2个用于顶部和底部）。
    // 特殊情况：最后一行与第一行共享2个顶点。
    Vec2f p = Eigen::Rotation2Df(0.f) * Eigen::Vector2f(0, r);
    vertices.emplace_back(Vec3f(p(0), p(1), 0.f));
    vertices.emplace_back(Vec3f(p(0), p(1), float(h)));
    for (size_t i = 1; i < n_steps; ++i) {
        p = Eigen::Rotation2Df(angle_step * i) * Eigen::Vector2f(0, float(r));
        vertices.emplace_back(Vec3f(p(0), p(1), 0.f));
        vertices.emplace_back(Vec3f(p(0), p(1), float(h)));
        int id = (int)vertices.size() - 1;
        facets.emplace_back( 0, id - 1, id - 3); // top
        facets.emplace_back(id,      1, id - 2); // bottom
        facets.emplace_back(id, id - 2, id - 3); // upper-right of side
        facets.emplace_back(id, id - 3, id - 1); // bottom-left of side
    }
    // Connect the last set of vertices with the first.
    int id = (int)vertices.size() - 1;
    facets.emplace_back( 0, 2, id - 1);
    facets.emplace_back( 3, 1,     id);
    facets.emplace_back(id, 2,      3);
    facets.emplace_back(id, id - 1, 2);

    return mesh;
}

indexed_triangle_set its_make_frustum(double r, double h, double fa)
{
    indexed_triangle_set mesh;
    size_t n_steps    = (size_t)ceil(2. * PI / fa);
    double angle_step = 2. * PI / n_steps;

    auto &vertices = mesh.vertices;
    auto &facets   = mesh.indices;
    vertices.reserve(2 * n_steps + 2);
    facets.reserve(4 * n_steps);

    // 2个特殊顶点，顶部和底部中心，其余相对于此
    vertices.emplace_back(Vec3f(0.f, 0.f, 0.f));
    vertices.emplace_back(Vec3f(0.f, 0.f, float(h)));

    // 对于沿着近似圆顶部/底部的多边形的每条线，
    // 生成四个点和四个面片（2个用于壁面，2个用于顶部和底部）。
    // 特殊情况：最后一行与第一行共享2个顶点。
    Vec2f vec_top = Eigen::Rotation2Df(0.f) * Eigen::Vector2f(0, 0.5f*r);
    Vec2f vec_botton = Eigen::Rotation2Df(0.f) * Eigen::Vector2f(0, r);

    vertices.emplace_back(Vec3f(vec_botton(0), vec_botton(1), 0.f));
    vertices.emplace_back(Vec3f(vec_top(0), vec_top(1), float(h)));
    for (size_t i = 1; i < n_steps; ++i) {
        vec_top = Eigen::Rotation2Df(angle_step * i) * Eigen::Vector2f(0, 0.5f*float(r));
        vec_botton = Eigen::Rotation2Df(angle_step * i) * Eigen::Vector2f(0, float(r));
        vertices.emplace_back(Vec3f(vec_botton(0), vec_botton(1), 0.f));
        vertices.emplace_back(Vec3f(vec_top(0), vec_top(1), float(h)));
        int id = (int)vertices.size() - 1;
        facets.emplace_back( 0, id - 1, id - 3); // top
        facets.emplace_back(id,      1, id - 2); // bottom
        facets.emplace_back(id, id - 2, id - 3); // upper-right of side
        facets.emplace_back(id, id - 3, id - 1); // bottom-left of side
    }
    // Connect the last set of vertices with the first.
    int id = (int)vertices.size() - 1;
    facets.emplace_back( 0, 2, id - 1);
    facets.emplace_back( 3, 1,     id);
    facets.emplace_back(id, 2,      3);
    facets.emplace_back(id, id - 1, 2);

    return mesh;
}

// 生成环形网格
// r: 主半径（管中心到环中心的距离）
// h: 次半径（管的半径）
// fa: 角度步长（弧度）（越小 = 段越多）
indexed_triangle_set its_make_torus(double r, double h, double fa)
{
    indexed_triangle_set mesh;
    auto& vertices = mesh.vertices;
    auto& facets = mesh.indices;

    // 主环和管周围的段数
    size_t n_major = (size_t)ceil(2. * PI / fa);
    size_t n_minor = (size_t)ceil(2. * PI / fa);

    double major_step = 2. * PI / n_major;
    double minor_step = 2. * PI / n_minor;

    // 为性能预留内存
    vertices.reserve(n_major * n_minor);
    facets.reserve(n_major * n_minor * 2);

    // 生成顶点
    for (size_t i = 0; i < n_major; ++i) {
        double major_angle = i * major_step;
        double cos_major = cos(major_angle);
        double sin_major = sin(major_angle);
        for (size_t j = 0; j < n_minor; ++j) {
            double minor_angle = j * minor_step;
            double cos_minor = cos(minor_angle);
            double sin_minor = sin(minor_angle);
            // 环面的参数方程
            float x = float((r + h * cos_minor) * cos_major);
            float y = float((r + h * cos_minor) * sin_major);
            float z = float(h * sin_minor);
            vertices.emplace_back(Vec3f(x, y, z));
        }
    }

    // 生成面
    for (size_t i = 0; i < n_major; ++i) {
        size_t inext = (i + 1) % n_major;
        for (size_t j = 0; j < n_minor; ++j) {
            size_t jnext = (j + 1) % n_minor;
            int v0 = int(i * n_minor + j);
            int v1 = int(inext * n_minor + j);
            int v2 = int(inext * n_minor + jnext);
            int v3 = int(i * n_minor + jnext);
            // 每个四边形两个三角形
            facets.emplace_back(v0, v1, v2);
            facets.emplace_back(v0, v2, v3);
        }
    }

    return mesh;
}

indexed_triangle_set its_make_cone(double r, double h, double fa)
{
    indexed_triangle_set mesh;
    auto& vertices = mesh.vertices;
    auto& facets = mesh.indices;
    vertices.reserve(3 + 2 * size_t(2 * PI / fa));

    // 底部中心和顶部顶点
    vertices.emplace_back(Vec3f::Zero());
    vertices.emplace_back(Vec3f(0., 0., h));

    size_t i = 0;
    for (double angle=0; angle<2*PI; angle+=fa) {
        vertices.emplace_back(r*std::cos(angle), r*std::sin(angle), 0.);
        if (angle > 0.) {
            facets.emplace_back(0, i+2, i+1);
            facets.emplace_back(1, i+1, i+2);
        }
        ++i;
    }
    facets.emplace_back(0, 2, i+1); // 闭合形状
    facets.emplace_back(1, i+1, 2);

    return mesh;
}

indexed_triangle_set its_make_pyramid(float base, float height)
{
    float a = base / 2.f;
    return {
        {
            {0, 1, 2},
            {0, 2, 3},
            {0, 1, 4},
            {1, 2, 4},
            {2, 3, 4},
            {3, 0, 4}
        },
        {
            {-a, -a, 0}, {a, -a, 0}, {a, a, 0},
            {-a, a, 0}, {0.f, 0.f, height}
        }
    };
}

// 生成以原点为中心的球体网格，使用生成的角度确定粒度。
// 默认角度为1度。
//FIXME 最好递归离散化二十面体 http://www.songho.ca/opengl/gl_sphere.html
indexed_triangle_set its_make_sphere(double radius, double fa)
{
    int   sectorCount = int(ceil(2. * M_PI / fa));
    int   stackCount  = int(ceil(M_PI / fa));
    float sectorStep  = float(2. * M_PI / sectorCount);
    float stackStep   = float(M_PI / stackCount);

    indexed_triangle_set mesh;
    auto& vertices = mesh.vertices;
    vertices.reserve((stackCount - 1) * sectorCount + 2);
    for (int i = 0; i <= stackCount; ++ i) {
        // 从 pi/2 到 -pi/2
        double stackAngle = 0.5 * M_PI - stackStep * i;
        double xy = radius * cos(stackAngle);
        double z  = radius * sin(stackAngle);
        if (i == 0 || i == stackCount)
            vertices.emplace_back(Vec3f(float(xy), 0.f, float(z)));
        else
            for (int j = 0; j < sectorCount; ++ j) {
                // 从 0 到 2pi
                double sectorAngle = sectorStep * j;
                vertices.emplace_back(Vec3d(xy * std::cos(sectorAngle), xy * std::sin(sectorAngle), z).cast<float>());
            }
    }

    auto& facets = mesh.indices;
    facets.reserve(2 * (stackCount - 1) * sectorCount);
    for (int i = 0; i < stackCount; ++ i) {
        // 当前堆栈的开始。
        int k1 = (i == 0) ? 0 : (1 + (i - 1) * sectorCount);
        int k1_first = k1;
        // 下一个堆栈的开始。
        int k2 = (i == 0) ? 1 : (k1 + sectorCount);
        int k2_first = k2;
        for (int j = 0; j < sectorCount; ++ j) {
            // 2 triangles per sector excluding first and last stacks
            int k1_next = k1;
            int k2_next = k2;
            if (i != 0) {
                k1_next = (j + 1 == sectorCount) ? k1_first : (k1 + 1);
                facets.emplace_back(k1, k2, k1_next);
            }
            if (i + 1 != stackCount) {
                k2_next = (j + 1 == sectorCount) ? k2_first : (k2 + 1);
                facets.emplace_back(k1_next, k2, k2_next);
            }
            k1 = k1_next;
            k2 = k2_next;
        }
    }

    return mesh;
}

// Generates mesh for a frustum dowel centered about the origin, using the count of sectors
// Note: This function uses code for sphere generation, but for stackCount = 2;
indexed_triangle_set its_make_frustum_dowel(double radius, double h, int sectorCount)
{
    int   stackCount = 2;
    float sectorStep = float(2. * M_PI / sectorCount);
    float stackStep = float(M_PI / stackCount);

    indexed_triangle_set mesh;
    auto& vertices = mesh.vertices;
    vertices.reserve((stackCount - 1) * sectorCount + 2);
    for (int i = 0; i <= stackCount; ++i) {
        // 从 pi/2 到 -pi/2
        double stackAngle = 0.5 * M_PI - stackStep * i;
        double xy = radius * cos(stackAngle);
        double z = radius * sin(stackAngle);
        if (i == 0 || i == stackCount)
            vertices.emplace_back(Vec3f(float(xy), 0.f, float(h * sin(stackAngle))));
        else
            for (int j = 0; j < sectorCount; ++j) {
                // 从 0 到 2pi
                double sectorAngle = sectorStep * j + 0.25 * M_PI;
                vertices.emplace_back(Vec3d(xy * std::cos(sectorAngle), xy * std::sin(sectorAngle), z).cast<float>());
            }
    }

    auto& facets = mesh.indices;
    facets.reserve(2 * (stackCount - 1) * sectorCount);
    for (int i = 0; i < stackCount; ++i) {
        // 当前堆栈的开始。
        int k1 = (i == 0) ? 0 : (1 + (i - 1) * sectorCount);
        int k1_first = k1;
        // 下一个堆栈的开始。
        int k2 = (i == 0) ? 1 : (k1 + sectorCount);
        int k2_first = k2;
        for (int j = 0; j < sectorCount; ++j) {
            // 2 triangles per sector excluding first and last stacks
            int k1_next = k1;
            int k2_next = k2;
            if (i != 0) {
                k1_next = (j + 1 == sectorCount) ? k1_first : (k1 + 1);
                facets.emplace_back(k1, k2, k1_next);
            }
            if (i + 1 != stackCount) {
                k2_next = (j + 1 == sectorCount) ? k2_first : (k2 + 1);
                facets.emplace_back(k1_next, k2, k2_next);
            }
            k1 = k1_next;
            k2 = k2_next;
        }
    }

    return mesh;
}

indexed_triangle_set its_make_snap(double r, double h, float space_proportion, float bulge_proportion)
{
    const float radius = (float)r;
    const float height = (float)h;
    const size_t sectors_cnt = 10; //(float)fa;
    const float halfPI = 0.5f * (float)PI;

    const float space_len = space_proportion * radius;

    const float b_len = radius;
    const float m_len = (1 + bulge_proportion) * radius;
    const float t_len = 0.5f * radius;

    const float b_height = 0.f;
    const float m_height = 0.5f * height;
    const float t_height = height;

    const float b_angle = acos(space_len/b_len);
    const float t_angle = acos(space_len/t_len);

    const float b_angle_step = b_angle / (float)sectors_cnt;
    const float t_angle_step = t_angle / (float)sectors_cnt;

    const Vec2f b_vec = Eigen::Vector2f(0, b_len);
    const Vec2f t_vec = Eigen::Vector2f(0, t_len);


    auto add_side_vertices = [b_vec, t_vec, b_height, m_height, t_height](std::vector<stl_vertex>& vertices, float b_angle, float t_angle, const Vec2f& m_vec) {
        Vec2f b_pt = Eigen::Rotation2Df(b_angle) * b_vec;
        Vec2f m_pt = Eigen::Rotation2Df(b_angle) * m_vec;
        Vec2f t_pt = Eigen::Rotation2Df(t_angle) * t_vec;

        vertices.emplace_back(Vec3f(b_pt(0), b_pt(1), b_height));
        vertices.emplace_back(Vec3f(m_pt(0), m_pt(1), m_height));
        vertices.emplace_back(Vec3f(t_pt(0), t_pt(1), t_height));
    };

    auto add_side_facets = [](std::vector<stl_triangle_vertex_indices>& facets, int vertices_cnt, int frst_id, int scnd_id) {
        int id = vertices_cnt - 1;

        facets.emplace_back(frst_id, id - 2, id - 5);

        facets.emplace_back(id - 2, id - 1, id - 5);
        facets.emplace_back(id - 1, id - 4, id - 5);
        facets.emplace_back(id - 4, id - 1, id);
        facets.emplace_back(id, id - 3, id - 4);

        facets.emplace_back(id, scnd_id, id - 3);
    };

    const float f = (b_len - m_len) / m_len; // Flattening

    auto get_m_len = [b_len, f](float angle) {
        const float rad_sqr = b_len * b_len;
        const float sin_sqr = sin(angle) * sin(angle);
        const float f_sqr = (1-f)*(1-f);
        return sqrtf(rad_sqr / (1 + (1 / f_sqr - 1) * sin_sqr));
    };

    auto add_sub_mesh = [add_side_vertices, add_side_facets, get_m_len,
                        b_height, t_height, b_angle, t_angle, b_angle_step, t_angle_step]
                        (indexed_triangle_set& mesh, float center_x, float angle_rotation, int frst_vertex_id) {
        auto& vertices = mesh.vertices;
        auto& facets     = mesh.indices;

        // 2个特殊顶点，顶部和底部中心，其余相对于此
        vertices.emplace_back(Vec3f(center_x, 0.f, b_height));
        vertices.emplace_back(Vec3f(center_x, 0.f, t_height));

        float b_angle_start = angle_rotation - b_angle;
        float t_angle_start = angle_rotation - t_angle;
        const float b_angle_stop  = angle_rotation + b_angle;

        const int frst_id = frst_vertex_id;
        const int scnd_id = frst_id + 1;

        // add first side vertices and internal facets
        {
            const Vec2f m_vec = Eigen::Vector2f(0, get_m_len(b_angle_start));
            add_side_vertices(vertices, b_angle_start, t_angle_start, m_vec);

            int id = (int)vertices.size() - 1;

            facets.emplace_back(frst_id, id - 2, id - 1);
            facets.emplace_back(frst_id, id - 1, id);
            facets.emplace_back(frst_id, id, scnd_id);
        }

        // add d side vertices and facets
        while (!is_approx(b_angle_start, b_angle_stop)) {
            b_angle_start += b_angle_step;
            t_angle_start += t_angle_step;

            const Vec2f m_vec = Eigen::Vector2f(0, get_m_len(b_angle_start));
            add_side_vertices(vertices, b_angle_start, t_angle_start, m_vec);

            add_side_facets(facets, (int)vertices.size(), frst_id, scnd_id);
        }

        // add last internal facets to close the mesh
        {
            int id = (int)vertices.size() - 1;

            facets.emplace_back(frst_id, scnd_id, id);
            facets.emplace_back(frst_id, id, id - 1);
            facets.emplace_back(frst_id, id - 1, id - 2);
        }
    };


    indexed_triangle_set mesh;

    mesh.vertices.reserve(2 * (3 * (2 * sectors_cnt + 1) + 2));
    mesh.indices.reserve(2 * (6 * 2 * sectors_cnt + 6));

    add_sub_mesh(mesh, -space_len, halfPI    , 0);
    add_sub_mesh(mesh,  space_len, 3 * halfPI, (int)mesh.vertices.size());

    return mesh;
}

indexed_triangle_set its_convex_hull(const std::vector<Vec3f> &pts)
{
    std::vector<Vec3f>  dst_vertices;
    std::vector<Vec3i32>  dst_facets;

    if (! pts.empty()) {
        // The qhull call:
        orgQhull::Qhull qhull;
        qhull.disableOutputStream(); // we want qhull to be quiet
    #if ! REALfloat
        std::vector<realT> src_vertices;
    #endif
        try {
    #if REALfloat
            qhull.runQhull("", 3, (int)pts.size(), (const realT*)(pts.front().data()), "Qt");
    #else
            src_vertices.reserve(pts.size() * 3);
            // We will now fill the vector with input points for computation:
            for (const stl_vertex &v : pts)
                for (int i = 0; i < 3; ++ i)
                    src_vertices.emplace_back(v(i));
            qhull.runQhull("", 3, (int)src_vertices.size() / 3, src_vertices.data(), "Qt");
    #endif
        } catch (...) {
            BOOST_LOG_TRIVIAL(error) << "its_convex_hull: Unable to create convex hull";
            return {};
        }

        // Let's collect results:
        // Map of QHull's vertex ID to our own vertex ID (pointing to dst_vertices).
        std::vector<int>    map_dst_vertices;
    #ifndef NDEBUG
        Vec3f               centroid = Vec3f::Zero();
        for (const stl_vertex& pt : pts)
            centroid += pt;
        centroid /= float(pts.size());
    #endif // NDEBUG
        for (const orgQhull::QhullFacet &facet : qhull.facetList()) {
            // Collect face vertices first, allocate unique vertices in dst_vertices based on QHull's vertex ID.
            Vec3i32  indices;
            int    cnt = 0;
            for (const orgQhull::QhullVertex vertex : facet.vertices()) {
                int id = vertex.id();
                assert(id >= 0);
                if (id >= int(map_dst_vertices.size()))
                    map_dst_vertices.resize(next_highest_power_of_2(size_t(id + 1)), -1);
                if (int i = map_dst_vertices[id]; i == -1) {
                    // Allocate a new vertex.
                    i = int(dst_vertices.size());
                    map_dst_vertices[id] = i;
                    orgQhull::QhullPoint pt(vertex.point());
                    dst_vertices.emplace_back(pt[0], pt[1], pt[2]);
                    indices[cnt] = i;
                } else {
                    // Reuse existing vertex.
                    indices[cnt] = i;
                }
                if (cnt ++ == 3)
                    break;
            }
            assert(cnt == 3);
            if (cnt == 3) {
                // QHull sorts vertices of a face lexicographically by their IDs, not by face normals.
                // Calculate face normal based on the order of vertices.
                Vec3f n  = (dst_vertices[indices(1)] - dst_vertices[indices(0)]).cross(dst_vertices[indices(2)] - dst_vertices[indices(1)]);
                auto *n2 = facet.getBaseT()->normal;
                auto  d = n.x() * n2[0] + n.y() * n2[1] + n.z() * n2[2];
    #ifndef NDEBUG
                Vec3f n3 = (dst_vertices[indices(0)] - centroid);
                auto  d3 = n.dot(n3);
                assert((d < 0.f) == (d3 < 0.f));
    #endif // NDEBUG
                // Get the face normal from QHull.
                if (d < 0.f)
                    // Fix face orientation.
                    std::swap(indices[1], indices[2]);
                dst_facets.emplace_back(indices);
            }
        }
    }

    return { std::move(dst_facets), std::move(dst_vertices) };
}

void its_reverse_all_facets(indexed_triangle_set &its)
{
    for (stl_triangle_vertex_indices &face : its.indices)
        std::swap(face[0], face[1]);
}

void its_merge(indexed_triangle_set &A, const indexed_triangle_set &B)
{
    auto N   = int(A.vertices.size());
    auto N_f = A.indices.size();

    A.vertices.insert(A.vertices.end(), B.vertices.begin(), B.vertices.end());
    A.indices.insert(A.indices.end(), B.indices.begin(), B.indices.end());

    for(size_t n = N_f; n < A.indices.size(); n++)
        A.indices[n] += Vec3i32{N, N, N};
}

void its_merge(indexed_triangle_set &A, const std::vector<Vec3f> &triangles)
{
    const size_t offs = A.vertices.size();
    A.vertices.insert(A.vertices.end(), triangles.begin(), triangles.end());
    A.indices.reserve(A.indices.size() + A.vertices.size() / 3);

    for(int i = int(offs); i < int(A.vertices.size()); i += 3)
        A.indices.emplace_back(i, i + 1, i + 2);
}

void its_merge(indexed_triangle_set &A, const Pointf3s &triangles)
{
    auto trianglesf = reserve_vector<Vec3f> (triangles.size());
    for (auto &t : triangles)
        trianglesf.emplace_back(t.cast<float>());

    its_merge(A, trianglesf);
}

float its_volume(const indexed_triangle_set &its)
{
    if (its.empty()) return 0.;

    // Choose a point, any point as the reference.
    auto p0 = its.vertices.front();
    float volume = 0.f;
    for (size_t i = 0; i < its.indices.size(); ++ i) {
        // Do dot product to get distance from point to plane.
        its_triangle triangle = its_triangle_vertices(its, i);
        Vec3f U = triangle[1] - triangle[0];
        Vec3f V = triangle[2] - triangle[0];
        Vec3f C = U.cross(V);
        Vec3f normal = C.normalized();
        float area = 0.5 * C.norm();
        float height = normal.dot(triangle[0] - p0);
        volume += (area * height) / 3.0f;
    }

    return volume;
}

float its_average_edge_length(const indexed_triangle_set &its)
{
    if (its.indices.empty())
        return 0.f;

    double edge_length = 0.f;
    for (size_t i = 0; i < its.indices.size(); ++ i) {
        const its_triangle v = its_triangle_vertices(its, i);
        edge_length += (v[1] - v[0]).cast<double>().norm() + 
                       (v[2] - v[0]).cast<double>().norm() +
                       (v[1] - v[2]).cast<double>().norm();
    }
    return float(edge_length / (3 * its.indices.size()));
}

std::vector<indexed_triangle_set> its_split(const indexed_triangle_set &its)
{
    return its_split<>(its);
}

// Number of disconnected patches (faces are connected if they share an edge, shared edge defined with 2 shared vertex indices).
size_t its_number_of_patches(const indexed_triangle_set &its)
{
    return its_number_of_patches<>(its);
}
size_t its_number_of_patches(const indexed_triangle_set &its, const std::vector<Vec3i32> &face_neighbors)
{
    return its_number_of_patches<>(ItsNeighborsWrapper{ its, face_neighbors });
}

// Same as its_number_of_patches(its) > 1, but faster.
bool its_is_splittable(const indexed_triangle_set &its)
{
    return its_is_splittable<>(its);
}
bool its_is_splittable(const indexed_triangle_set &its, const std::vector<Vec3i32> &face_neighbors)
{
    return its_is_splittable<>(ItsNeighborsWrapper{ its, face_neighbors });
}

size_t its_num_open_edges(const std::vector<Vec3i32> &face_neighbors)
{
    size_t num_open_edges = 0;
    for (Vec3i32 neighbors : face_neighbors)
        for (int n : neighbors)
            if (n < 0)
                ++ num_open_edges;
    return num_open_edges;
}

size_t its_num_open_edges(const indexed_triangle_set &its)
{
    return its_num_open_edges(its_face_neighbors(its));
}

void VertexFaceIndex::create(const indexed_triangle_set &its)
{
    m_vertex_to_face_start.assign(its.vertices.size() + 1, 0);
    // 1) Calculate vertex incidence by scatter.
    for (auto &face : its.indices) {
        ++ m_vertex_to_face_start[face(0) + 1];
        ++ m_vertex_to_face_start[face(1) + 1];
        ++ m_vertex_to_face_start[face(2) + 1];
    }
    // 2) Prefix sum to calculate offsets to m_vertex_faces_all.
    for (size_t i = 2; i < m_vertex_to_face_start.size(); ++ i)
        m_vertex_to_face_start[i] += m_vertex_to_face_start[i - 1];
    // 3) Scatter indices of faces incident to a vertex into m_vertex_faces_all.
    m_vertex_faces_all.assign(m_vertex_to_face_start.back(), 0);
    for (size_t face_idx = 0; face_idx < its.indices.size(); ++ face_idx) {
        auto &face = its.indices[face_idx];
        for (int i = 0; i < 3; ++ i)
            m_vertex_faces_all[m_vertex_to_face_start[face(i)] ++] = face_idx;
    }
    // 4) The previous loop modified m_vertex_to_face_start. Revert the change.
    for (auto i = int(m_vertex_to_face_start.size()) - 1; i > 0; -- i)
        m_vertex_to_face_start[i] = m_vertex_to_face_start[i - 1];
    m_vertex_to_face_start.front() = 0;
}

std::vector<Vec3i32> its_face_neighbors(const indexed_triangle_set &its)
{
    return create_face_neighbors_index(ex_seq, its);
}

std::vector<Vec3i32> its_face_neighbors_par(const indexed_triangle_set &its)
{
    return create_face_neighbors_index(ex_tbb, its);
}

std::vector<Vec3f> its_face_normals(const indexed_triangle_set &its) 
{
    std::vector<Vec3f> normals;
    normals.reserve(its.indices.size());
    for (stl_triangle_vertex_indices face : its.indices)
        normals.push_back(its_face_normal(its, face));
    return normals;
}

#if BOOST_ENDIAN_LITTLE_BYTE
static inline void big_endian_reverse_quads(char*, size_t) {}
#else // BOOST_ENDIAN_LITTLE_BYTE
static inline void big_endian_reverse_quads(char *buf, size_t cnt)
{
    for (size_t i = 0; i < cnt; i += 4) {
        std::swap(buf[i], buf[i+3]);
        std::swap(buf[i+1], buf[i+2]);
    }
}
#endif // BOOST_ENDIAN_LITTLE_BYTE

bool its_write_stl_ascii(const char *file, const char *label, const std::vector<stl_triangle_vertex_indices> &indices, const std::vector<stl_vertex> &vertices)
{
    FILE *fp = boost::nowide::fopen(file, "w");
    if (fp == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "its_write_stl_ascii: Couldn't open " << file << " for writing";
        return false;
    }

    fprintf(fp, "solid  %s\n", label);

    for (const stl_triangle_vertex_indices& face : indices) {
        Vec3f vertex[3] = { vertices[face(0)], vertices[face(1)], vertices[face(2)] };
        Vec3f normal    = (vertex[1] - vertex[0]).cross(vertex[2] - vertex[1]).normalized();
        fprintf(fp, "  facet normal % .8E % .8E % .8E\n", normal(0), normal(1), normal(2));
        fprintf(fp, "    outer loop\n");
        fprintf(fp, "      vertex % .8E % .8E % .8E\n", vertex[0](0), vertex[0](1), vertex[0](2));
        fprintf(fp, "      vertex % .8E % .8E % .8E\n", vertex[1](0), vertex[1](1), vertex[1](2));
        fprintf(fp, "      vertex % .8E % .8E % .8E\n", vertex[2](0), vertex[2](1), vertex[2](2));
        fprintf(fp, "    endloop\n");
        fprintf(fp, "  endfacet\n");
    }

    fprintf(fp, "endsolid  %s\n", label);
    fclose(fp);
    return true;
}

bool its_write_stl_binary(const char *file, const char *label, const std::vector<stl_triangle_vertex_indices> &indices, const std::vector<stl_vertex> &vertices)
{
    FILE *fp = boost::nowide::fopen(file, "wb");
    if (fp == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "its_write_stl_binary: Couldn't open " << file << " for writing";
        return false;
    }

    {
        static constexpr const int header_size = 80;
        std::vector<char> header(header_size, 0);
        if (int header_len = std::min((label == nullptr) ? 0 : int(strlen(label)), header_size); header_len > 0)
            ::memcpy(header.data(), label, header_len);
        ::fwrite(header.data(), header_size, 1, fp);
    }

    uint32_t nfaces = indices.size();
    big_endian_reverse_quads(reinterpret_cast<char*>(&nfaces), 4);
    ::fwrite(&nfaces, 4, 1, fp);

    stl_facet f;
    f.extra[0] = 0;
    f.extra[1] = 0;
    for (const stl_triangle_vertex_indices& face : indices) {
        f.vertex[0] = vertices[face(0)];
        f.vertex[1] = vertices[face(1)];
        f.vertex[2] = vertices[face(2)];
        f.normal = (f.vertex[1] - f.vertex[0]).cross(f.vertex[2] - f.vertex[1]).normalized();
        big_endian_reverse_quads(reinterpret_cast<char*>(&f), 48);
        fwrite(&f, 50, 1, fp);
    }

    fclose(fp);
    return true;
}

} // namespace Slic3r
