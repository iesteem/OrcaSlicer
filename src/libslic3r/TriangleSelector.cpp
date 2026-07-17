#include "TriangleSelector.hpp"
#include "Model.hpp"

#include <boost/container/small_vector.hpp>
#include <boost/log/trivial.hpp>
#include <cstddef>
#include <tbb/parallel_for.h>

#ifndef NDEBUG
//    #define EXPENSIVE_DEBUG_CHECKS
#endif // NDEBUG

namespace Slic3r {

// 检查线段是否完全在球体内，或部分在球体内（与球体相交）。
// 灵感来自 Christer Ericson 的《实时碰撞检测》第177-179页。
static bool test_line_inside_sphere(const Vec3f &line_a, const Vec3f &line_b, const Vec3f &sphere_p, const float sphere_radius)
{
    const float sphere_radius_sqr = Slic3r::sqr(sphere_radius);
    const Vec3f line_dir          = line_b - line_a;   // n
    const Vec3f origins_diff      = line_a - sphere_p; // m

    const float m_dot_m           = origins_diff.dot(origins_diff);
    // 检查线段的任一端点是否在球体内。
    if (m_dot_m <= sphere_radius_sqr || (line_b - sphere_p).squaredNorm() <= sphere_radius_sqr)
        return true;

    // 检查无限直线是否穿过球体。
    const float n_dot_n = line_dir.dot(line_dir);
    const float m_dot_n = origins_diff.dot(line_dir);

    const float eq_a    = n_dot_n;
    const float eq_b    = m_dot_n;
    const float eq_c    = m_dot_m - sphere_radius_sqr;

    const float discr = eq_b * eq_b - eq_a * eq_c;
    // 负判别式表示无限直线不穿过球体。
    if (discr < 0.f)
        return false;

    // 检查有限线段是否穿过球体。
    const float discr_sqrt = std::sqrt(discr);
    const float t1         = (-eq_b - discr_sqrt) / eq_a;
    if (0.f <= t1 && t1 <= 1.f)
        return true;

    const float t2 = (-eq_b + discr_sqrt) / eq_a;
    if (0.f <= t2 && t2 <= 1.f && discr_sqrt > 0.f)
        return true;

    return false;
}

// 检查线段是否完全在有限圆柱体内，或部分在圆柱体内（与圆柱体相交）。
// 灵感来自 Christer Ericson 的《实时碰撞检测》第194-198页。
static bool test_line_inside_cylinder(const Vec3f &line_a, const Vec3f &line_b, const Vec3f &cylinder_P, const Vec3f &cylinder_Q, const float cylinder_radius)
{
    assert(cylinder_P != cylinder_Q);
    const Vec3f cylinder_dir                    = cylinder_Q - cylinder_P; // d
    auto        is_point_inside_finite_cylinder = [&cylinder_P, &cylinder_Q, &cylinder_radius, &cylinder_dir](const Vec3f &pt) {
        const Vec3f first_center_diff  = cylinder_P - pt;
        const Vec3f second_center_diff = cylinder_Q - pt;
        // 首先，检查点pt是否位于由cylinder_p和cylinder_q定义的平面之间。
        // 然后检查它是否在cylinder_p和cylinder_q之间的圆柱体内。
        return first_center_diff.dot(cylinder_dir) <= 0 && second_center_diff.dot(cylinder_dir) >= 0 &&
               (first_center_diff.cross(cylinder_dir).norm() / cylinder_dir.norm()) <= cylinder_radius;
    };

    // 检查线段的任一端点是否在圆柱体内。
    if (is_point_inside_finite_cylinder(line_a) || is_point_inside_finite_cylinder(line_b))
       return true;

    // 检查线段是否穿过圆柱体。
    const Vec3f origins_diff = line_a - cylinder_P;     // m
    const Vec3f line_dir     = line_b - line_a;         // n

    const float m_dot_d = origins_diff.dot(cylinder_dir);
    const float n_dot_d = line_dir.dot(cylinder_dir);
    const float d_dot_d = cylinder_dir.dot(cylinder_dir);

    const float n_dot_n = line_dir.dot(line_dir);
    const float m_dot_n = origins_diff.dot(line_dir);
    const float m_dot_m = origins_diff.dot(origins_diff);

    const float eq_a    = d_dot_d * n_dot_n - n_dot_d * n_dot_d;
    const float eq_b    = d_dot_d * m_dot_n - n_dot_d * m_dot_d;
    const float eq_c    = d_dot_d * (m_dot_m - Slic3r::sqr(cylinder_radius)) - m_dot_d * m_dot_d;

    const float discr   = eq_b * eq_b - eq_a * eq_c;
    // 负判别式表示无限直线不穿过无限圆柱体。
    if (discr < 0.0f)
        return false;

    // 检查有限线段是否穿过有限圆柱体。
    const float discr_sqrt = std::sqrt(discr);
    const float t1         = (-eq_b - discr_sqrt) / eq_a;
    if (0.f <= t1 && t1 <= 1.f)
        if (const float cylinder_endcap_t1 = m_dot_d + t1 * n_dot_d; 0.f <= cylinder_endcap_t1 && cylinder_endcap_t1 <= d_dot_d)
            return true;

    const float t2 = (-eq_b + discr_sqrt) / eq_a;
    if (0.f <= t2 && t2 <= 1.f)
        if (const float cylinder_endcap_t2 = (m_dot_d + t2 * n_dot_d); 0.f <= cylinder_endcap_t2 && cylinder_endcap_t2 <= d_dot_d)
            return true;

    return false;
}

// 检查线段是否完全在胶囊体内，或部分在胶囊体内（与胶囊体相交）。
static bool test_line_inside_capsule(const Vec3f &line_a, const Vec3f &line_b, const Vec3f &capsule_p, const Vec3f &capsule_q, const float capsule_radius) {
    assert(capsule_p != capsule_q);

    // 检查线段是否与构成胶囊体的任一球体相交。
    if (test_line_inside_sphere(line_a, line_b, capsule_p, capsule_radius) || test_line_inside_sphere(line_a, line_b, capsule_q, capsule_radius))
        return true;

    // 检查线段是否与球体中心之间的圆柱体相交。
    return test_line_inside_cylinder(line_a, line_b, capsule_p, capsule_q, capsule_radius);
}

#ifndef NDEBUG
bool TriangleSelector::verify_triangle_midpoints(const Triangle &tr) const
{
    for (int i = 0; i < 3; ++ i) {
        int v1   = tr.verts_idxs[i];
        int v2   = tr.verts_idxs[next_idx_modulo(i, 3)];
        int vmid = this->triangle_midpoint(tr, v1, v2);
        assert(vmid >= -1);
        if (vmid != -1) {
            Vec3f c1 = 0.5f * (m_vertices[v1].v + m_vertices[v2].v);
            Vec3f c2 = m_vertices[vmid].v;
            float d  = (c2 - c1).norm();
            assert(std::abs(d) < EPSILON);
        }
    }
    return true;
}

bool TriangleSelector::verify_triangle_neighbors(const Triangle &tr, const Vec3i32 &neighbors) const
{
    assert(neighbors(0) >= -1);
    assert(neighbors(1) >= -1);
    assert(neighbors(2) >= -1);
    assert(verify_triangle_midpoints(tr));

    for (int i = 0; i < 3; ++i)
        if (neighbors(i) != -1) {
            const Triangle &tr2 = m_triangles[neighbors(i)];
            assert(verify_triangle_midpoints(tr2));
            int v1 = tr.verts_idxs[i];
            int v2 = tr.verts_idxs[next_idx_modulo(i, 3)];
            assert(tr2.verts_idxs[0] == v1 || tr2.verts_idxs[1] == v1 || tr2.verts_idxs[2] == v1);
            int j = tr2.verts_idxs[0] == v1 ? 0 : tr2.verts_idxs[1] == v1 ? 1 : 2;
            assert(tr2.verts_idxs[j] == v1);
            assert(tr2.verts_idxs[prev_idx_modulo(j, 3)] == v2);
        }
    return true;
}
#endif // NDEBUG

// sides_to_split==-1 : 仅恢复之前的拆分
void TriangleSelector::Triangle::set_division(int sides_to_split, int special_side_idx)
{
    assert(sides_to_split >= 0 && sides_to_split <= 3);
    assert(special_side_idx >= 0 && special_side_idx < 3);
    assert(sides_to_split == 1 || sides_to_split == 2 || special_side_idx == 0);
    this->number_of_splits = char(sides_to_split);
    this->special_side_idx = char(special_side_idx);
}

inline bool is_point_inside_triangle(const Vec3f &pt, const Vec3f &p1, const Vec3f &p2, const Vec3f &p3)
{
    // 实时碰撞检测，Ericson，第3.4章
    auto barycentric = [&pt, &p1, &p2, &p3]() -> Vec3f {
        std::array<Vec3f, 3> v     = {p2 - p1, p3 - p1, pt - p1};
        float                d00   = v[0].dot(v[0]);
        float                d01   = v[0].dot(v[1]);
        float                d11   = v[1].dot(v[1]);
        float                d20   = v[2].dot(v[0]);
        float                d21   = v[2].dot(v[1]);
        float                denom = d00 * d11 - d01 * d01;

        Vec3f barycentric_cords(1.f, (d11 * d20 - d01 * d21) / denom, (d00 * d21 - d01 * d20) / denom);
        barycentric_cords.x() = barycentric_cords.x() - barycentric_cords.y() - barycentric_cords.z();
        return barycentric_cords;
    };

    Vec3f barycentric_cords = barycentric();
    return std::all_of(begin(barycentric_cords), end(barycentric_cords), [](float cord) { return 0.f <= cord && cord <= 1.0; });
}

int TriangleSelector::select_unsplit_triangle(const Vec3f &hit, int facet_idx, const Vec3i32 &neighbors) const
{
    assert(facet_idx < int(m_triangles.size()));
    const Triangle *tr = &m_triangles[facet_idx];
    if (!tr->valid())
        return -1;

    if (!tr->is_split()) {
        if (const std::array<int, 3> &t_vert = m_triangles[facet_idx].verts_idxs; is_point_inside_triangle(hit, m_vertices[t_vert[0]].v, m_vertices[t_vert[1]].v, m_vertices[t_vert[2]].v))
            return facet_idx;

        return -1;
    }

    assert(this->verify_triangle_neighbors(*tr, neighbors));

    int num_of_children = tr->number_of_split_sides() + 1;
    if (num_of_children != 1) {
        for (int i = 0; i < num_of_children; ++i) {
            assert(i < int(tr->children.size()));
            assert(tr->children[i] < int(m_triangles.size()));
            // 递归，深度优先搜索该三角形的子三角形。
            // 该三角形的所有子三角形都是通过拆分原始网格的一个源三角形创建的。

            const std::array<int, 3> &t_vert = m_triangles[tr->children[i]].verts_idxs;
            if (is_point_inside_triangle(hit, m_vertices[t_vert[0]].v, m_vertices[t_vert[1]].v, m_vertices[t_vert[2]].v))
                return this->select_unsplit_triangle(hit, tr->children[i], this->child_neighbors(*tr, neighbors, i));
        }
    }

    return -1;
}

int TriangleSelector::select_unsplit_triangle(const Vec3f &hit, int facet_idx) const
{
    assert(facet_idx < int(m_triangles.size()));
    if (!m_triangles[facet_idx].valid())
        return -1;

    Vec3i32 neighbors = m_neighbors[facet_idx];
    assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors));
    return this->select_unsplit_triangle(hit, facet_idx, neighbors);
}

