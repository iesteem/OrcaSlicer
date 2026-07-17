// Copyright (c) 2023 UltiMaker
// CuraEngine 根据 AGPLv3 或更高版本的条款发布。

#include "InterlockingGenerator.hpp"

namespace std {
template<> struct hash<Slic3r::GridPoint3>
{
    size_t operator()(const Slic3r::GridPoint3& pp) const noexcept
    {
        static int prime  = 31;
        int        result = 89;
        result            = static_cast<int>(result * prime + pp.x());
        result            = static_cast<int>(result * prime + pp.y());
        result            = static_cast<int>(result * prime + pp.z());
        return static_cast<size_t>(result);
    }
};
} // namespace std


namespace Slic3r {

void InterlockingGenerator::generate_interlocking_structure(PrintObject* print_object)
{
    const auto& config = print_object->config();
    if (!config.interlocking_beam) {
        return;
    }

    const float    rotation           = Geometry::deg2rad(config.interlocking_orientation.value);
    const coord_t  beam_layer_count   = config.interlocking_beam_layer_count;
    const int      interface_depth    = config.interlocking_depth;
    const int      boundary_avoidance = config.interlocking_boundary_avoidance;
    const coord_t  beam_width         = scaled(config.interlocking_beam_width.value);

    const DilationKernel interface_dilation(GridPoint3(interface_depth, interface_depth, interface_depth), DilationKernel::Type::PRISM);

    const bool           air_filtering = boundary_avoidance > 0;
    const DilationKernel air_dilation(GridPoint3(boundary_avoidance, boundary_avoidance, boundary_avoidance), DilationKernel::Type::PRISM);

    const coord_t cell_width = beam_width + beam_width;
    const Vec3crd cell_size(cell_width, cell_width, 2 * beam_layer_count);

    for (size_t region_a_index = 0; region_a_index < print_object->num_printing_regions(); region_a_index++) {
        const PrintRegion& region_a      = print_object->printing_region(region_a_index);
        const auto         extruder_nr_a = region_a.extruder(FlowRole::frExternalPerimeter);

        for (size_t region_b_index = region_a_index + 1; region_b_index < print_object->num_printing_regions(); region_b_index++) {
            const PrintRegion& region_b      = print_object->printing_region(region_b_index);
            const auto         extruder_nr_b = region_b.extruder(FlowRole::frExternalPerimeter);
            if (extruder_nr_a == extruder_nr_b) {
                continue;
            }

            InterlockingGenerator gen(*print_object, region_a_index, region_b_index, beam_width, boundary_avoidance, rotation, cell_size, beam_layer_count,
                                      interface_dilation, air_dilation, air_filtering);
            gen.generateInterlockingStructure();
        }
    }
}

std::pair<ExPolygons, ExPolygons> InterlockingGenerator::growBorderAreasPerpendicular(const ExPolygons& a, const ExPolygons& b, const coord_t& detect) const
{
    const coord_t min_line =
        std::min(print_object.printing_region(region_a_index).flow(print_object, frExternalPerimeter, 0.1).scaled_width(),
                 print_object.printing_region(region_b_index).flow(print_object, frExternalPerimeter, 0.1).scaled_width());

    const ExPolygons total_shrunk = offset_ex(union_ex(offset_ex(a, min_line), offset_ex(b, min_line)), 2 * -min_line);

    ExPolygons from_border_a = diff_ex(a, total_shrunk);
    ExPolygons from_border_b = diff_ex(b, total_shrunk);

    ExPolygons temp_a, temp_b;
    for (coord_t i = 0; i < (detect / min_line) + 2; ++i) {
        temp_a        = offset_ex(from_border_a, min_line);
        temp_b        = offset_ex(from_border_b, min_line);
        from_border_a = diff_ex(temp_a, temp_b);
        from_border_b = diff_ex(temp_b, temp_a);
    }

    return {from_border_a, from_border_b};
}

void InterlockingGenerator::handleThinAreas(const std::unordered_set<GridPoint3>& has_all_meshes) const
{
    const coord_t     number_of_beams_detect = boundary_avoidance;
    const coord_t     number_of_beams_expand = boundary_avoidance - 1;
    constexpr coord_t rounding_errors        = 5;

    const coord_t max_beam_width = beam_width;
    const coord_t detect         = (max_beam_width * number_of_beams_detect) + rounding_errors;
    const coord_t expand         = (max_beam_width * number_of_beams_expand) + rounding_errors;
    const coord_t close_gaps =
        std::min(print_object.printing_region(region_a_index).flow(print_object, frExternalPerimeter, 0.1).scaled_width(),
                 print_object.printing_region(region_b_index).flow(print_object, frExternalPerimeter, 0.1).scaled_width()) / 4;

    //// 制作包含多边形，以便仅实际处理靠近实际微结构的薄区域（因此不在蒙皮中）。
    std::vector<Polygons> near_interlock_per_layer;
    near_interlock_per_layer.assign(print_object.layer_count(), Polygons());
    for (const auto& cell : has_all_meshes) {
        const auto bottom_corner = vu.toLowerCorner(cell);
        for (coord_t layer_nr = bottom_corner.z();
             layer_nr < bottom_corner.z() + cell_size.z() && layer_nr < static_cast<coord_t>(near_interlock_per_layer.size()); ++layer_nr) {
            near_interlock_per_layer[static_cast<size_t>(layer_nr)].push_back(vu.toPolygon(cell));
        }
    }
    for (auto& near_interlock : near_interlock_per_layer) {
        near_interlock = offset(union_(closing(near_interlock, rounding_errors)), detect);
        polygons_rotate(near_interlock, rotation);
    }

    //// 仅当两个网格中都存在时更改层，zip 应处理此情况。
    for (size_t layer_nr = 0; layer_nr < print_object.layer_count(); layer_nr++){
        auto       layer   = print_object.get_layer(layer_nr);
        ExPolygons polys_a = to_expolygons(layer->get_region(region_a_index)->slices.surfaces);
        ExPolygons polys_b = to_expolygons(layer->get_region(region_b_index)->slices.surfaces);

        const auto [from_border_a, from_border_b] = growBorderAreasPerpendicular(polys_a, polys_b, detect);

        //// 通过执行形态学开运算，获取每个网格中非薄（大）的区域。
        const ExPolygons large_a = opening_ex(polys_a, detect);
        const ExPolygons large_b = opening_ex(polys_b, detect);

        //// 从已有的信息中推导出薄区域需要扩展到的区域（所以添加到薄条带的区域）。
        const ExPolygons thin_expansion_a =
            offset_ex(intersection_ex(intersection_ex(intersection_ex(large_b, offset_ex(diff_ex(polys_a, large_a), expand)),
                                                      near_interlock_per_layer[layer_nr]),
                                      from_border_a),
                      rounding_errors);
        const ExPolygons thin_expansion_b =
            offset_ex(intersection_ex(intersection_ex(intersection_ex(large_a, offset_ex(diff_ex(polys_b, large_b), expand)),
                                                      near_interlock_per_layer[layer_nr]),
                                      from_border_b),
                      rounding_errors);

        //// 对面多边形的扩展薄区域应"侵入"该多边形的较大区域，
        // 反之亦然，将扩展添加到它们自己的薄区域。
        layer->get_region(region_a_index)->slices.set(closing_ex(diff_ex(union_ex(polys_a, thin_expansion_a), thin_expansion_b), close_gaps), stInternal);
        layer->get_region(region_b_index)->slices.set(closing_ex(diff_ex(union_ex(polys_b, thin_expansion_b), thin_expansion_a), close_gaps), stInternal);
    }
}

void InterlockingGenerator::generateInterlockingStructure() const
{
    std::vector<std::unordered_set<GridPoint3>> voxels_per_mesh = getShellVoxels(interface_dilation);

    std::unordered_set<GridPoint3>& has_any_mesh   = voxels_per_mesh[0];
    std::unordered_set<GridPoint3>& has_all_meshes = voxels_per_mesh[1];
    has_any_mesh.merge(has_all_meshes); // 执行并集和交集同时进行。消耗 voxels_per_mesh

    if (has_all_meshes.empty()) {
        return;
    }

    const std::vector<ExPolygons> layer_regions = computeUnionedVolumeRegions();

    if (air_filtering) {
        std::unordered_set<GridPoint3> air_cells;
        addBoundaryCells(layer_regions, air_dilation, air_cells);

        for (const GridPoint3& p : air_cells) {
            has_all_meshes.erase(p);
        }

        handleThinAreas(has_all_meshes);
    }

    applyMicrostructureToOutlines(has_all_meshes, layer_regions);
}

std::vector<std::unordered_set<GridPoint3>> InterlockingGenerator::getShellVoxels(const DilationKernel& kernel) const
{
    std::vector<std::unordered_set<GridPoint3>> voxels_per_mesh(2);

    // 标记包含某些边界的所有单元格
    for (size_t region_idx = 0; region_idx < 2; region_idx++)
    {
        const size_t region = (region_idx == 0) ? region_a_index : region_b_index;
        std::unordered_set<GridPoint3>& mesh_voxels = voxels_per_mesh[region_idx];

        std::vector<ExPolygons> rotated_polygons_per_layer(print_object.layer_count());
        for (size_t layer_nr = 0; layer_nr < print_object.layer_count(); layer_nr++)
        {
            auto layer = print_object.get_layer(layer_nr);
            rotated_polygons_per_layer[layer_nr] = to_expolygons(layer->get_region(region)->slices.surfaces);
            expolygons_rotate(rotated_polygons_per_layer[layer_nr], rotation);
        }

        addBoundaryCells(rotated_polygons_per_layer, kernel, mesh_voxels);
    }

    return voxels_per_mesh;
}

void InterlockingGenerator::addBoundaryCells(const std::vector<ExPolygons>&  layers,
                                             const DilationKernel&           kernel,
                                             std::unordered_set<GridPoint3>& cells) const
{
    auto voxel_emplacer = [&cells](GridPoint3 p) {
        if (p.z() < 0) {
            return true;
        }
        cells.emplace(p);
        return true;
    };

    for (size_t layer_nr = 0; layer_nr < layers.size(); layer_nr++) {
        const coord_t z = static_cast<coord_t>(layer_nr);
        vu.walkDilatedPolygons(layers[layer_nr], z, kernel, voxel_emplacer);
        ExPolygons skin = layers[layer_nr];
        if (layer_nr > 0) {
            skin = xor_ex(skin, layers[layer_nr - 1]);
        }
        skin = opening_ex(skin, cell_size.x() / 2.f); // 去除多余的小区域，这些区域也会因为 walkPolygons 而被包含
        vu.walkDilatedAreas(skin, z, kernel, voxel_emplacer);
    }
}

std::vector<ExPolygons> InterlockingGenerator::computeUnionedVolumeRegions() const
{
    const size_t max_layer_count = print_object.layer_count() +
                                   1; // 引入顶部幻影层以正确计算最顶层的蒙皮。
    std::vector<ExPolygons> layer_regions(max_layer_count);

    for (size_t layer_nr = 0; layer_nr < max_layer_count - 1; layer_nr++) {
        auto& layer_region = layer_regions[static_cast<size_t>(layer_nr)];
        for (size_t region_idx : {region_a_index, region_b_index}) {
            auto layer = print_object.get_layer(layer_nr);
            expolygons_append(layer_region, to_expolygons(layer->get_region(region_idx)->slices.surfaces));
        }
        layer_region = closing_ex(layer_region, ignored_gap_); // 形态学闭合以将网格合并为单个体积
        expolygons_rotate(layer_region, rotation);
    }
    return layer_regions;
}

std::vector<std::vector<ExPolygons>> InterlockingGenerator::generateMicrostructure() const
{
    std::vector<std::vector<ExPolygons>> cell_area_per_mesh_per_layer;
    cell_area_per_mesh_per_layer.resize(2);
    cell_area_per_mesh_per_layer[0].resize(2);
    const coord_t beam_w_sum = beam_width + beam_width;
    const coord_t middle     = cell_size.x() * beam_width / beam_w_sum;
    const coord_t width[2]   = {middle, cell_size.x() - middle};
    for (size_t mesh_idx : {0ul, 1ul}) {
        Point offset(mesh_idx ? middle : 0, 0);
        Point area_size(width[mesh_idx], cell_size.y());

        Polygon poly;
        poly.append(offset);
        poly.append(offset + Point(area_size.x(), 0));
        poly.append(offset + area_size);
        poly.append(offset + Point(0, area_size.y()));
        cell_area_per_mesh_per_layer[0][mesh_idx].emplace_back(poly);
    }
    cell_area_per_mesh_per_layer[1] = cell_area_per_mesh_per_layer[0];
    for (ExPolygons& polys : cell_area_per_mesh_per_layer[1]) {
        for (ExPolygon& poly : polys) {
            for (Point& p : poly.contour) {
                std::swap(p.x(), p.y());
            }
        }
    }
    return cell_area_per_mesh_per_layer;
}

void InterlockingGenerator::applyMicrostructureToOutlines(const std::unordered_set<GridPoint3>& cells,
                                                          const std::vector<ExPolygons>&        layer_regions) const
{
    std::vector<std::vector<ExPolygons>> cell_area_per_mesh_per_layer = generateMicrostructure();

    const float  unapply_rotation = -rotation;
    const size_t max_layer_count  = print_object.layer_count();

    std::vector<ExPolygons> structure_per_layer[2]; // 每个网格在每个层上的结构

    //// 每 `beam_layer_count` 层组合为一个互锁梁层
    //// 为存储这些我们需要 ceil(max_layer_count / beam_layer_count) 层
    //// 整数公式重写为 (max_layer_count + beam_layer_count - 1) / beam_layer_count，以进行整除
    size_t num_interlocking_layers = (max_layer_count + static_cast<size_t>(beam_layer_count) - 1ul) /
                                     static_cast<size_t>(beam_layer_count);
    structure_per_layer[0].resize(num_interlocking_layers);
    structure_per_layer[1].resize(num_interlocking_layers);

    //// 仅计算一半层的单元结构，因为我们的梁有两层高，结构的每个奇数层
    // 将与下一层相同。
    for (const GridPoint3& grid_loc : cells) {
        Vec3crd bottom_corner = vu.toLowerCorner(grid_loc);
        for (size_t mesh_idx = 0; mesh_idx < 2; mesh_idx++) {
            for (size_t layer_nr = bottom_corner.z(); layer_nr < bottom_corner.z() + cell_size.z() && layer_nr < max_layer_count;
                 layer_nr += beam_layer_count) {
                ExPolygons areas_here = cell_area_per_mesh_per_layer[static_cast<size_t>(layer_nr / beam_layer_count) %
                                                                cell_area_per_mesh_per_layer.size()][mesh_idx];
                for (auto & here : areas_here) {
                    here.translate(bottom_corner.x(), bottom_corner.y());
                }
                expolygons_append(structure_per_layer[mesh_idx][static_cast<size_t>(layer_nr / beam_layer_count)], areas_here);
            }
        }
    }

    for (size_t mesh_idx = 0; mesh_idx < 2; mesh_idx++) {
        for (size_t layer_nr = 0; layer_nr < structure_per_layer[mesh_idx].size(); layer_nr++) {
            ExPolygons& layer_structure = structure_per_layer[mesh_idx][layer_nr];
            layer_structure = union_ex(layer_structure);
            expolygons_rotate(layer_structure, unapply_rotation);
        }
    }

    for (size_t region_idx = 0; region_idx < 2; region_idx++) {
        const size_t region = (region_idx == 0) ? region_a_index : region_b_index;
        for (size_t layer_nr = 0; layer_nr < max_layer_count; layer_nr++) {
            ExPolygons layer_outlines = layer_regions[layer_nr];
            expolygons_rotate(layer_outlines, unapply_rotation);

            const ExPolygons areas_here = intersection_ex(structure_per_layer[region_idx][layer_nr / static_cast<size_t>(beam_layer_count)], layer_outlines);
            const ExPolygons& areas_other = structure_per_layer[!region_idx][layer_nr / static_cast<size_t>(beam_layer_count)];

            auto       layer  = print_object.get_layer(layer_nr);
            auto&      slices = layer->get_region(region)->slices;
            ExPolygons polys  = to_expolygons(slices.surfaces);
            slices.set(union_ex(diff_ex(polys, areas_other), // 用其他网格的梁向内减少层区域
                                areas_here)                  // 用新添加的梁向外扩展层区域
                       , stInternal);
        }
    }
}

} // namespace Slic3r
