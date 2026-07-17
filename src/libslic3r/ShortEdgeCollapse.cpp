#include "ShortEdgeCollapse.hpp"
#include "libslic3r/NormalUtils.hpp"

#include <unordered_map>
#include <unordered_set>
#include <random>
#include <algorithm>

namespace Slic3r {

void its_short_edge_collpase(indexed_triangle_set &mesh, size_t target_triangle_count) {
    // 每当顶点被移除时，其映射更新为与之合并的顶点索引
    std::vector<size_t> vertices_index_mapping(mesh.vertices.size());
    for (size_t idx = 0; idx < vertices_index_mapping.size(); ++idx) {
        vertices_index_mapping[idx] = idx;
    }
    // 算法使用get_final_index查询来获取实际顶点索引。该查询还会沿途更新所有映射，本质上是展平映射
    std::vector<size_t> flatten_queue;
    auto get_final_index = [&vertices_index_mapping, &flatten_queue](const size_t &orig_index) {
        flatten_queue.clear();
        size_t idx = orig_index;
        while (vertices_index_mapping[idx] != idx) {
            flatten_queue.push_back(idx);
            idx = vertices_index_mapping[idx];
        }
        for (size_t i : flatten_queue) {
            vertices_index_mapping[i] = idx;
        }
        return idx;

    };

    // 如果面被移除，在此标记
    std::vector<bool> face_removal_flags(mesh.indices.size(), false);

    std::vector<Vec3i32> triangles_neighbors = its_face_neighbors_par(mesh);

    // 计算顶点点积 - 在边折叠期间使用，
    // 确定要移除哪个顶点和保留哪个顶点；我们尝试保留角度较大的那个，因为它"更"能定义形状。
    // 最小顶点点积是其法线与周围面法线的最低点积。
    // 点积越低，我们越想保留该顶点
    // 注意：即使精简改变了网格，此分数也不会更新。这节省了计算时间，且没有充分的理由去更新。
    std::vector<float> min_vertex_dot_product(mesh.vertices.size(), 1);
    {
        std::vector<Vec3f> face_normals = its_face_normals(mesh);
        std::vector<Vec3f> vertex_normals = NormalUtils::create_normals(mesh);

        for (size_t face_idx = 0; face_idx < mesh.indices.size(); ++face_idx) {
            Vec3i32 t = mesh.indices[face_idx];
            Vec3f n = face_normals[face_idx];
            min_vertex_dot_product[t[0]] = std::min(min_vertex_dot_product[t[0]], n.dot(vertex_normals[t[0]]));
            min_vertex_dot_product[t[1]] = std::min(min_vertex_dot_product[t[1]], n.dot(vertex_normals[t[1]]));
            min_vertex_dot_product[t[2]] = std::min(min_vertex_dot_product[t[2]], n.dot(vertex_normals[t[2]]));
        }
    }

    // 删除面的lambda函数。它将面标记为已删除，并更新邻居信息
    auto remove_face = [&triangles_neighbors, &face_removal_flags](int face_idx, int other_face_idx) {
        if (face_idx < 0) {
            return;
        }
        face_removal_flags[face_idx] = true;
        Vec3i32 neighbors = triangles_neighbors[face_idx];
        int n_a = neighbors[0] != other_face_idx ? neighbors[0] : neighbors[1];
        int n_b = neighbors[2] != other_face_idx ? neighbors[2] : neighbors[1];
        if (n_a > 0)
            for (int &n : triangles_neighbors[n_a]) {
                if (n == face_idx) {
                    n = n_b;
                    break;
                }
            }
        if (n_b > 0)
            for (int &n : triangles_neighbors[n_b]) {
                if (n == face_idx) {
                    n = n_a;
                    break;
                }
            }
    };

    std::mt19937_64 generator { 27644437 };// 默认常量种子！使结果具有确定性
    std::vector<size_t> face_indices(mesh.indices.size());
    for (size_t idx = 0; idx < face_indices.size(); ++idx) {
        face_indices[idx] = idx;
    }
    // 临时面索引，仅用于交换
    std::vector<size_t> tmp_face_indices(mesh.indices.size());

    float decimation_ratio = 1.0f; // 每次迭代更新的精简比率。它是移除的三角形数/总数
    float edge_len = 0.2f; // 允许折叠的边长。从较小值开始，逐渐增大

    while (face_indices.size() > target_triangle_count) {
        // 简单的增加边长的函数 - 如果精简比率低，则增加边长最多两倍，如果精简比率高，则增量较小
        edge_len = edge_len * (1.0f + 1.0 - decimation_ratio);
        float max_edge_len_squared = edge_len * edge_len;

        // 打乱面并以随机顺序遍历，这会极大地提高结果质量
        std::shuffle(face_indices.begin(), face_indices.end(), generator);
        
        int allowed_face_removals = int(face_indices.size()) - int(target_triangle_count);
        for (const size_t &face_idx : face_indices) {
            if (face_removal_flags[face_idx]) {
                // 如果面已从之前的折叠中移除，则跳过（每次折叠至少移除两个三角形）
                continue;
            }

            // 检查每条边是否为折叠的好候选
            for (size_t edge_idx = 0; edge_idx < 3; ++edge_idx) {
                size_t vertex_index_keep = get_final_index(mesh.indices[face_idx][edge_idx]);
                size_t vertex_index_remove = get_final_index(mesh.indices[face_idx][(edge_idx + 1) % 3]);
                // 检查距离，跳过较长边
                if ((mesh.vertices[vertex_index_keep] - mesh.vertices[vertex_index_remove]).squaredNorm()
                        > max_edge_len_squared) {
                    continue;
                }
                // 如果vertex_index_keep有更高的点积则交换索引（我们想要保留低点积的顶点）
                if (min_vertex_dot_product[vertex_index_remove] < min_vertex_dot_product[vertex_index_keep]) {
                    size_t tmp = vertex_index_keep;
                    vertex_index_keep = vertex_index_remove;
                    vertex_index_remove = tmp;
                }

                // 移除顶点
                {
                    // 将其索引映射到保留顶点的索引
                    vertices_index_mapping[vertex_index_remove] = vertices_index_mapping[vertex_index_keep];
                }

                int neighbor_to_remove_face_idx = triangles_neighbors[face_idx][edge_idx];
                // 移除面
                remove_face(face_idx, neighbor_to_remove_face_idx);
                remove_face(neighbor_to_remove_face_idx, face_idx);
                allowed_face_removals-=2;

                // 跳出。这个三角形已完成
                break;
            }

            if (allowed_face_removals <= 0) { break; }
        }

        // 过滤face_indices，移除已折叠的
        size_t prev_size = face_indices.size();
        tmp_face_indices.clear();
        for (size_t face_idx : face_indices) {
            if (!face_removal_flags[face_idx]){
                tmp_face_indices.push_back(face_idx);
            }
        }
        face_indices.swap(tmp_face_indices);

        decimation_ratio = float(prev_size - face_indices.size()) / float(prev_size);
        //std::cout << " DECIMATION RATIO: " << decimation_ratio << std::endl;
    }

    // 提取结果网格
    std::unordered_map<size_t, size_t> final_vertices_mapping;
    std::vector<Vec3f> final_vertices;
    std::vector<Vec3i32> final_indices;
    final_indices.reserve(face_indices.size());
    for (size_t idx : face_indices) {
        Vec3i32 final_face;
        for (size_t i = 0; i < 3; ++i) {
            final_face[i] = get_final_index(mesh.indices[idx][i]);
        }
        if (final_face[0] == final_face[1] || final_face[1] == final_face[2] || final_face[2] == final_face[0]) {
            continue; // 丢弃退化三角形
        }

        for (size_t i = 0; i < 3; ++i) {
            if (final_vertices_mapping.find(final_face[i]) == final_vertices_mapping.end()) {
                final_vertices_mapping[final_face[i]] = final_vertices.size();
                final_vertices.push_back(mesh.vertices[final_face[i]]);
            }
            final_face[i] = final_vertices_mapping[final_face[i]];
        }

        final_indices.push_back(final_face);
    }

    mesh.vertices = final_vertices;
    mesh.indices = final_indices;
}

} //namespace Slic3r