void TriangleSelector::select_patch(int facet_start, std::unique_ptr<Cursor> &&cursor, EnforcerBlockerType new_state, const Transform3d& trafo_no_translate, bool triangle_splitting, float highlight_by_angle_deg)
{
    assert(facet_start < m_orig_size_indices);

    // 保存当前光标中心、半径平方和相机方向，这样我们就不必到处传递它们。
    m_cursor = std::move(cursor);

    // 如果用户自上次以来更改了光标大小，更新三角形边长限制。
    // 必须比较m_cursor中的内部半径！半径在世界坐标中，缩放后不变。
    if (m_old_cursor_radius_sqr != m_cursor->radius_sqr) {
        // BBS: improve details for large cursor radius
        TriangleSelector::HeightRange* hr_cursor = dynamic_cast<TriangleSelector::HeightRange*>(m_cursor.get());
        if (hr_cursor == nullptr) {
            set_edge_limit(std::min(std::sqrt(m_cursor->radius_sqr) / 5.f, 0.05f));
            m_old_cursor_radius_sqr = m_cursor->radius_sqr;
        }
        else {
            set_edge_limit(0.1);
            m_old_cursor_radius_sqr = 0.1;
        }
    }

    const float highlight_angle_limit = -cos(Geometry::deg2rad(highlight_by_angle_deg));

    // BBS
    std::vector<int> start_facets;
    HeightRange* hr_cursor = dynamic_cast<HeightRange*>(m_cursor.get());
    if (hr_cursor) {
        for (int facet_id = 0; facet_id < m_orig_size_indices; facet_id++) {
            const Triangle& tr = m_triangles[facet_id];
            if (m_cursor->is_edge_inside_cursor(tr, m_vertices)) {
                start_facets.push_back(facet_id);
            }
        }
    }
    else {
        start_facets.push_back(facet_start);
    }

    // 跟踪已经处理过的原始网格的面。
    std::vector<bool> visited(m_orig_size_indices, false);

    for (int i = 0; i < start_facets.size(); i++) {
        int start_facet_id = start_facets[i];
        if (visited[start_facet_id])
            continue;

        // 现在从指针指向的面开始，检查所有相邻的面。
        std::vector<int> facets_to_check;
        facets_to_check.reserve(16);
        facets_to_check.emplace_back(start_facet_id);

        // 在命中点周围进行广度优先搜索。facets_to_check可能会变得非常大。
        // 广度优先facets_to_check FIFO的头部。
        int facet_idx = 0;
        while (facet_idx < int(facets_to_check.size())) {
            int          facet = facets_to_check[facet_idx];
            const Vec3f& facet_normal = m_face_normals[m_triangles[facet].source_triangle];
            Matrix3f     normal_matrix = static_cast<Matrix3f>(trafo_no_translate.matrix().block(0, 0, 3, 3).inverse().transpose().cast<float>());
            float        world_normal_z = (normal_matrix* facet_normal).normalized().z();
            if (!visited[facet] && (highlight_by_angle_deg == 0.f || world_normal_z < highlight_angle_limit)) {
                if (select_triangle(facet, new_state, triangle_splitting)) {
                    // 将相邻的面添加到稍后处理的列表中
                    for (int neighbor_idx : m_neighbors[facet])
                        if (neighbor_idx >= 0 && m_cursor->is_facet_visible(neighbor_idx, m_face_normals))
                            facets_to_check.push_back(neighbor_idx);
                }
            }
            visited[facet] = true;
            ++facet_idx;
        }
    }
}

bool TriangleSelector::is_facet_clipped(int facet_idx, const ClippingPlane &clp) const
{
    for (int vert_idx : m_triangles[facet_idx].verts_idxs)
        if (clp.is_active() && clp.is_mesh_point_clipped(m_vertices[vert_idx].v))
            return true;

    return false;
}

void TriangleSelector::seed_fill_select_triangles(const Vec3f &hit, int facet_start, const Transform3d& trafo_no_translate,
                                                  const ClippingPlane &clp, float seed_fill_angle, float highlight_by_angle_deg,
                                                  bool force_reselection)
{
    assert(facet_start < m_orig_size_indices);

    // 仅当光标指向未被种子填充选择的面或裁剪平面处于活动状态时，才重新计算种子填充。
    if (int start_facet_idx = select_unsplit_triangle(hit, facet_start); start_facet_idx >= 0 && m_triangles[start_facet_idx].is_selected_by_seed_fill() && !force_reselection && !clp.is_active())
        return;

    this->seed_fill_unselect_all_triangles();

    std::vector<bool> visited(m_triangles.size(), false);
    std::queue<int>   facet_queue;
    facet_queue.push(facet_start);

    const double facet_angle_limit     = cos(Geometry::deg2rad(seed_fill_angle)) - EPSILON;
    const float  highlight_angle_limit = -cos(Geometry::deg2rad(highlight_by_angle_deg));

    // 对鼠标光标发射的射线击中的面的相邻面进行深度优先遍历。
    while (!facet_queue.empty()) {
        int current_facet = facet_queue.front();
        facet_queue.pop();

        const Vec3f &facet_normal = m_face_normals[m_triangles[current_facet].source_triangle];
        Matrix3f     normal_matrix  = static_cast<Matrix3f>(trafo_no_translate.matrix().block(0, 0, 3, 3).inverse().transpose().cast<float>());
        float        world_normal_z = (normal_matrix * facet_normal).normalized().z();
        if (!visited[current_facet] && (highlight_by_angle_deg == 0.f || world_normal_z < highlight_angle_limit)) {
            if (m_triangles[current_facet].is_split()) {
                for (int split_triangle_idx = 0; split_triangle_idx <= m_triangles[current_facet].number_of_split_sides(); ++split_triangle_idx) {
                    assert(split_triangle_idx < int(m_triangles[current_facet].children.size()));
                    assert(m_triangles[current_facet].children[split_triangle_idx] < int(m_triangles.size()));
                    if (int child = m_triangles[current_facet].children[split_triangle_idx]; !visited[child])
                        // 子三角形与其父三角形共享法线。选择它。
                        facet_queue.push(child);
                }
            } else
                m_triangles[current_facet].select_by_seed_fill();

            if (current_facet < m_orig_size_indices)
                // 在原始三角形上传播。
                for (int neighbor_idx : m_neighbors[current_facet]) {
                    assert(neighbor_idx >= -1);
                    if (neighbor_idx >= 0 && !visited[neighbor_idx] && !is_facet_clipped(neighbor_idx, clp)) {
                        // 检查相邻面是否满足seed_fill_angle中的角度，如果满足则将其添加到facet_queue。
                        const Vec3f &n1 = m_face_normals[m_triangles[neighbor_idx].source_triangle];
                        const Vec3f &n2 = m_face_normals[m_triangles[current_facet].source_triangle];
                        if (std::clamp(n1.dot(n2), 0.f, 1.f) >= facet_angle_limit)
                            facet_queue.push(neighbor_idx);
                    }
                }
        }
        visited[current_facet] = true;
    }
}

void TriangleSelector::precompute_all_neighbors_recursive(const int facet_idx, const Vec3i32 &neighbors, const Vec3i32 &neighbors_propagated, std::vector<Vec3i32> &neighbors_out, std::vector<Vec3i32> &neighbors_propagated_out) const
{
    assert(facet_idx < int(m_triangles.size()));

    const Triangle *tr = &m_triangles[facet_idx];
    if (!tr->valid())
        return;

    neighbors_out[facet_idx]            = neighbors;
    neighbors_propagated_out[facet_idx] = neighbors_propagated;
    if (tr->is_split()) {
        assert(this->verify_triangle_neighbors(*tr, neighbors));

        int num_of_children = tr->number_of_split_sides() + 1;
        if (num_of_children != 1) {
            for (int i = 0; i < num_of_children; ++i) {
                assert(i < int(tr->children.size()));
                assert(tr->children[i] < int(m_triangles.size()));
                // 递归，深度优先搜索该三角形的子三角形。
                // 该三角形的所有子三角形都是通过拆分原始网格的一个源三角形创建的。
                const Vec3i32 child_neighbors = this->child_neighbors(*tr, neighbors, i);
                this->precompute_all_neighbors_recursive(tr->children[i], child_neighbors,
                                                         this->child_neighbors_propagated(*tr, neighbors_propagated, i, child_neighbors), neighbors_out,
                                                         neighbors_propagated_out);
            }
        }
    }
}

std::pair<std::vector<Vec3i32>, std::vector<Vec3i32>> TriangleSelector::precompute_all_neighbors() const
{
    std::vector<Vec3i32> neighbors(m_triangles.size(), Vec3i32(-1, -1, -1));
    std::vector<Vec3i32> neighbors_propagated(m_triangles.size(), Vec3i32(-1, -1, -1));
    for (int facet_idx = 0; facet_idx < this->m_orig_size_indices; ++facet_idx) {
        neighbors[facet_idx]            = m_neighbors[facet_idx];
        neighbors_propagated[facet_idx] = neighbors[facet_idx];
        assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors[facet_idx]));
        if (m_triangles[facet_idx].is_split())
            this->precompute_all_neighbors_recursive(facet_idx, neighbors[facet_idx], neighbors_propagated[facet_idx], neighbors, neighbors_propagated);
    }
    return std::make_pair(std::move(neighbors), std::move(neighbors_propagated));
}

// 追加所有接触三角形边(vertexi, vertexj)的三角形。
// 不追加仅通过部分边接触三角形的三角形，这意味着这些三角形来自较低深度。
void TriangleSelector::append_touching_subtriangles(int itriangle, int vertexi, int vertexj, std::vector<int> &touching_subtriangles_out) const
{
    if (itriangle == -1)
        return;

    auto process_subtriangle = [this, &itriangle, &vertexi, &vertexj, &touching_subtriangles_out](const int subtriangle_idx, Partition partition) -> void {
        assert(subtriangle_idx != -1);
        if (!m_triangles[subtriangle_idx].is_split())
            touching_subtriangles_out.emplace_back(subtriangle_idx);
        else if (int midpoint = this->triangle_midpoint(itriangle, vertexi, vertexj); midpoint != -1)
            append_touching_subtriangles(subtriangle_idx, partition == Partition::First ? vertexi : midpoint, partition == Partition::First ? midpoint : vertexj, touching_subtriangles_out);
        else
            append_touching_subtriangles(subtriangle_idx, vertexi, vertexj, touching_subtriangles_out);
    };

    std::pair<int, int> touching = this->triangle_subtriangles(itriangle, vertexi, vertexj);
    if (touching.first != -1)
        process_subtriangle(touching.first, Partition::First);

    if (touching.second != -1)
        process_subtriangle(touching.second, Partition::Second);
}

// 追加所有接触三角形边(vertexi, vertexj)且未被种子填充选中的边。
// 不追加仅通过部分边接触三角形的边，这意味着这些三角形来自较低深度。
void TriangleSelector::append_touching_edges(int itriangle, int vertexi, int vertexj, std::vector<Vec2i32> &touching_edges_out) const
{
    if (itriangle == -1)
        return;

    auto process_subtriangle = [this, &itriangle, &vertexi, &vertexj, &touching_edges_out](const int subtriangle_idx, Partition partition) -> void {
        assert(subtriangle_idx != -1);
        if (!m_triangles[subtriangle_idx].is_split()) {
            if (!m_triangles[subtriangle_idx].is_selected_by_seed_fill()) {
                int midpoint = this->triangle_midpoint(itriangle, vertexi, vertexj);
                if (partition == Partition::First && midpoint != -1) {
                    touching_edges_out.emplace_back(vertexi, midpoint);
                } else if (partition == Partition::First && midpoint == -1) {
                    touching_edges_out.emplace_back(vertexi, vertexj);
                } else {
                    assert(midpoint != -1 && partition == Partition::Second);
                    touching_edges_out.emplace_back(midpoint, vertexj);
                }
            }
        } else if (int midpoint = this->triangle_midpoint(itriangle, vertexi, vertexj); midpoint != -1)
            append_touching_edges(subtriangle_idx, partition == Partition::First ? vertexi : midpoint, partition == Partition::First ? midpoint : vertexj,
                                  touching_edges_out);
        else
            append_touching_edges(subtriangle_idx, vertexi, vertexj, touching_edges_out);
    };

    std::pair<int, int> touching = this->triangle_subtriangles(itriangle, vertexi, vertexj);
    if (touching.first != -1)
        process_subtriangle(touching.first, Partition::First);

    if (touching.second != -1)
        process_subtriangle(touching.second, Partition::Second);
}

// BBS: add seed_fill_angle parameter
void TriangleSelector::bucket_fill_select_triangles(const Vec3f& hit, int facet_start, const ClippingPlane &clp, float seed_fill_angle, bool propagate, bool force_reselection)
{
    int start_facet_idx = select_unsplit_triangle(hit, facet_start);
    assert(start_facet_idx != -1);
    // 仅当光标指向未被桶填充选择的面或裁剪平面处于活动状态时，才重新计算桶填充。
    if (start_facet_idx == -1 || (m_triangles[start_facet_idx].is_selected_by_seed_fill() && !force_reselection && !clp.is_active()))
        return;

    assert(!m_triangles[start_facet_idx].is_split());
    EnforcerBlockerType start_facet_state = m_triangles[start_facet_idx].get_state();
    this->seed_fill_unselect_all_triangles();

    if (!propagate) {
        m_triangles[start_facet_idx].select_by_seed_fill();
        return;
    }

    // seed_fill_angle < 0.f 禁用边缘检测
    const double facet_angle_limit = (seed_fill_angle < 0.f ? -1.f : cos(Geometry::deg2rad(seed_fill_angle))) - EPSILON;

    auto get_all_touching_triangles = [this](int facet_idx, const Vec3i32 &neighbors, const Vec3i32 &neighbors_propagated) -> std::vector<int> {
        assert(facet_idx != -1 && facet_idx < int(m_triangles.size()));
        assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors));
        std::vector<int> touching_triangles;
        Vec3i32            vertices = {m_triangles[facet_idx].verts_idxs[0], m_triangles[facet_idx].verts_idxs[1], m_triangles[facet_idx].verts_idxs[2]};
        append_touching_subtriangles(neighbors(0), vertices(1), vertices(0), touching_triangles);
        append_touching_subtriangles(neighbors(1), vertices(2), vertices(1), touching_triangles);
        append_touching_subtriangles(neighbors(2), vertices(0), vertices(2), touching_triangles);

        for (int neighbor_idx : neighbors_propagated)
            if (neighbor_idx != -1 && !m_triangles[neighbor_idx].is_split())
                touching_triangles.emplace_back(neighbor_idx);

        return touching_triangles;
    };

    auto [neighbors, neighbors_propagated] = this->precompute_all_neighbors();
    std::vector<bool>  visited(m_triangles.size(), false);
    std::queue<int>    facet_queue;

    facet_queue.push(start_facet_idx);
    while (!facet_queue.empty()) {
        int current_facet = facet_queue.front();
        facet_queue.pop();
        assert(!m_triangles[current_facet].is_split());

        if (!visited[current_facet]) {
            m_triangles[current_facet].select_by_seed_fill();

            std::vector<int> touching_triangles = get_all_touching_triangles(current_facet, neighbors[current_facet], neighbors_propagated[current_facet]);
            for(const int tr_idx : touching_triangles) {
                if (tr_idx < 0 || visited[tr_idx] || m_triangles[tr_idx].get_state() != start_facet_state || is_facet_clipped(tr_idx, clp))
                    continue;

                const Vec3f& n1 = m_face_normals[m_triangles[tr_idx].source_triangle];
                const Vec3f& n2 = m_face_normals[m_triangles[current_facet].source_triangle];
                if (seed_fill_angle >= -EPSILON && std::clamp(n1.dot(n2), 0.f, 1.f) < facet_angle_limit)
                    continue;

                assert(!m_triangles[tr_idx].is_split());
                facet_queue.push(tr_idx);
            }
        }

        visited[current_facet] = true;
    }
}

// 选择整个三角形（丢弃其所有子三角形），或递归分割
// 三角形，仅选择真正位于圆内的子三角形。
// 通过实际的递归调用来完成。如果三角形在光标外则返回false。
// 由select_patch()和自身调用。
bool TriangleSelector::select_triangle(int facet_idx, EnforcerBlockerType type, bool triangle_splitting)
{
    assert(facet_idx < int(m_triangles.size()));

    if (! m_triangles[facet_idx].valid())
        return false;

    Vec3i32 neighbors = m_neighbors[facet_idx];
    assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors));

    if (! select_triangle_recursive(facet_idx, neighbors, type, triangle_splitting))
        return false;

    // 如果所有子三角形都是叶节点且现在具有相同状态，
    // 它们可以被移除并由父三角形替代。
    remove_useless_children(facet_idx);

#ifdef EXPENSIVE_DEBUG_CHECKS
    // Make sure that we did not lose track of invalid triangles.
    assert(m_invalid_triangles == std::count_if(m_triangles.begin(), m_triangles.end(),
               [](const Triangle& tr) { return ! tr.valid(); }));
#endif // EXPENSIVE_DEBUG_CHECKS

    // 也许需要垃圾回收？
    if (2*m_invalid_triangles > int(m_triangles.size()))
        garbage_collect();

    return true;
}

// 返回itriangle在CCW方向边(vertexi, vertexj)上的子三角形，第一部分或第二部分。
// 如果共享边(vertexi, vertexj)未拆分，返回-1。
int TriangleSelector::neighbor_child(const Triangle &tr, int vertexi, int vertexj, Partition partition) const
{
    if (tr.number_of_split_sides() == 0)
        // 如果此三角形未拆分，则没有共享该边的上/下子三角形。
        return -1;

    // 查找三角形的边。
    int edge = tr.verts_idxs[0] == vertexi ? 0 : tr.verts_idxs[1] == vertexi ? 1 : 2;
    assert(tr.verts_idxs[edge] == vertexi);
    assert(tr.verts_idxs[next_idx_modulo(edge, 3)] == vertexj);

    int child_idx;
    if (tr.number_of_split_sides() == 1) {
        if (edge != next_idx_modulo(tr.special_side(), 3))
            // A child may or may not be split at this side.
            return this->neighbor_child(m_triangles[tr.children[edge == tr.special_side() ? 0 : 1]], vertexi, vertexj, partition);
        child_idx = partition == Partition::First ? 0 : 1;
    } else if (tr.number_of_split_sides() == 2) {
        if (edge == next_idx_modulo(tr.special_side(), 3))
            // A child may or may not be split at this side.
            return this->neighbor_child(m_triangles[tr.children[2]], vertexi, vertexj, partition);
        child_idx = edge == tr.special_side() ?
            (partition == Partition::First ? 0 : 1) :
            (partition == Partition::First ? 2 : 0);
    } else {
        assert(tr.number_of_split_sides() == 3);
        assert(tr.special_side() == 0);
        switch(edge) {
        case 0:  child_idx = partition == Partition::First ? 0 : 1; break;
        case 1:  child_idx = partition == Partition::First ? 1 : 2; break;
        default: assert(edge == 2);
                 child_idx = partition == Partition::First ? 2 : 0; break;
        }
    }
    return tr.children[child_idx];
}

// 返回itriangle在CCW方向边(vertexi, vertexj)上的子三角形，第一部分或第二部分。
// 如果itriangle == -1或共享边(vertexi, vertexj)未拆分，返回-1。
int TriangleSelector::neighbor_child(int itriangle, int vertexi, int vertexj, Partition partition) const
{
    return itriangle == -1 ? -1 : this->neighbor_child(m_triangles[itriangle], vertexi, vertexj, partition);
}

std::pair<int, int> TriangleSelector::triangle_subtriangles(int itriangle, int vertexi, int vertexj) const
{
    return itriangle == -1 ? std::make_pair(-1, -1) : Slic3r::TriangleSelector::triangle_subtriangles(m_triangles[itriangle], vertexi, vertexj);
}

std::pair<int, int> TriangleSelector::triangle_subtriangles(const Triangle &tr, int vertexi, int vertexj)
{
    if (tr.number_of_split_sides() == 0)
        // 如果此三角形未拆分，则没有接触该边的子三角形。
        return std::make_pair(-1, -1);

    // 查找三角形的边。
    int edge = tr.verts_idxs[0] == vertexi ? 0 : tr.verts_idxs[1] == vertexi ? 1 : 2;
    assert(tr.verts_idxs[edge] == vertexi);
    assert(tr.verts_idxs[next_idx_modulo(edge, 3)] == vertexj);

    if (tr.number_of_split_sides() == 1) {
        return edge == next_idx_modulo(tr.special_side(), 3) ? std::make_pair(tr.children[0], tr.children[1]) :
                                                                     std::make_pair(tr.children[edge == tr.special_side() ? 0 : 1], -1);
    } else if (tr.number_of_split_sides() == 2) {
        return edge == next_idx_modulo(tr.special_side(), 3) ? std::make_pair(tr.children[2], -1) :
               edge == tr.special_side()                           ? std::make_pair(tr.children[0], tr.children[1]) :
                                                                     std::make_pair(tr.children[2], tr.children[0]);
    } else {
        assert(tr.number_of_split_sides() == 3);
        assert(tr.special_side() == 0);
        return edge == 0 ? std::make_pair(tr.children[0], tr.children[1]) :
               edge == 1 ? std::make_pair(tr.children[1], tr.children[2]) :
                           std::make_pair(tr.children[2], tr.children[0]);
    }

    return std::make_pair(-1, -1);
}

// 返回CCW方向边(vertexi, vertexj)的现有中点。
// 如果itriangle == -1或共享边(vertexi, vertexj)未拆分，返回-1。
int TriangleSelector::triangle_midpoint(const Triangle &tr, int vertexi, int vertexj) const
{
    if (tr.number_of_split_sides() == 0)
        // 如果此三角形未拆分，则没有共享该边的上/下子三角形。
        return -1;

    // 查找三角形的边。
    int edge = tr.verts_idxs[0] == vertexi ? 0 : tr.verts_idxs[1] == vertexi ? 1 : 2;
    assert(tr.verts_idxs[edge] == vertexi);
    assert(tr.verts_idxs[next_idx_modulo(edge, 3)] == vertexj);

    if (tr.number_of_split_sides() == 1) {
        return edge == next_idx_modulo(tr.special_side(), 3) ?
            m_triangles[tr.children[0]].verts_idxs[2] :
            this->triangle_midpoint(m_triangles[tr.children[edge == tr.special_side() ? 0 : 1]], vertexi, vertexj);
    } else if (tr.number_of_split_sides() == 2) {
        return edge == next_idx_modulo(tr.special_side(), 3) ?
                    this->triangle_midpoint(m_triangles[tr.children[2]], vertexi, vertexj) :
               edge == tr.special_side() ?
                    m_triangles[tr.children[0]].verts_idxs[1] :
                    m_triangles[tr.children[1]].verts_idxs[2];
    } else {
        assert(tr.number_of_split_sides() == 3);
        assert(tr.special_side() == 0);
        return
            (edge == 0) ? m_triangles[tr.children[0]].verts_idxs[1] :
            (edge == 1) ? m_triangles[tr.children[1]].verts_idxs[2] :
                          m_triangles[tr.children[2]].verts_idxs[2];
    }
}

// 返回CCW方向边(vertexi, vertexj)的现有中点。
// 如果itriangle == -1或共享边(vertexi, vertexj)未拆分，返回-1。
int TriangleSelector::triangle_midpoint(int itriangle, int vertexi, int vertexj) const
{
    return itriangle == -1 ? -1 : this->triangle_midpoint(m_triangles[itriangle], vertexi, vertexj);
}

int TriangleSelector::triangle_midpoint_or_allocate(int itriangle, int vertexi, int vertexj)
{
    int midpoint = this->triangle_midpoint(itriangle, vertexi, vertexj);
    if (midpoint == -1) {
        Vec3f c = 0.5f * (m_vertices[vertexi].v + m_vertices[vertexj].v);
#ifdef EXPENSIVE_DEBUG_CHECKS
        // 验证该顶点确实是新的。
        auto it = std::find_if(m_vertices.begin(), m_vertices.end(), [c](const Vertex &v) {
            return v.ref_cnt > 0 && (v.v - c).norm() < EPSILON; });
        assert(it == m_vertices.end());
#endif // EXPENSIVE_DEBUG_CHECKS
        // 分配新顶点，可能重用空闲列表。
        if (m_free_vertices_head == -1) {
            // 分配新顶点。
            midpoint = int(m_vertices.size());
            m_vertices.emplace_back(c);
        } else {
            // 从空闲列表中重用顶点。
            assert(m_free_vertices_head >= -1 && m_free_vertices_head < int(m_vertices.size()));
            midpoint = m_free_vertices_head;
            memcpy(&m_free_vertices_head, &m_vertices[midpoint].v[0], sizeof(m_free_vertices_head));
            assert(m_free_vertices_head >= -1 && m_free_vertices_head < int(m_vertices.size()));
            m_vertices[midpoint].v = c;
        }
        assert(m_vertices[midpoint].ref_cnt == 0);
    } else {
#ifndef NDEBUG
        Vec3f c1 = 0.5f * (m_vertices[vertexi].v + m_vertices[vertexj].v);
        Vec3f c2 = m_vertices[midpoint].v;
        float d = (c2 - c1).norm();
        assert(std::abs(d) < EPSILON);
#endif // NDEBUG
        assert(m_vertices[midpoint].ref_cnt > 0);
    }
    return midpoint;
}

// 返回给定三角形邻居的第i个子三角形的邻居。
// 如果这样的邻居根本不存在，或与第i个子三角形不在同一深度，则返回-1。
// 使用与TriangleSelector::split_triangle()相同的拆分策略。
Vec3i32 TriangleSelector::child_neighbors(const Triangle &tr, const Vec3i32 &neighbors, int child_idx) const
{
    assert(this->verify_triangle_neighbors(tr, neighbors));

    assert(child_idx >= 0 && child_idx <= tr.number_of_split_sides());
    int   i = tr.special_side();
    int   j = next_idx_modulo(i, 3);
    int   k = next_idx_modulo(j, 3);

    Vec3i32 out;
    switch (tr.number_of_split_sides()) {
    case 1:
        switch (child_idx) {
        case 0:
            out(0) = neighbors(i);
            out(1) = this->neighbor_child(neighbors(j), tr.verts_idxs[k], tr.verts_idxs[j], Partition::Second);
            out(2) = tr.children[1];
            break;
        default:
            assert(child_idx == 1);
            out(0) = this->neighbor_child(neighbors(j), tr.verts_idxs[k], tr.verts_idxs[j], Partition::First);
            out(1) = neighbors(k);
            out(2) = tr.children[0];
            break;
        }
        break;

    case 2:
        switch (child_idx) {
        case 0:
            out(0) = this->neighbor_child(neighbors(i), tr.verts_idxs[j], tr.verts_idxs[i], Partition::Second);
            out(1) = tr.children[1];
            out(2) = this->neighbor_child(neighbors(k), tr.verts_idxs[i], tr.verts_idxs[k], Partition::First);
            break;
        case 1:
            assert(child_idx == 1);
            out(0) = this->neighbor_child(neighbors(i), tr.verts_idxs[j], tr.verts_idxs[i], Partition::First);
            out(1) = tr.children[2];
            out(2) = tr.children[0];
            break;
        default:
            assert(child_idx == 2);
            out(0) = neighbors(j);
            out(1) = this->neighbor_child(neighbors(k), tr.verts_idxs[i], tr.verts_idxs[k], Partition::Second);
            out(2) = tr.children[1];
            break;
        }
        break;

    case 3:
        assert(tr.special_side() == 0);
        switch (child_idx) {
        case 0:
            out(0) = this->neighbor_child(neighbors(0), tr.verts_idxs[1], tr.verts_idxs[0], Partition::Second);
            out(1) = tr.children[3];
            out(2) = this->neighbor_child(neighbors(2), tr.verts_idxs[0], tr.verts_idxs[2], Partition::First);
            break;
        case 1:
            out(0) = this->neighbor_child(neighbors(0), tr.verts_idxs[1], tr.verts_idxs[0], Partition::First);
            out(1) = this->neighbor_child(neighbors(1), tr.verts_idxs[2], tr.verts_idxs[1], Partition::Second);
            out(2) = tr.children[3];
            break;
        case 2:
            out(0) = this->neighbor_child(neighbors(1), tr.verts_idxs[2], tr.verts_idxs[1], Partition::First);
            out(1) = this->neighbor_child(neighbors(2), tr.verts_idxs[0], tr.verts_idxs[2], Partition::Second);
            out(2) = tr.children[3];
            break;
        default:
            assert(child_idx == 3);
            out(0) = tr.children[1];
            out(1) = tr.children[2];
            out(2) = tr.children[0];
            break;
        }
        break;

    default:
        assert(false);
    }

    assert(this->verify_triangle_neighbors(tr, neighbors));
    assert(this->verify_triangle_neighbors(m_triangles[tr.children[child_idx]], out));
    return out;
}

// 返回给定三角形邻居的第i个子三角形的邻居。
// 如果这样的邻居不存在，则返回上一深度的邻居。
Vec3i32 TriangleSelector::child_neighbors_propagated(const Triangle &tr, const Vec3i32 &neighbors_propagated, int child_idx, const Vec3i32 &child_neighbors) const
{
    int i = tr.special_side();
    int j = next_idx_modulo(i, 3);
    int k = next_idx_modulo(j, 3);

    Vec3i32 out = child_neighbors;
    auto  replace_if_not_exists = [&out, &neighbors_propagated](int index_to_replace, int neighbor_idx) {
        if (out(index_to_replace) == -1)
            out(index_to_replace) = neighbors_propagated(neighbor_idx);
    };

    switch (tr.number_of_split_sides()) {
    case 1:
        switch (child_idx) {
        case 0:
            replace_if_not_exists(0, i);
            replace_if_not_exists(1, j);
            break;
        default:
            assert(child_idx == 1);
            replace_if_not_exists(0, j);
            replace_if_not_exists(1, k);
            break;
        }
        break;

    case 2:
        switch (child_idx) {
        case 0:
            replace_if_not_exists(0, i);
            replace_if_not_exists(2, k);
            break;
        case 1:
            assert(child_idx == 1);
            replace_if_not_exists(0, i);
            break;
        default:
            assert(child_idx == 2);
            replace_if_not_exists(0, j);
            replace_if_not_exists(1, k);
            break;
        }
        break;

    case 3:
        assert(tr.special_side() == 0);
        switch (child_idx) {
        case 0:
            replace_if_not_exists(0, 0);
            replace_if_not_exists(2, 2);
            break;
        case 1:
            replace_if_not_exists(0, 0);
            replace_if_not_exists(1, 1);
            break;
        case 2:
            replace_if_not_exists(0, 1);
            replace_if_not_exists(1, 2);
            break;
        default:
            assert(child_idx == 3);
            break;
        }
        break;

    default: assert(false);
    }

    return out;
}

bool TriangleSelector::select_triangle_recursive(int facet_idx, const Vec3i32 &neighbors, EnforcerBlockerType type, bool triangle_splitting)
{
    assert(facet_idx < int(m_triangles.size()));

    Triangle* tr = &m_triangles[facet_idx];
    if (! tr->valid())
        return false;

    assert(this->verify_triangle_neighbors(*tr, neighbors));

    int num_of_inside_vertices = m_cursor->vertices_inside(*tr, m_vertices);

    if (num_of_inside_vertices == 0
     && ! m_cursor->is_pointer_in_triangle(*tr, m_vertices)
     && ! m_cursor->is_edge_inside_cursor(*tr, m_vertices))
        return false;

    if (num_of_inside_vertices == 3) {
        // dump any subdivision and select whole triangle
        undivide_triangle(facet_idx);
        tr->set_state(type);
    } else {
        // the triangle is partially inside, let's recursively divide it
        // (if not already) and try selecting its children.

        if (! tr->is_split() && tr->get_state() == type) {
            // 这是叶三角形，整体上已经是正确的类型。
            // 无需拆分，所有子三角形无论如何都会被选中。
            return true;
        }

        if (triangle_splitting)
            split_triangle(facet_idx, neighbors);
        else if (!m_triangles[facet_idx].is_split())
            m_triangles[facet_idx].set_state(type);
        tr = &m_triangles[facet_idx]; // might have been invalidated by split_triangle().

        int num_of_children = tr->number_of_split_sides() + 1;
        if (num_of_children != 1) {
            for (int i=0; i<num_of_children; ++i) {
                assert(i < int(tr->children.size()));
                assert(tr->children[i] < int(m_triangles.size()));
                // 递归，深度优先搜索该三角形的子三角形。
                // 该三角形的所有子三角形都是通过拆分原始网格的一个源三角形创建的。
                select_triangle_recursive(tr->children[i], this->child_neighbors(*tr, neighbors, i), type, triangle_splitting);
                tr = &m_triangles[facet_idx]; // might have been invalidated
            }
        }
    }

    return true;
}

void TriangleSelector::set_facet(int facet_idx, EnforcerBlockerType state)
{
    assert(facet_idx < m_orig_size_indices);
    undivide_triangle(facet_idx);
    assert(! m_triangles[facet_idx].is_split());
    m_triangles[facet_idx].set_state(state);
}

// 由select_patch()->select_triangle()...select_triangle()调用
// 决定分割三角形的哪些边，并通过调用set_division()和perform_split()实际分割它。
void TriangleSelector::split_triangle(int facet_idx, const Vec3i32 &neighbors)
{
    if (m_triangles[facet_idx].is_split()) {
        // 三角形已经分割过了。
        return;
    }

    Triangle* tr = &m_triangles[facet_idx];
    assert(this->verify_triangle_neighbors(*tr, neighbors));

    EnforcerBlockerType old_type = tr->get_state();

    // 如果执行到这里，我们即将实际分割三角形。
    const double limit_squared = m_edge_limit_sqr;

    std::array<int, 3>& facet = tr->verts_idxs;
    std::array<const stl_vertex*, 3> pts = { &m_vertices[facet[0]].v,
                                             &m_vertices[facet[1]].v,
                                             &m_vertices[facet[2]].v};
    std::array<stl_vertex, 3> pts_transformed; // must stay in scope of pts !!!

    // 如果对象非均匀缩放，则将点变换到世界坐标。
    if (! m_cursor->uniform_scaling) {
        for (size_t i=0; i<pts.size(); ++i) {
            pts_transformed[i] = m_cursor->trafo * (*pts[i]);
            pts[i] = &pts_transformed[i];
        }
    }

    std::array<double, 3> sides = {(*pts[2] - *pts[1]).squaredNorm(),
                                   (*pts[0] - *pts[2]).squaredNorm(),
                                   (*pts[1] - *pts[0]).squaredNorm()};

    boost::container::small_vector<int, 3> sides_to_split;
    int side_to_keep = -1;
    for (int pt_idx = 0; pt_idx<3; ++pt_idx) {
        if (sides[pt_idx] > limit_squared)
            sides_to_split.push_back(pt_idx);
        else
            side_to_keep = pt_idx;
    }
    if (sides_to_split.empty()) {
        // This shall be unselected.
        tr->set_division(0, 0);
        return;
    }

    // 保存三角形将如何分割。第二个参数仅对一个或两个分割边有意义，否则该值被忽略。
    tr->set_division(int(sides_to_split.size()),
        sides_to_split.size() == 2 ? side_to_keep : sides_to_split[0]);

    perform_split(facet_idx, neighbors, old_type);
}

// 指针是否在三角形内？
bool TriangleSelector::Cursor::is_pointer_in_triangle(const Triangle &tr, const std::vector<Vertex> &vertices) const {
    const Vec3f& p1 = vertices[tr.verts_idxs[0]].v;
    const Vec3f& p2 = vertices[tr.verts_idxs[1]].v;
    const Vec3f& p3 = vertices[tr.verts_idxs[2]].v;
    return this->is_pointer_in_triangle(p1, p2, p3);
}

// 确定该面是否可能可见（仍可能被遮挡）。
bool TriangleSelector::Cursor::is_facet_visible(const Cursor &cursor, int facet_idx, const std::vector<Vec3f> &face_normals)
{
    assert(facet_idx < int(face_normals.size()));
    Vec3f n = face_normals[facet_idx];
    if (!cursor.uniform_scaling)
        n = cursor.trafo_normal * n;
    return n.dot(cursor.dir) < 0.f;
}

// 三角形的多少个顶点在圆内？
int TriangleSelector::Cursor::vertices_inside(const Triangle &tr, const std::vector<Vertex> &vertices) const
{
    int inside = 0;
    for (size_t i = 0; i < 3; ++i)
        if (this->is_mesh_point_inside(vertices[tr.verts_idxs[i]].v))
            ++inside;

    return inside;
}

// 是否有任何边在球体光标内？
bool TriangleSelector::Sphere::is_edge_inside_cursor(const Triangle &tr, const std::vector<Vertex> &vertices) const
{
    std::array<Vec3f, 3> pts;
    for (int i = 0; i < 3; ++i) {
        pts[i] = vertices[tr.verts_idxs[i]].v;
        if (!this->uniform_scaling)
            pts[i] = this->trafo * pts[i];
    }

    for (int side = 0; side < 3; ++side) {
        const Vec3f &edge_a = pts[side];
        const Vec3f &edge_b = pts[side < 2 ? side + 1 : 0];
        if (test_line_inside_sphere(edge_a, edge_b, this->center, this->radius))
            return true;
    }
    return false;
}

// 边是否在光标内？
bool TriangleSelector::Circle::is_edge_inside_cursor(const Triangle &tr, const std::vector<Vertex> &vertices) const
{
    std::array<Vec3f, 3> pts;
    for (int i = 0; i < 3; ++i) {
        pts[i] = vertices[tr.verts_idxs[i]].v;
        if (!this->uniform_scaling)
            pts[i] = this->trafo * pts[i];
    }

    const Vec3f &p = this->center;
    for (int side = 0; side < 3; ++side) {
        const Vec3f &a      = pts[side];
        const Vec3f &b      = pts[side < 2 ? side + 1 : 0];
        Vec3f        s      = (b - a).normalized();
        float        t      = (p - a).dot(s);
        Vec3f        vector = a + t * s - p;

        // vector是从中心到交点的3D向量。我们要测量的是其在垂直于dir的平面上的投影长度。
        float dist_sqr = vector.squaredNorm() - std::pow(vector.dot(this->dir), 2.f);
        if (dist_sqr < this->radius_sqr && t >= 0.f && t <= (b - a).norm())
            return true;
    }
    return false;
}

// BBS
bool TriangleSelector::HeightRange::is_pointer_in_triangle(const Vec3f& p1_, const Vec3f& p2_, const Vec3f& p3_) const
{
    return false;
}

bool TriangleSelector::HeightRange::is_mesh_point_inside(const Vec3f& point) const
{
    // 仅使用40%的边长限制作为公差
    const float tolerance = 0.02;
    const Vec3f transformed_point = trafo * point;
    float top_z = m_z_world + m_height + tolerance;
    float bot_z = m_z_world - tolerance;

    return transformed_point.z() > bot_z && transformed_point.z() < top_z;
}

bool TriangleSelector::HeightRange::is_edge_inside_cursor(const Triangle& tr, const std::vector<Vertex>& vertices) const
{
    float top_z = m_z_world + m_height + EPSILON;
    float bot_z = m_z_world - EPSILON;
    std::array<Vec3f, 3> pts;
    for (int i = 0; i < 3; ++i) {
        pts[i] = vertices[tr.verts_idxs[i]].v;
        pts[i] = this->trafo * pts[i];
    }

    return !((pts[0].z() < bot_z && pts[1].z() < bot_z && pts[2].z() < bot_z) ||
             (pts[0].z() > top_z && pts[1].z() > top_z && pts[2].z() > top_z));
}

// 递归移除所有子三角形。
void TriangleSelector::undivide_triangle(int facet_idx)
{
    assert(facet_idx < int(m_triangles.size()));
    Triangle& tr = m_triangles[facet_idx];

    if (tr.is_split()) {
        for (int i = 0; i <= tr.number_of_split_sides(); ++i) {
            int       child    = tr.children[i];
            Triangle &child_tr = m_triangles[child];
            assert(child_tr.valid());
            undivide_triangle(child);
            for (int j = 0; j < 3; ++j) {
                int     iv = child_tr.verts_idxs[j];
                Vertex &v  = m_vertices[iv];
                assert(v.ref_cnt > 0);
                if (-- v.ref_cnt == 0) {
                    // 释放此顶点。
                    // 通过ref_cnt将释放的顶点链接成链表。
                    assert(m_free_vertices_head >= -1 && m_free_vertices_head < int(m_vertices.size()));
                    memcpy(&m_vertices[iv].v[0], &m_free_vertices_head, sizeof(m_free_vertices_head));
                    m_free_vertices_head = iv;
                    assert(m_free_vertices_head >= -1 && m_free_vertices_head < int(m_vertices.size()));
                }
            }
            // 通过children[0]将释放的三角形链接成链表。
            assert(child_tr.valid());
            child_tr.m_valid = false;
            assert(m_free_triangles_head >= -1 && m_free_triangles_head < int(m_triangles.size()));
            assert(m_free_triangles_head == -1 || ! m_triangles[m_free_triangles_head].valid());
            child_tr.children[0] = m_free_triangles_head;
            m_free_triangles_head = child;
            assert(m_free_triangles_head >= -1 && m_free_triangles_head < int(m_triangles.size()));
            ++m_invalid_triangles;
        }
        tr.set_division(0, 0); // not split
    }
}

void TriangleSelector::remove_useless_children(int facet_idx)
{
    // 检查所有子三角形是否为相同类型的叶节点。如果不是，尝试使它们成为叶节点（递归调用）。如果成功则移除它们。

    assert(facet_idx < int(m_triangles.size()) && m_triangles[facet_idx].valid());
    Triangle& tr = m_triangles[facet_idx];

    if (! tr.is_split()) {
        // 这是叶节点，无需执行任何操作。这在第一次（非递归调用）时可能发生。其他情况下不应该发生。
        return;
    }

    // 对所有非叶子的子三角形调用此函数。
    for (int child_idx=0; child_idx<=tr.number_of_split_sides(); ++child_idx) {
        assert(child_idx < int(m_triangles.size()) && m_triangles[child_idx].valid());
        if (m_triangles[tr.children[child_idx]].is_split())
            remove_useless_children(tr.children[child_idx]);
    }


    // 如果子三角形不是叶节点或两个子三角形类型不同，则返回。
    EnforcerBlockerType first_child_type = EnforcerBlockerType::NONE;
    for (int child_idx=0; child_idx<=tr.number_of_split_sides(); ++child_idx) {
        if (m_triangles[tr.children[child_idx]].is_split())
            return;
        if (child_idx == 0)
            first_child_type = m_triangles[tr.children[0]].get_state();
        else if (m_triangles[tr.children[child_idx]].get_state() != first_child_type)
            return;
    }

    // If we got here, the children can be removed.
    undivide_triangle(facet_idx);
    tr.set_state(first_child_type);
}

void TriangleSelector::garbage_collect()
{
    // 首先创建从旧到新的三角形索引映射。
    int new_idx = m_orig_size_indices;
    std::vector<int> new_triangle_indices(m_triangles.size(), -1);
    for (int i = m_orig_size_indices; i<int(m_triangles.size()); ++i)
        if (m_triangles[i].valid())
            new_triangle_indices[i] = new_idx ++;

    // 现在我们知道哪些顶点不再被引用。创建从旧索引到新索引的映射，就像我们对三角形所做的那样。
    new_idx = m_orig_size_vertices;
    std::vector<int> new_vertices_indices(m_vertices.size(), -1);
    for (int i=m_orig_size_vertices; i<int(m_vertices.size()); ++i) {
        assert(m_vertices[i].ref_cnt >= 0);
        if (m_vertices[i].ref_cnt != 0)
            new_vertices_indices[i] = new_idx ++;
    }

    // 我们可以移除所有不再被引用的无效三角形和顶点。
    m_triangles.erase(std::remove_if(m_triangles.begin()+m_orig_size_indices, m_triangles.end(),
                          [](const Triangle& tr) { return ! tr.valid(); }),
                      m_triangles.end());
    m_vertices.erase(std::remove_if(m_vertices.begin()+m_orig_size_vertices, m_vertices.end(),
                          [](const Vertex& vert) { return vert.ref_cnt == 0; }),
                      m_vertices.end());

    // 现在遍历所有剩余的三角形并更新更改的索引。
    for (Triangle& tr : m_triangles) {
        assert(tr.valid());

        if (tr.is_split()) {
            // 有子三角形。更新它们的索引。
            for (int j=0; j<=tr.number_of_split_sides(); ++j) {
                assert(new_triangle_indices[tr.children[j]] != -1);
                tr.children[j] = new_triangle_indices[tr.children[j]];
            }
        }

        // 更新m_vertices的索引。原始顶点从未被触及且无需重新索引。
        for (int& idx : tr.verts_idxs) {
            if (idx >= m_orig_size_vertices) {
                assert(new_vertices_indices[idx] != -1);
                idx = new_vertices_indices[idx];
            }
        }
    }

    m_invalid_triangles = 0;
    m_free_triangles_head = -1;
    m_free_vertices_head = -1;
}

void TriangleSelector::remap_triangle_state(const EnforcerBlockerStateMap& state_map)
{
    if (m_triangles.empty())
        return;

    tbb::parallel_for(tbb::blocked_range<size_t>(0, m_triangles.size()), [this, &state_map](const tbb::blocked_range<size_t>& range) {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            Triangle& tr = m_triangles[i];
            if (tr.valid()) {
                const auto current_state = static_cast<size_t>(tr.get_state());
                tr.set_state(state_map[current_state]);
            }
        }
    });
}

TriangleSelector::TriangleSelector(const TriangleMesh& mesh, float edge_limit)
    : m_mesh{mesh}, m_neighbors(its_face_neighbors(mesh.its)), m_face_normals(its_face_normals(mesh.its)), m_edge_limit(edge_limit)
{
    reset();
}

void TriangleSelector::reset()
{
    m_vertices.clear();
    m_triangles.clear();
    m_invalid_triangles = 0;
    m_free_triangles_head = -1;
    m_free_vertices_head = -1;
    m_vertices.reserve(m_mesh.its.vertices.size());
    for (const stl_vertex& vert : m_mesh.its.vertices)
        m_vertices.emplace_back(vert);
    m_triangles.reserve(m_mesh.its.indices.size());
    for (size_t i = 0; i < m_mesh.its.indices.size(); ++i) {
        const stl_triangle_vertex_indices &ind = m_mesh.its.indices[i];
        push_triangle(ind[0], ind[1], ind[2], int(i));
    }
    m_orig_size_vertices = int(m_vertices.size());
    m_orig_size_indices  = int(m_triangles.size());
}

void TriangleSelector::set_edge_limit(float edge_limit)
{
    m_edge_limit_sqr = std::pow(edge_limit, 2.f);
}

int TriangleSelector::push_triangle(int a, int b, int c, int source_triangle, const EnforcerBlockerType state)
{
    for (int i : {a, b, c}) {
        assert(i >= 0 && i < int(m_vertices.size()));
        ++m_vertices[i].ref_cnt;
    }
    int idx;
    if (m_free_triangles_head == -1) {
        // Allocate a new triangle.
        assert(m_invalid_triangles == 0);
        idx = int(m_triangles.size());
        m_triangles.emplace_back(a, b, c, source_triangle, state);
    } else {
        // Reuse triangle from the free list.
        assert(m_free_triangles_head >= -1 && m_free_triangles_head < int(m_triangles.size()));
        assert(! m_triangles[m_free_triangles_head].valid());
        assert(m_invalid_triangles > 0);
        idx = m_free_triangles_head;
        m_free_triangles_head = m_triangles[idx].children[0];
        -- m_invalid_triangles;
        assert(m_free_triangles_head >= -1 && m_free_triangles_head < int(m_triangles.size()));
        assert(m_free_triangles_head == -1 || ! m_triangles[m_free_triangles_head].valid());
        assert(m_invalid_triangles >= 0);
        assert((m_invalid_triangles == 0) == (m_free_triangles_head == -1));
        m_triangles[idx] = {a, b, c, source_triangle, state};
    }
    assert(m_triangles[idx].valid());
    return idx;
}

// 由deserialize()和select_patch()->select_triangle()->...select_triangle()->split_triangle()调用
// 根据Triangle::number_of_split_sides()和Triangle::special_side()分割三角形，
// 通过分配子三角形和中点顶点。
// 中点顶点可能通过遍历相邻三角形的子三角形来重用。
void TriangleSelector::perform_split(int facet_idx, const Vec3i32 &neighbors, EnforcerBlockerType old_state)
{
    // 预先为新三角形保留空间，以便对此三角形的引用不会改变。
    {
        size_t num_triangles_new = m_triangles.size() + m_triangles[facet_idx].number_of_split_sides() + 1;
        if (m_triangles.capacity() < num_triangles_new)
            m_triangles.reserve(next_highest_power_of_2(num_triangles_new));
    }

    Triangle &tr = m_triangles[facet_idx];
    assert(tr.is_split());

    // 三角形顶点的索引
#ifdef NDEBUG
    boost::container::small_vector<int, 6> verts_idxs;
#else // NDEBUG
    // For easier debugging.
    std::vector<int> verts_idxs;
    verts_idxs.reserve(6);
#endif // NDEBUG
    for (int j=0, idx = tr.special_side(); j<3; ++j, idx = next_idx_modulo(idx, 3))
        verts_idxs.push_back(tr.verts_idxs[idx]);

    auto get_alloc_vertex = [this, &neighbors, &verts_idxs](int edge, int i1, int i2) -> int {
        return this->triangle_midpoint_or_allocate(neighbors(edge), verts_idxs[i1], verts_idxs[i2]);
    };

    int ichild = 0;
    switch (tr.number_of_split_sides()) {
    case 1:
        verts_idxs.insert(verts_idxs.begin()+2, get_alloc_vertex(next_idx_modulo(tr.special_side(), 3), 2, 1));
        tr.children[ichild ++] = push_triangle(verts_idxs[0], verts_idxs[1], verts_idxs[2], tr.source_triangle, old_state);
        tr.children[ichild   ] = push_triangle(verts_idxs[2], verts_idxs[3], verts_idxs[0], tr.source_triangle, old_state);
        break;

    case 2:
        verts_idxs.insert(verts_idxs.begin()+1, get_alloc_vertex(tr.special_side(), 1, 0));
        verts_idxs.insert(verts_idxs.begin()+4, get_alloc_vertex(prev_idx_modulo(tr.special_side(), 3), 0, 3));
        tr.children[ichild ++] = push_triangle(verts_idxs[0], verts_idxs[1], verts_idxs[4], tr.source_triangle, old_state);
        tr.children[ichild ++] = push_triangle(verts_idxs[1], verts_idxs[2], verts_idxs[4], tr.source_triangle, old_state);
        tr.children[ichild   ] = push_triangle(verts_idxs[2], verts_idxs[3], verts_idxs[4], tr.source_triangle, old_state);
        break;

    case 3:
        assert(tr.special_side() == 0);
        verts_idxs.insert(verts_idxs.begin()+1, get_alloc_vertex(0, 1, 0));
        verts_idxs.insert(verts_idxs.begin()+3, get_alloc_vertex(1, 3, 2));
        verts_idxs.insert(verts_idxs.begin()+5, get_alloc_vertex(2, 0, 4));
        tr.children[ichild ++] = push_triangle(verts_idxs[0], verts_idxs[1], verts_idxs[5], tr.source_triangle, old_state);
        tr.children[ichild ++] = push_triangle(verts_idxs[1], verts_idxs[2], verts_idxs[3], tr.source_triangle, old_state);
        tr.children[ichild ++] = push_triangle(verts_idxs[3], verts_idxs[4], verts_idxs[5], tr.source_triangle, old_state);
        tr.children[ichild   ] = push_triangle(verts_idxs[1], verts_idxs[3], verts_idxs[5], tr.source_triangle, old_state);
        break;

    default:
        break;
    }

#ifndef NDEBUG
    assert(this->verify_triangle_neighbors(tr, neighbors));
    for (int i = 0; i <= tr.number_of_split_sides(); ++i) {
        Vec3i32 n = this->child_neighbors(tr, neighbors, i);
        assert(this->verify_triangle_neighbors(m_triangles[tr.children[i]], n));
    }
#endif // NDEBUG
}

bool TriangleSelector::has_facets(EnforcerBlockerType state) const
{
    for (const Triangle& tr : m_triangles)
        if (tr.valid() && ! tr.is_split() && tr.get_state() == state)
            return true;
    return false;
}

int TriangleSelector::num_facets(EnforcerBlockerType state) const
{
    int cnt = 0;
    for (const Triangle& tr : m_triangles)
        if (tr.valid() && ! tr.is_split() && tr.get_state() == state)
            ++ cnt;
    return cnt;
}

indexed_triangle_set TriangleSelector::get_facets(EnforcerBlockerType state) const
{
    indexed_triangle_set out;
    std::vector<int> vertex_map(m_vertices.size(), -1);
    for (const Triangle& tr : m_triangles) {
        if (tr.valid() && ! tr.is_split() && tr.get_state() == state) {
            stl_triangle_vertex_indices indices;
            for (int i=0; i<3; ++i) {
                int j = tr.verts_idxs[i];
                if (vertex_map[j] == -1) {
                    vertex_map[j] = int(out.vertices.size());
                    out.vertices.emplace_back(m_vertices[j].v);
                }
                indices[i] = vertex_map[j];
            }
            out.indices.emplace_back(indices);
        }
    }
    return out;
}

// BBS
void TriangleSelector::get_facets(std::vector<indexed_triangle_set>& facets_per_type) const
{
    facets_per_type.clear();

    int max_state = int(EnforcerBlockerType::NONE);
    for (const Triangle &tr : m_triangles)
        if (tr.valid() && !tr.is_split())
            max_state = std::max(max_state, int(tr.get_state()));

    for (int type = int(EnforcerBlockerType::NONE); type <= max_state; ++type) {
        facets_per_type.emplace_back();
        indexed_triangle_set& its = facets_per_type.back();
        std::vector<int> vertex_map(m_vertices.size(), -1);

        for (const Triangle& tr : m_triangles) {
            if (tr.valid() && !tr.is_split() && tr.get_state() == (EnforcerBlockerType)type) {
                stl_triangle_vertex_indices indices;
                for (int i = 0; i < 3; ++i) {
                    int j = tr.verts_idxs[i];
                    if (vertex_map[j] == -1) {
                        vertex_map[j] = int(its.vertices.size());
                        its.vertices.emplace_back(m_vertices[j].v);
                    }
                    indices[i] = vertex_map[j];
                }
                its.indices.emplace_back(indices);
            }
        }
    }
}

indexed_triangle_set TriangleSelector::get_facets_strict(EnforcerBlockerType state) const
{
    indexed_triangle_set out;

    size_t num_vertices = 0;
    for (const Vertex &v : m_vertices)
        if (v.ref_cnt > 0)
            ++ num_vertices;
    out.vertices.reserve(num_vertices);
    std::vector<int> vertex_map(m_vertices.size(), -1);
    for (size_t i = 0; i < m_vertices.size(); ++ i)
        if (const Vertex &v = m_vertices[i]; v.ref_cnt > 0) {
            vertex_map[i] = int(out.vertices.size());
            out.vertices.emplace_back(v.v);
        }

    for (int itriangle = 0; itriangle < m_orig_size_indices; ++ itriangle)
        this->get_facets_strict_recursive(m_triangles[itriangle], m_neighbors[itriangle], state, out.indices);

    for (auto &triangle : out.indices)
        for (int i = 0; i < 3; ++ i)
            triangle(i) = vertex_map[triangle(i)];

    return out;
}

void TriangleSelector::get_facets_strict_recursive(
    const Triangle                              &tr,
    const Vec3i32                                 &neighbors,
    EnforcerBlockerType                          state,
    std::vector<stl_triangle_vertex_indices>    &out_triangles) const
{
    if (tr.is_split()) {
        for (int i = 0; i <= tr.number_of_split_sides(); ++ i)
            this->get_facets_strict_recursive(
                m_triangles[tr.children[i]],
                this->child_neighbors(tr, neighbors, i),
                state, out_triangles);
    } else if (tr.get_state() == state)
        this->get_facets_split_by_tjoints({tr.verts_idxs[0], tr.verts_idxs[1], tr.verts_idxs[2]}, neighbors, out_triangles);
}

void TriangleSelector::get_facets_split_by_tjoints(const Vec3i32 &vertices, const Vec3i32 &neighbors, std::vector<stl_triangle_vertex_indices> &out_triangles) const
{
// 导出此三角形，但首先收集沿其边的T形接头顶点。
    Vec3i32 midpoints(
        this->triangle_midpoint(neighbors(0), vertices(1), vertices(0)),
        this->triangle_midpoint(neighbors(1), vertices(2), vertices(1)),
        this->triangle_midpoint(neighbors(2), vertices(0), vertices(2)));
    int splits = (midpoints(0) != -1) + (midpoints(1) != -1) + (midpoints(2) != -1);
    switch (splits) {
    case 0:
        // 直接输出此三角形。
        out_triangles.emplace_back(vertices(0), vertices(1), vertices(2));
        break;
    case 1:
    {
        // 分割成两个三角形
        int i = midpoints(0) != -1 ? 2 : midpoints(1) != -1 ? 0 : 1;
        int j = next_idx_modulo(i, 3);
        int k = next_idx_modulo(j, 3);
        this->get_facets_split_by_tjoints(
            { vertices(i), vertices(j), midpoints(j) },
            { neighbors(i),
              this->neighbor_child(neighbors(j), vertices(k), vertices(j), Partition::Second),
              -1 },
              out_triangles);
        this->get_facets_split_by_tjoints(
            { midpoints(j), vertices(k), vertices(i) },
            { this->neighbor_child(neighbors(j), vertices(k), vertices(j), Partition::First),
              neighbors(k),
              -1 },
              out_triangles);
        break;
    }
    case 2:
    {
        // 分割成三个三角形。
        int i = midpoints(0) == -1 ? 2 : midpoints(1) == -1 ? 0 : 1;
        int j = next_idx_modulo(i, 3);
        int k = next_idx_modulo(j, 3);
        this->get_facets_split_by_tjoints(
            { vertices(i), midpoints(i), midpoints(k) },
            { this->neighbor_child(neighbors(i), vertices(j), vertices(i), Partition::Second),
              -1,
              this->neighbor_child(neighbors(k), vertices(i), vertices(k), Partition::First) },
              out_triangles);
        this->get_facets_split_by_tjoints(
            { midpoints(i), vertices(j), midpoints(k) },
            { this->neighbor_child(neighbors(i), vertices(j), vertices(i), Partition::First),
              -1, -1 },
              out_triangles);
        this->get_facets_split_by_tjoints(
            { vertices(j), vertices(k), midpoints(k) },
            { neighbors(j),
              this->neighbor_child(neighbors(k), vertices(i), vertices(k), Partition::Second),
              -1 },
              out_triangles);
        break;
    }
    default:
        assert(splits == 3);
        // 分割成四个三角形。
        this->get_facets_split_by_tjoints(
            { vertices(0), midpoints(0), midpoints(2) },
            { this->neighbor_child(neighbors(0), vertices(1), vertices(0), Partition::Second),
              -1, 
              this->neighbor_child(neighbors(2), vertices(0), vertices(2), Partition::First) },
              out_triangles);
        this->get_facets_split_by_tjoints(
            { midpoints(0), vertices(1), midpoints(1) },
            { this->neighbor_child(neighbors(0), vertices(1), vertices(0), Partition::First),
              this->neighbor_child(neighbors(1), vertices(2), vertices(1), Partition::Second),
              -1 },
              out_triangles);
        this->get_facets_split_by_tjoints(
            { midpoints(1), vertices(2), midpoints(2) },
            { this->neighbor_child(neighbors(1), vertices(2), vertices(1), Partition::First),
              this->neighbor_child(neighbors(2), vertices(0), vertices(2), Partition::Second),
              -1 },
              out_triangles);
        out_triangles.emplace_back(midpoints);
        break;
    }
}

std::vector<Vec2i32> TriangleSelector::get_seed_fill_contour() const {
    std::vector<Vec2i32> edges_out;
    for (int facet_idx = 0; facet_idx < this->m_orig_size_indices; ++facet_idx) {
        const Vec3i32 neighbors = m_neighbors[facet_idx];
        assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors));
        this->get_seed_fill_contour_recursive(facet_idx, neighbors, neighbors, edges_out);
    }

    return edges_out;
}

void TriangleSelector::get_seed_fill_contour_recursive(const int facet_idx, const Vec3i32 &neighbors, const Vec3i32 &neighbors_propagated, std::vector<Vec2i32> &edges_out) const {
    assert(facet_idx != -1 && facet_idx < int(m_triangles.size()));
    assert(this->verify_triangle_neighbors(m_triangles[facet_idx], neighbors));
    const Triangle *tr = &m_triangles[facet_idx];
    if (!tr->valid())
        return;

    if (tr->is_split()) {
        int num_of_children = tr->number_of_split_sides() + 1;
        if (num_of_children != 1) {
            for (int i = 0; i < num_of_children; ++i) {
                assert(i < int(tr->children.size()));
                assert(tr->children[i] < int(m_triangles.size()));
                // 递归，深度优先搜索该三角形的子三角形。
                // 该三角形的所有子三角形都是通过拆分原始网格的一个源三角形创建的。
                const Vec3i32 child_neighbors = this->child_neighbors(*tr, neighbors, i);
                this->get_seed_fill_contour_recursive(tr->children[i], child_neighbors,
                                                      this->child_neighbors_propagated(*tr, neighbors_propagated, i, child_neighbors), edges_out);
            }
        }
    } else if (tr->is_selected_by_seed_fill()) {
        Vec3i32 vertices = {m_triangles[facet_idx].verts_idxs[0], m_triangles[facet_idx].verts_idxs[1], m_triangles[facet_idx].verts_idxs[2]};
        append_touching_edges(neighbors(0), vertices(1), vertices(0), edges_out);
        append_touching_edges(neighbors(1), vertices(2), vertices(1), edges_out);
        append_touching_edges(neighbors(2), vertices(0), vertices(2), edges_out);

        // 追加仅通过部分边接触三角形的边，这意味着这些三角形来自较低深度。
        for (int idx = 0; idx < 3; ++idx)
            if (int neighbor_tr_idx = neighbors_propagated(idx); neighbor_tr_idx != -1 && !m_triangles[neighbor_tr_idx].is_split() && !m_triangles[neighbor_tr_idx].is_selected_by_seed_fill())
                edges_out.emplace_back(vertices(idx), vertices(next_idx_modulo(idx, 3)));
    }
}

TriangleSelector::TriangleSplittingData TriangleSelector::serialize() const {
    // 网格的每个原始三角形都被分配一个编码其状态或如何分割的数字。
    // 每个三角形由4位(xxyy)或4位加一个或多个扩展半字节编码：
    // 叶三角形：xx = EnforcerBlockerType（仅值0、1、2。值3用作额外4位的指示符），yy = 0
    // 叶三角形：xx = 0b11, yy = 0b00, zzzz... = EnforcerBlockerType（减3后）以base-15块表示
    // 非叶节点：xx = 特殊边，yy = 分割边数
    // 这些位被逐位追加并形成一个64位整数。

    // 该函数返回从原始三角形索引到位流（编码状态和后代）的映射。

    // 使用显式函数对象来支持Serializer::serialize()的递归调用。
    // 这比之前使用类型擦除的std::function递归调用的实现更高效。
    // （std::function使用指针调用，而此实现直接调用）。
    struct Serializer {
        const TriangleSelector* triangle_selector;
        TriangleSplittingData data;

        void serialize(int facet_idx) {
            const Triangle& tr = triangle_selector->m_triangles[facet_idx];

            // 始终保存分割边数。对于未分割的三角形为零。
            int split_sides = tr.number_of_split_sides();
            assert(split_sides >= 0 && split_sides <= 3);

            data.bitstream.push_back(split_sides & 0b01);
            data.bitstream.push_back(split_sides & 0b10);

            if (split_sides) {
                // 如果此三角形已分割，保存哪条边被分割（如果是一条边分割）或保留（如果是两条边分割）。对于三条边分割，该值将被忽略。
                assert(tr.is_split() && split_sides > 0);
                assert(tr.special_side() >= 0 && tr.special_side() <= 3);
                data.bitstream.push_back(tr.special_side() & 0b01);
                data.bitstream.push_back(tr.special_side() & 0b10);
                // 现在保存所有子三角形。
                // 以相反顺序序列化，以与PrusaSlicer 2.3.1兼容。
                for (int child_idx = split_sides; child_idx >= 0; -- child_idx)
                    this->serialize(tr.children[child_idx]);
            } else {
                // 如果这是叶节点，我们最好保存其状态信息。
                int n = int(tr.get_state());
                if (n <= static_cast<size_t>(EnforcerBlockerType::ExtruderMax))
                    data.used_states[n] = true;

                if (n >= 3) {
                    // 存储"11"加一个或多个4位块(n - 3)，其中0b1111表示还有另一个块。
                    data.bitstream.insert(data.bitstream.end(), { true, true });
                    n -= 3;
                    while (n >= 15) {
                        for (size_t bit_idx = 0; bit_idx < 4; ++bit_idx)
                            data.bitstream.push_back(uint64_t(0b1111) & (uint64_t(0b0001) << bit_idx));
                        n -= 15;
                    }
                    for (size_t bit_idx = 0; bit_idx < 4; ++bit_idx)
                        data.bitstream.push_back(n & (uint64_t(0b0001) << bit_idx));
                } else {
                    // 简单情况，与PrusaSlicer 2.3.1及更早版本兼容，用于存储在支撑和接缝上的涂色。
                    // 存储n的2位。
                    data.bitstream.push_back(n & 0b01);
                    data.bitstream.push_back(n & 0b10);
                }
            }
        }
    } out { this };

    out.data.triangles_to_split.reserve(m_orig_size_indices);
    for (int i=0; i<m_orig_size_indices; ++i)
        if (const Triangle& tr = m_triangles[i]; tr.is_split() || tr.get_state() != EnforcerBlockerType::NONE) {
            // 存储分配给第i个三角形的第一个位的索引。
            out.data.triangles_to_split.emplace_back(i, int(out.data.bitstream.size()));
            // 输出三角形位。
            out.serialize(i);
        }

    // 可能存储到撤销/重做堆栈上，因此节省内存。
    out.data.triangles_to_split.shrink_to_fit();
    out.data.bitstream.shrink_to_fit();
    return out.data;
}

void TriangleSelector::deserialize(const TriangleSplittingData& data,
                                   bool                         needs_reset,
                                   EnforcerBlockerType          max_ebt,
                                   EnforcerBlockerType          to_delete_filament,
                                   EnforcerBlockerType          replace_filament,
                                   const EnforcerBlockerStateMap* state_map)
{
    if (needs_reset)
        reset(); // dump any current state
    for (auto [triangle_id, ibit] : data.triangles_to_split) {
        if (triangle_id >= int(m_triangles.size())) {
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "array bound:error:triangle_id >= int(m_triangles.size())";
            return;
        }
    }
    // 按照每个三角形保存4位来预留三角形数量。
    // 对于MMU涂色，此估计可能偏低，但聊胜于无。
    m_triangles.reserve(std::max(m_mesh.its.indices.size(), data.bitstream.size() / 4));
    // 在亏格为0的大型流形网格上，三角形数量是顶点数量的两倍。
    // 这里三角形计数同时计入节点和叶节点，因此以下行可能会高估数量。
    m_vertices.reserve(std::max(m_mesh.its.vertices.size(), m_triangles.size() / 2));

    // 存储所有有后代的父节点的向量。
    struct ProcessingInfo {
        int facet_id = 0;
        Vec3i32 neighbors { -1, -1, -1 };
        int processed_children = 0;
        int total_children = 0;
    };
    // 源网格三角形及其子节点的深度优先队列。
    // 保持在循环外部以避免在循环内重新分配。
    std::vector<ProcessingInfo> parents;

    for (auto [triangle_id, ibit] : data.triangles_to_split) {
        assert(triangle_id < int(m_triangles.size()));
        assert(ibit < int(data.bitstream.size()));
        auto next_nibble = [&data, &ibit = ibit]() {
            int n = 0;
            for (int i = 0; i < 4; ++ i)
                n |= data.bitstream[ibit ++] << i;
            return n;
        };

        parents.clear();
        while (true) {
            // 读取下一个三角形信息。
            int code = next_nibble();
            int num_of_split_sides = code & 0b11;
            int num_of_children = num_of_split_sides == 0 ? 0 : num_of_split_sides + 1;
            bool is_split = num_of_children != 0;
            // 仅在非is_split时有效。第二个半字节的值减去了3，因此加回来。
            // auto state = is_split ? EnforcerBlockerType::NONE : EnforcerBlockerType((code & 0b1100) == 0b1100 ? next_nibble() + 3 : code >> 2);
            auto state = EnforcerBlockerType::NONE;
            //// BBS
            //if (state > max_ebt)
            //    state = EnforcerBlockerType::NONE;

            if (!is_split) {
                if ((code & 0b1100) == 0b1100) {
                    int next_code = next_nibble();
                    int num       = 0;
                    while (next_code == 0b1111) {
                        num++;
                        next_code = next_nibble();
                    }
                    state = EnforcerBlockerType(next_code + 15 * num + 3); // old:next_nibble() + 3;
                } else {
                    state = EnforcerBlockerType(code >> 2);
                }
            }

            if (state_map != nullptr && state != EnforcerBlockerType::NONE) {
                const size_t state_idx = static_cast<size_t>(state);
                state = state_idx < state_map->size() ? (*state_map)[state_idx] : EnforcerBlockerType::NONE;
            } else {
                if (state == to_delete_filament)
                    state = replace_filament;
                else if (to_delete_filament != EnforcerBlockerType::NONE && state != EnforcerBlockerType::NONE) {
                    state = state > to_delete_filament ? EnforcerBlockerType((int) state - 1) : state;
                }
            }

            if (state > max_ebt) {
                assert(false);
                state = EnforcerBlockerType::NONE;
            }

            // 仅在is_split时有效。
            int special_side = code >> 2;

            // 单独处理第一次迭代，以便使其他迭代的处理更简单。
            if (parents.empty()) {
                if (is_split) {
                    // 根节点已分割，将其添加到父节点列表并分割它。
                    // 然后继续下一个。
                    Vec3i32 neighbors = m_neighbors[triangle_id];
                    parents.push_back({triangle_id, neighbors, 0, num_of_children});
                    m_triangles[triangle_id].set_division(num_of_split_sides, special_side);
                    perform_split(triangle_id, neighbors, EnforcerBlockerType::NONE);
                    continue;
                } else {
                    // 根节点未分割。只需设置状态即可。
                    m_triangles[triangle_id].set_state(state);
                    break;
                }
            }

            // 这不是第一次迭代。此三角形是上一个父节点的子节点。
            assert(! parents.empty());
            assert(parents.back().processed_children < parents.back().total_children);

            if (ProcessingInfo& last = parents.back();  is_split) {
                // 分割三角形并将其保存为后续子节点的父节点。
                const Triangle &tr = m_triangles[last.facet_id];
                int   child_idx = last.total_children - last.processed_children - 1;
                Vec3i32 neighbors = this->child_neighbors(tr, last.neighbors, child_idx);
                int this_idx = tr.children[child_idx];
                m_triangles[this_idx].set_division(num_of_split_sides, special_side);
                perform_split(this_idx, neighbors, EnforcerBlockerType::NONE);
                parents.push_back({this_idx, neighbors, 0, num_of_children});
            } else {
                // 此三角形属于最后一个分割的三角形
                int child_idx = last.total_children - last.processed_children - 1;
                m_triangles[m_triangles[last.facet_id].children[child_idx]].set_state(state);
                ++last.processed_children;
            }

            // 如果前一个父三角形的所有子节点都已认领，则移动到祖父节点。
            while (parents.back().processed_children == parents.back().total_children) {
                parents.pop_back();

                if (parents.empty())
                    break;

                // 增加祖父节点的子节点计数器，因为我们刚刚完成了该分支并返回到这里。
                ++parents.back().processed_children;
            }

            // 如果我们弹出回到了根节点，就应该完成了。
            if (parents.empty())
                break;
        }
    }
}

void TriangleSelector::TriangleSplittingData::update_used_states(const size_t bitstream_start_idx) {
    assert(bitstream_start_idx < this->bitstream.size());
    assert(!this->bitstream.empty() && this->bitstream.size() != bitstream_start_idx);
    assert((this->bitstream.size() - bitstream_start_idx) % 4 == 0);

    if (this->bitstream.empty() || this->bitstream.size() == bitstream_start_idx)
        return;

    size_t nibble_idx = bitstream_start_idx;

    auto read_next_nibble = [&data_bitstream = std::as_const(this->bitstream), &nibble_idx]() -> uint8_t {
        assert(nibble_idx + 3 < data_bitstream.size());
        uint8_t code = 0;
        for (size_t bit_idx = 0; bit_idx < 4; ++bit_idx)
            code |= data_bitstream[nibble_idx++] << bit_idx;
        return code;
    };

    while (nibble_idx < this->bitstream.size()) {
        const uint8_t code = read_next_nibble();

        if (const bool is_split = (code & 0b11) != 0; is_split)
            continue;

        size_t facet_state = code >> 2;
        if ((code & 0b1100) == 0b1100) {
            size_t extension_count = 0;
            size_t next_code = read_next_nibble();
            while (next_code == 0b1111) {
                ++extension_count;
                next_code = read_next_nibble();
            }
            facet_state = next_code + 15 * extension_count + 3;
        }
        assert(facet_state < this->used_states.size());
        if (facet_state >= this->used_states.size())
            continue;

        this->used_states[facet_state] = true;
    }
}

// 轻量级反序列化变体，仅测试是否存在test_state的面。
bool TriangleSelector::has_facets(const TriangleSplittingData &data, const EnforcerBlockerType test_state) {
    // 若干未访问子节点的深度优先队列。
    // 保持在循环外部以避免在循环内重新分配。
    std::vector<int> parents_children;
    parents_children.reserve(64);

    for (const TriangleBitStreamMapping &triangle_id_and_ibit : data.triangles_to_split) {
        int ibit = triangle_id_and_ibit.bitstream_start_idx;
        assert(ibit < int(data.bitstream.size()));
        auto next_nibble = [&data, &ibit = ibit]() {
            int n = 0;
            for (int i = 0; i < 4; ++ i)
                n |= data.bitstream[ibit ++] << i;
            return n;
        };
        // < 0 -> 子节点数量的相反数
        // >= 0 -> 状态
        auto num_children_or_state = [&next_nibble]() -> int {
            int code               = next_nibble();
            int num_of_split_sides = code & 0b11;
            if (num_of_split_sides != 0)
                return - num_of_split_sides - 1;

            if ((code & 0b1100) != 0b1100)
                return code >> 2;

            int extension_count = 0;
            int next_code = next_nibble();
            while (next_code == 0b1111) {
                ++extension_count;
                next_code = next_nibble();
            }
            return next_code + 15 * extension_count + 3;
        };

        int state = num_children_or_state();
        if (state < 0) {
            // 根节点已分割。
            parents_children.clear();
            parents_children.emplace_back(- state);
            do {
                if (-- parents_children.back() >= 0) {
                    int state = num_children_or_state();
                    if (state < 0)
                        // 子节点已分割。
                        parents_children.emplace_back(- state);
                    else if (state == int(test_state))
                        // 子节点未分割且找到了test_state的面。
                        return true;
                } else
                    parents_children.pop_back();
            } while (! parents_children.empty());
        } else if (state == int(test_state))
            // 根节点未分割且找到了test_state的面。
            return true;
    }

    return false;
}

void TriangleSelector::seed_fill_unselect_all_triangles()
{
    for (Triangle &triangle : m_triangles)
        if (!triangle.is_split())
            triangle.unselect_by_seed_fill();
}

void TriangleSelector::seed_fill_apply_on_triangles(EnforcerBlockerType new_state)
{
    for (Triangle &triangle : m_triangles)
        if (!triangle.is_split() && triangle.is_selected_by_seed_fill())
            triangle.set_state(new_state);

    for (Triangle &triangle : m_triangles)
        if (triangle.is_split() && triangle.valid()) {
            size_t facet_idx = &triangle - &m_triangles.front();
            remove_useless_children(int(facet_idx));
        }
}

TriangleSelector::Cursor::Cursor(const Vec3f &source_, float radius_world, const Transform3d &trafo_, const ClippingPlane &clipping_plane_)
    : source{source_}, trafo{trafo_.cast<float>()}, clipping_plane{clipping_plane_}
{
    Vec3d sf = Geometry::Transformation(trafo_).get_scaling_factor();
    if (is_approx(sf(0), sf(1)) && is_approx(sf(1), sf(2))) {
        radius          = float(radius_world / sf(0));
        radius_sqr      = float(Slic3r::sqr(radius_world / sf(0)));
        uniform_scaling = true;
    } else {
        // In case that the transformation is non-uniform, all checks whether
        // something is inside the cursor should be done in world coords.
        // First transform source in world coords and remember that we did this.
        source          = trafo * source;
        uniform_scaling = false;
        radius          = radius_world;
        radius_sqr      = Slic3r::sqr(radius_world);
        trafo_normal    = trafo.linear().inverse().transpose();
    }
}

TriangleSelector::SinglePointCursor::SinglePointCursor(const Vec3f& center_, const Vec3f& source_, float radius_world, const Transform3d& trafo_, const ClippingPlane &clipping_plane_)
    : center{center_}, Cursor(source_, radius_world, trafo_, clipping_plane_)
{
    // 如果变换是非均匀的，所有关于某物是否在光标内的检查都应在世界坐标中进行。
    // 因为中心点被变换了。
    if (!uniform_scaling)
        center = trafo * center;

    // 计算dir，使用适当的任何坐标。
    dir = (center - source).normalized();
}

TriangleSelector::DoublePointCursor::DoublePointCursor(const Vec3f &first_center_, const Vec3f &second_center_, const Vec3f &source_, float radius_world, const Transform3d &trafo_, const ClippingPlane &clipping_plane_)
    : first_center{first_center_}, second_center{second_center_}, Cursor(source_, radius_world, trafo_, clipping_plane_)
{
    if (!uniform_scaling) {
        first_center  = trafo * first_center_;
        second_center = trafo * second_center_;
    }

    // 计算dir，使用适当的任何坐标。
    dir = (first_center - source).normalized();
}

// 如果裁剪平面未激活或点未被裁剪平面裁剪，则返回true。
inline static bool is_mesh_point_not_clipped(const Vec3f &point, const TriangleSelector::ClippingPlane &clipping_plane)
{
    return !clipping_plane.is_active() || !clipping_plane.is_mesh_point_clipped(point);
}

// 点（在网格坐标中）是否在球体光标内？
bool TriangleSelector::Sphere::is_mesh_point_inside(const Vec3f &point) const
{
    const Vec3f transformed_point = uniform_scaling ? point : Vec3f(trafo * point);
    if ((center - transformed_point).squaredNorm() < radius_sqr)
        return is_mesh_point_not_clipped(point, clipping_plane);

    return false;
}

// 点（在网格坐标中）是否在圆形光标内？
bool TriangleSelector::Circle::is_mesh_point_inside(const Vec3f &point) const
{
    const Vec3f transformed_point = uniform_scaling ? point : Vec3f(trafo * point);
    const Vec3f diff              = center - transformed_point;

    if ((diff - diff.dot(dir) * dir).squaredNorm() < radius_sqr)
        return is_mesh_point_not_clipped(point, clipping_plane);

    return false;
}

// 点（在网格坐标中）是否在3D胶囊光标内？
bool TriangleSelector::Capsule3D::is_mesh_point_inside(const Vec3f &point) const
{
    const Vec3f transformed_point  = uniform_scaling ? point : Vec3f(trafo * point);
    const Vec3f first_center_diff  = this->first_center - transformed_point;
    const Vec3f second_center_diff = this->second_center - transformed_point;
    if (first_center_diff.squaredNorm() < this->radius_sqr || second_center_diff.squaredNorm() < this->radius_sqr)
        return is_mesh_point_not_clipped(point, clipping_plane);

    // 首先，检查点pt是否位于由first_center和second_center定义的平面之间。
    // 然后检查它是否在first_center和second_center之间的圆柱体内。
    const Vec3f centers_diff = this->second_center - this->first_center;
    if (first_center_diff.dot(centers_diff) <= 0.f && second_center_diff.dot(centers_diff) >= 0.f && (first_center_diff.cross(centers_diff).norm() / centers_diff.norm()) <= this->radius)
        return is_mesh_point_not_clipped(point, clipping_plane);

    return false;
}

// 点（在网格坐标中）是否在2D胶囊光标内？
bool TriangleSelector::Capsule2D::is_mesh_point_inside(const Vec3f &point) const
{
    const Vec3f transformed_point           = uniform_scaling ? point : Vec3f(trafo * point);
    const Vec3f first_center_diff           = this->first_center - transformed_point;
    const Vec3f first_center_diff_projected = first_center_diff - first_center_diff.dot(this->dir) * this->dir;
    if (first_center_diff_projected.squaredNorm() < this->radius_sqr)
        return is_mesh_point_not_clipped(point, clipping_plane);

    const Vec3f second_center_diff           = this->second_center - transformed_point;
    const Vec3f second_center_diff_projected = second_center_diff - second_center_diff.dot(this->dir) * this->dir;
    if (second_center_diff_projected.squaredNorm() < this->radius_sqr)
        return is_mesh_point_not_clipped(point, clipping_plane);

    const Vec3f centers_diff           = this->second_center - this->first_center;
    const Vec3f centers_diff_projected = centers_diff - centers_diff.dot(this->dir) * this->dir;

    // 首先，检查点是否位于first_center和second_center之间。
    if (first_center_diff_projected.dot(centers_diff_projected) <= 0.f && second_center_diff_projected.dot(centers_diff_projected) >= 0.f) {
        // 与以first_center为圆心的圆相交的矩形的|AD|线方向向量。
        const Vec3f rectangle_da_dir              = centers_diff.cross(this->dir);
        // 从first_center指向矩形点'A'的向量。
        const Vec3f first_center_rectangle_a_diff = rectangle_da_dir.normalized() * this->radius;
        const Vec3f rectangle_a                   = this->first_center - first_center_rectangle_a_diff;
        const Vec3f rectangle_d                   = this->first_center + first_center_rectangle_a_diff;
        // 现在检查点是否位于以first_center和second_center为圆心的圆之间的矩形内。
        if ((rectangle_a - transformed_point).dot(rectangle_da_dir) <= 0.f && (rectangle_d - transformed_point).dot(rectangle_da_dir) >= 0.f)
            return is_mesh_point_not_clipped(point, clipping_plane);
    }

    return false;
}

// p1, p2, p3 在网格坐标中！
static bool is_circle_pointer_inside_triangle(const Vec3f &p1_, const Vec3f &p2_, const Vec3f &p3_, const Vec3f &center, const Vec3f &dir, const bool uniform_scaling, const Transform3f &trafo) {
    const Vec3f& q1 = center + dir;
    const Vec3f& q2 = center - dir;

    auto signed_volume_sign = [](const Vec3f& a, const Vec3f& b,
                                 const Vec3f& c, const Vec3f& d) -> bool {
        return ((b-a).cross(c-a)).dot(d-a) > 0.;
    };

    // 如果对象非均匀缩放，则在世界坐标中进行检查。
    const Vec3f& p1 = uniform_scaling ? p1_ : Vec3f(trafo * p1_);
    const Vec3f& p2 = uniform_scaling ? p2_ : Vec3f(trafo * p2_);
    const Vec3f& p3 = uniform_scaling ? p3_ : Vec3f(trafo * p3_);

    if (signed_volume_sign(q1,p1,p2,p3) == signed_volume_sign(q2,p1,p2,p3))
        return false;

    bool pos = signed_volume_sign(q1,q2,p1,p2);
    return signed_volume_sign(q1,q2,p2,p3) == pos && signed_volume_sign(q1,q2,p3,p1) == pos;
}

// p1, p2, p3 在网格坐标中！
bool TriangleSelector::SinglePointCursor::is_pointer_in_triangle(const Vec3f &p1_, const Vec3f &p2_, const Vec3f &p3_) const
{
    return is_circle_pointer_inside_triangle(p1_, p2_, p3_, center, dir, uniform_scaling, trafo);
}

// p1, p2, p3 在网格坐标中！
bool TriangleSelector::DoublePointCursor::is_pointer_in_triangle(const Vec3f &p1_, const Vec3f &p2_, const Vec3f &p3_) const
{
    return is_circle_pointer_inside_triangle(p1_, p2_, p3_, first_center, dir, uniform_scaling, trafo) ||
           is_circle_pointer_inside_triangle(p1_, p2_, p3_, second_center, dir, uniform_scaling, trafo);
}

bool line_plane_intersection(const Vec3f &line_a, const Vec3f &line_b, const Vec3f &plane_origin, const Vec3f &plane_normal, Vec3f &out_intersection)
{
    Vec3f line_dir      = line_b - line_a;
    float t_denominator = plane_normal.dot(line_dir);
    if (t_denominator == 0.f)
        return false;

    // 使用平面上的某点（原点）计算平面方程中的'd'
    float plane_d = plane_normal.dot(plane_origin);
    if (float t = (plane_d - plane_normal.dot(line_a)) / t_denominator; t >= 0.f && t <= 1.f) {
        out_intersection = line_a + t * line_dir;
        return true;
    }

    return false;
}

bool TriangleSelector::Capsule3D::is_edge_inside_cursor(const Triangle &tr, const std::vector<Vertex> &vertices) const
{
    std::array<Vec3f, 3> pts;
    for (int i = 0; i < 3; ++i) {
        pts[i] = vertices[tr.verts_idxs[i]].v;
        if (!this->uniform_scaling)
            pts[i] = this->trafo * pts[i];
    }

    for (int side = 0; side < 3; ++side) {
        const Vec3f &edge_a = pts[side];
        const Vec3f &edge_b = pts[side < 2 ? side + 1 : 0];
        if (test_line_inside_capsule(edge_a, edge_b, this->first_center, this->second_center, this->radius))
            return true;
    }

    return false;
}

// 边是否在光标内？
bool TriangleSelector::Capsule2D::is_edge_inside_cursor(const Triangle &tr, const std::vector<Vertex> &vertices) const
{
    std::array<Vec3f, 3> pts;
    for (int i = 0; i < 3; ++i) {
        pts[i] = vertices[tr.verts_idxs[i]].v;
        if (!this->uniform_scaling)
            pts[i] = this->trafo * pts[i];
    }

    const Vec3f centers_diff                  = this->second_center - this->first_center;
    // 与以first_center为圆心的圆相交的矩形的|AD|线方向向量。
    const Vec3f rectangle_da_dir              = centers_diff.cross(this->dir);
    // 从first_center指向矩形点'A'的向量。
    const Vec3f first_center_rectangle_a_diff = rectangle_da_dir.normalized() * this->radius;
    const Vec3f rectangle_a                   = this->first_center - first_center_rectangle_a_diff;
    const Vec3f rectangle_d                   = this->first_center + first_center_rectangle_a_diff;

    auto edge_inside_rectangle = [&self = std::as_const(*this), &centers_diff](const Vec3f &edge_a, const Vec3f &edge_b, const Vec3f &plane_origin, const Vec3f &plane_normal) -> bool {
        Vec3f intersection(-1.f, -1.f, -1.f);
        if (line_plane_intersection(edge_a, edge_b, plane_origin, plane_normal, intersection)) {
            // 现在检查交点是否在矩形内。这意味着它在'first_center'和'second_center'之间，或'A'和'B'之间。
            if (self.first_center.dot(centers_diff) <= intersection.dot(centers_diff) && intersection.dot(centers_diff) <= self.second_center.dot(centers_diff))
                return true;
        }
        return false;
    };

    for (int side = 0; side < 3; ++side) {
        const Vec3f &edge_a     = pts[side];
        const Vec3f &edge_b     = pts[side < 2 ? side + 1 : 0];
        const Vec3f  edge_dir   = edge_b - edge_a;
        const Vec3f  edge_dir_n = edge_dir.normalized();

        float t1      = (this->first_center - edge_a).dot(edge_dir_n);
        float t2      = (this->second_center - edge_a).dot(edge_dir_n);
        Vec3f vector1 = edge_a + t1 * edge_dir_n - this->first_center;
        Vec3f vector2 = edge_a + t2 * edge_dir_n - this->second_center;

        // vector1和vector2是从中心到交点的3D向量。我们要测量的是其在垂直于dir的平面上的投影长度。
        if (float dist = vector1.squaredNorm() - std::pow(vector1.dot(this->dir), 2.f); dist < this->radius_sqr && t1 >= 0.f && t1 <= edge_dir.norm())
            return true;

        if (float dist = vector2.squaredNorm() - std::pow(vector2.dot(this->dir), 2.f); dist < this->radius_sqr && t2 >= 0.f && t2 <= edge_dir.norm())
            return true;

        // 检查边是否穿过first_center和second_center之间的矩形。
        if (edge_inside_rectangle(edge_a, edge_b, rectangle_a, (rectangle_d - rectangle_a)) || edge_inside_rectangle(edge_a, edge_b, rectangle_d, (rectangle_a - rectangle_d)))
            return true;
    }

    return false;
}

} // namespace Slic3r
