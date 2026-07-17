#include <unordered_set>

#include <libslic3r/Exception.hpp>
#include <libslic3r/SLAPrintSteps.hpp>
#include <libslic3r/MeshBoolean.hpp>
#include <libslic3r/TriangleMeshSlicer.hpp>

// 在掏空步骤中需要圆柱体方法来处理排水孔
#include <libslic3r/SLA/SupportTreeBuilder.hpp>

#include <libslic3r/SLA/Concurrency.hpp>
#include <libslic3r/SLA/Pad.hpp>
#include <libslic3r/SLA/SupportPointGenerator.hpp>

#include <libslic3r/ElephantFootCompensation.hpp>
#include <libslic3r/AABBTreeIndirect.hpp>

#include <libslic3r/ClipperUtils.hpp>

#include <boost/log/trivial.hpp>

#include "I18N.hpp"

//! 用于标记本地化字符串的宏，
//! 返回相同字符串
#define L(s) Slic3r::I18N::translate(s)

namespace Slic3r {

namespace {

const std::array<unsigned, slaposCount> OBJ_STEP_LEVELS = {
    10, // slaposHollowing,
    10, // slaposDrillHoles
    10, // slaposObjectSlice,
    20, // slaposSupportPoints,
    10, // slaposSupportTree,
    10, // slaposPad,
    30, // slaposSliceSupports,
};

std::string OBJ_STEP_LABELS(size_t idx)
{
    switch (idx) {
    case slaposHollowing:            return L("Hollowing model");
    case slaposDrillHoles:           return L("Drilling holes into model.");
    case slaposObjectSlice:          return L("Slicing model");
    case slaposSupportPoints:        return L("Generating support points");
    case slaposSupportTree:          return L("Generating support tree");
    case slaposPad:                  return L("Generating pad");
    case slaposSliceSupports:        return L("Slicing supports");
    default:;
    }
    assert(false);
    return "Out of bounds!";
}

const std::array<unsigned, slapsCount> PRINT_STEP_LEVELS = {
    10, // slapsMergeSlicesAndEval
    90, // slapsRasterize
};

std::string PRINT_STEP_LABELS(size_t idx)
{
    switch (idx) {
    case slapsMergeSlicesAndEval:   return L("Merging slices and calculating statistics");
    case slapsRasterize:            return L("Rasterizing layers");
    default:;
    }
    assert(false); return "Out of bounds!";
}

}

SLAPrint::Steps::Steps(SLAPrint *print)
    : m_print{print}
    , m_rng{std::random_device{}()}
    , objcount{m_print->m_objects.size()}
    , ilhd{m_print->m_material_config.initial_layer_height.getFloat()}
    , ilh{float(ilhd)}
    , ilhs{scaled(ilhd)}
    , objectstep_scale{(max_objstatus - min_objstatus) / (objcount * 100.0)}
{}

void SLAPrint::Steps::apply_printer_corrections(SLAPrintObject &po, SliceOrigin o)
{
    if (o == soSupport && !po.m_supportdata) return;

    auto faded_lyrs = size_t(po.m_config.faded_layers.getInt());
    double min_w = m_print->m_printer_config.elefant_foot_min_width.getFloat() / 2.;
    double start_efc = m_print->m_printer_config.elefant_foot_compensation.getFloat();

    double doffs = m_print->m_printer_config.absolute_correction.getFloat();
    coord_t clpr_offs = scaled(doffs);

    faded_lyrs = std::min(po.m_slice_index.size(), faded_lyrs);
    size_t faded_lyrs_efc = std::max(size_t(1), faded_lyrs - 1);

    auto efc = [start_efc, faded_lyrs_efc](size_t pos) {
        return (faded_lyrs_efc - pos) * start_efc / faded_lyrs_efc;
    };

    std::vector<ExPolygons> &slices = o == soModel ?
                                          po.m_model_slices :
                                          po.m_supportdata->support_slices;

    if (clpr_offs != 0) for (size_t i = 0; i < po.m_slice_index.size(); ++i) {
        size_t idx = po.m_slice_index[i].get_slice_idx(o);
        if (idx < slices.size())
            slices[idx] = offset_ex(slices[idx], float(clpr_offs));
    }

    if (start_efc > 0.) for (size_t i = 0; i < faded_lyrs; ++i) {
        size_t idx = po.m_slice_index[i].get_slice_idx(o);
        if (idx < slices.size())
            slices[idx] = elephant_foot_compensation(slices[idx], min_w, efc(i));
    }
}

void SLAPrint::Steps::hollow_model(SLAPrintObject &po)
{
    po.m_hollowing_data.reset();

    if (! po.m_config.hollowing_enable.getBool()) {
        BOOST_LOG_TRIVIAL(info) << "Skipping hollowing step!";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "Performing hollowing step!";

    double thickness = po.m_config.hollowing_min_thickness.getFloat();
    double quality  = po.m_config.hollowing_quality.getFloat();
    double closing_d = po.m_config.hollowing_closing_distance.getFloat();
    sla::HollowingConfig hlwcfg{thickness, quality, closing_d};

    sla::InteriorPtr interior = generate_interior(po.transformed_mesh(), hlwcfg);

    if (!interior || sla::get_mesh(*interior).empty())
        BOOST_LOG_TRIVIAL(warning) << "Hollowed interior is empty!";
    else {
        po.m_hollowing_data.reset(new SLAPrintObject::HollowingData());
        po.m_hollowing_data->interior = std::move(interior);
    }
}

struct FaceHash {

    // 64位数的最大十六进制位数
    static constexpr size_t MAX_NUM_CHARS = 16;

    // 为每个三角形创建哈希以便识别。哈希仅使用
    // 三角形的几何特征，而不是特定网格中的索引。
    std::unordered_set<std::string> facehash;

    // 返回反转的字符串，但对于哈希来说没问题
    static std::array<char, MAX_NUM_CHARS + 1> to_chars(int64_t val)
    {
        std::array<char, MAX_NUM_CHARS + 1> ret;

        static const constexpr char * Conv = "0123456789abcdef";

        auto ptr = ret.begin();
        auto uval = static_cast<uint64_t>(std::abs(val));
        while (uval) {
            *ptr = Conv[uval & 0xf];
            ++ptr;
            uval = uval >> 4;
        }
        if (val < 0) { *ptr = '-'; ++ptr; }
        *ptr = '\0'; // C style string ending

        return ret;
    }

    static std::string hash(const Vec<3, int64_t> &v)
    {
        std::string ret;
        ret.reserve(3 * MAX_NUM_CHARS);

        for (auto val : v)
            ret += to_chars(val).data();

        return ret;
    }

    static std::string facekey(const Vec3i32 &face, const std::vector<Vec3f> &vertices)
    {
        // 缩放到整数以避免浮点数
        std::array<Vec<3, int64_t>, 3> pts = {
            scaled<int64_t>(vertices[face(0)]),
            scaled<int64_t>(vertices[face(1)]),
            scaled<int64_t>(vertices[face(2)])
        };

        // 获取三角形的前两条边，计算叉积并将该向量移动到三角形中心。
        // 这编码了识别相同位置相同三角形的所有信息。
        Vec<3, int64_t> a = pts[0] - pts[2], b = pts[1] - pts[2];
        Vec<3, int64_t> c = a.cross(b) + (pts[0] + pts[1] + pts[2]) / 3;

        // 返回坐标的拼接字符串表示
        return hash(c);
    }

    FaceHash (const indexed_triangle_set &its): facehash(its.indices.size())
    {
        for (const Vec3i32 &face : its.indices)
            facehash.insert(facekey(face, its.vertices));
    }

    bool find(const std::string &key)
    {
        auto it = facehash.find(key);
        return it != facehash.end();
    }
};

// 为在掏空内部移除三角形创建排除掩码。
// 当内部已经是使用 CGAL 网格布尔运算钻孔的网格的一部分时，这是必要的。
// 将被排除的是原始属于内部网格的三角形以及构成钻孔壁的三角形。
static std::vector<bool> create_exclude_mask(
        const indexed_triangle_set &its,
        const sla::Interior &interior,
        const std::vector<sla::DrainHole> &holes)
{
    FaceHash interior_hash{sla::get_mesh(interior)};

    std::vector<bool> exclude_mask(its.indices.size(), false);

    VertexFaceIndex neighbor_index{its};

    auto exclude_neighbors = [&neighbor_index, &exclude_mask](const Vec3i32 &face)
    {
        for (int i = 0; i < 3; ++i) {
            const auto &neighbors_range = neighbor_index[face(i)];
            for (size_t fi_n : neighbors_range)
                exclude_mask[fi_n] = true;
        }
    };

    for (size_t fi = 0; fi < its.indices.size(); ++fi) {
        auto &face = its.indices[fi];

        if (interior_hash.find(FaceHash::facekey(face, its.vertices))) {
            exclude_mask[fi] = true;
            continue;
        }

        if (exclude_mask[fi]) {
            exclude_neighbors(face);
            continue;
        }

        // 处理孔洞。孔的所有三角形以及这些三角形的所有邻居都需要保留。
        // 这些邻居是由 CGAL 网格布尔运算创建的，该运算修改了输入网格内部的原始内部以包含孔洞。
        Vec3d tr_center = (
            its.vertices[face(0)] +
            its.vertices[face(1)] +
            its.vertices[face(2)]
        ).cast<double>() / 3.;

        // 如果中心在内部超过半毫米，则不可能成为孔壁的一部分。
        if (sla::get_distance(tr_center, interior) < -0.5)
            continue;

        Vec3f U = its.vertices[face(1)] - its.vertices[face(0)];
        Vec3f V = its.vertices[face(2)] - its.vertices[face(0)];
        Vec3f C = U.cross(V);
        Vec3f face_normal = C.normalized();

        for (const sla::DrainHole &dh : holes) {
            if (dh.failed) continue;

            Vec3d dhpos = dh.pos.cast<double>();
            Vec3d dhend = dhpos + dh.normal.cast<double>() * dh.height;

            Linef3 holeaxis{dhpos, dhend};

            double D_hole_center = line_alg::distance_to(holeaxis, tr_center);
            double D_hole        = std::abs(D_hole_center - dh.radius);
            float dot            = dh.normal.dot(face_normal);

            // 中心距离和法线角度的经验公差。
            // 对于孔壁部分的三角形，三角形法线与孔轴的角度约为90度，
            // 因此点积约为零。
            double D_tol = dh.radius / sla::DrainHole::steps;
            float normal_angle_tol = 1.f / sla::DrainHole::steps;

            if (D_hole < D_tol && std::abs(dot) < normal_angle_tol) {
                exclude_mask[fi] = true;
                exclude_neighbors(face);
            }
        }
    }

    return exclude_mask;
}

static indexed_triangle_set
remove_unconnected_vertices(const indexed_triangle_set &its)
{
    if (its.indices.empty()) {};

    indexed_triangle_set M;

    std::vector<int> vtransl(its.vertices.size(), -1);
    int vcnt = 0;
    for (auto &f : its.indices) {

        for (int i = 0; i < 3; ++i)
            if (vtransl[size_t(f(i))] < 0) {

                M.vertices.emplace_back(its.vertices[size_t(f(i))]);
                vtransl[size_t(f(i))] = vcnt++;
            }

        std::array<int, 3> new_f = {
            vtransl[size_t(f(0))],
            vtransl[size_t(f(1))],
            vtransl[size_t(f(2))]
        };

        M.indices.emplace_back(new_f[0], new_f[1], new_f[2]);
    }

    return M;
}

// Drill holes into the hollowed/original mesh.
void SLAPrint::Steps::drill_holes(SLAPrintObject &po)
{
    /*
    bool needs_drilling = ! po.m_model_object->sla_drain_holes.empty();
    bool is_hollowed =
        (po.m_hollowing_data && po.m_hollowing_data->interior &&
         !sla::get_mesh(*po.m_hollowing_data->interior).empty());

    if (! is_hollowed && ! needs_drilling) {
        // In this case we can dump any data that might have been
        // generated on previous runs.
        po.m_hollowing_data.reset();
        return;
    }

    if (! po.m_hollowing_data)
        po.m_hollowing_data.reset(new SLAPrintObject::HollowingData());

    // Hollowing and/or drilling is active, m_hollowing_data is valid.

    // Regenerate hollowed mesh, even if it was there already. It may contain
    // holes that are no longer on the frontend.
    TriangleMesh &hollowed_mesh = po.m_hollowing_data->hollow_mesh_with_holes;
    hollowed_mesh = po.transformed_mesh();
    if (is_hollowed)
        sla::hollow_mesh(hollowed_mesh, *po.m_hollowing_data->interior);

    TriangleMesh &mesh_view = po.m_hollowing_data->hollow_mesh_with_holes_trimmed;

    if (! needs_drilling) {
        mesh_view = po.transformed_mesh();

        if (is_hollowed)
            sla::hollow_mesh(mesh_view, *po.m_hollowing_data->interior,
                             sla::hfRemoveInsideTriangles);

        BOOST_LOG_TRIVIAL(info) << "Drilling skipped (no holes).";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "Drilling drainage holes.";
    sla::DrainHoles drainholes = po.transformed_drainhole_points();

    auto tree = AABBTreeIndirect::build_aabb_tree_over_indexed_triangle_set(
        hollowed_mesh.its.vertices,
        hollowed_mesh.its.indices
    );

    std::uniform_real_distribution<float> dist(0., float(EPSILON));
    auto holes_mesh_cgal = MeshBoolean::cgal::triangle_mesh_to_cgal({}, {});
    indexed_triangle_set part_to_drill = hollowed_mesh.its;

    bool hole_fail = false;
    for (size_t i = 0; i < drainholes.size(); ++i) {
        sla::DrainHole holept = drainholes[i];

        holept.normal += Vec3f{dist(m_rng), dist(m_rng), dist(m_rng)};
        holept.normal.normalize();
        holept.pos += Vec3f{dist(m_rng), dist(m_rng), dist(m_rng)};
        indexed_triangle_set m = holept.to_mesh();

        part_to_drill.indices.clear();
        auto bb = bounding_box(m);
        Eigen::AlignedBox<float, 3> ebb{bb.min.cast<float>(),
                                        bb.max.cast<float>()};
        //BBS
        //AABBTreeIndirect::traverse(
        //            tree,
        //            AABBTreeIndirect::intersecting(ebb),
        //            [&part_to_drill, &hollowed_mesh](size_t faceid)
        //{
        //    part_to_drill.indices.emplace_back(hollowed_mesh.its.indices[faceid]);
        //});

        auto cgal_meshpart = MeshBoolean::cgal::triangle_mesh_to_cgal(
            remove_unconnected_vertices(part_to_drill));

        if (MeshBoolean::cgal::does_self_intersect(*cgal_meshpart)) {
            BOOST_LOG_TRIVIAL(error) << "Failed to drill hole";

            hole_fail = drainholes[i].failed =
                    po.model_object()->sla_drain_holes[i].failed = true;

            continue;
        }

        auto cgal_hole = MeshBoolean::cgal::triangle_mesh_to_cgal(m);
        MeshBoolean::cgal::plus(*holes_mesh_cgal, *cgal_hole);
    }

    if (MeshBoolean::cgal::does_self_intersect(*holes_mesh_cgal))
        throw Slic3r::SlicingError(L("Too many overlapping holes."));

    auto hollowed_mesh_cgal = MeshBoolean::cgal::triangle_mesh_to_cgal(hollowed_mesh);

    if (!MeshBoolean::cgal::does_bound_a_volume(*hollowed_mesh_cgal)) {
        po.active_step_add_warning(
            PrintStateBase::WarningLevel::NON_CRITICAL,
            L("Mesh to be hollowed is not suitable for hollowing (does not "
              "bound a volume)."));
    }

    if (!MeshBoolean::cgal::empty(*holes_mesh_cgal)
        && !MeshBoolean::cgal::does_bound_a_volume(*holes_mesh_cgal)) {
        po.active_step_add_warning(
            PrintStateBase::WarningLevel::NON_CRITICAL,
            L("Unable to drill the current configuration of holes into the "
              "model."));
    }

    try {
        if (!MeshBoolean::cgal::empty(*holes_mesh_cgal))
            MeshBoolean::cgal::minus(*hollowed_mesh_cgal, *holes_mesh_cgal);

        hollowed_mesh = MeshBoolean::cgal::cgal_to_triangle_mesh(*hollowed_mesh_cgal);
        mesh_view = hollowed_mesh;

        if (is_hollowed) {
            auto &interior = *po.m_hollowing_data->interior;
            std::vector<bool> exclude_mask =
                    create_exclude_mask(mesh_view.its, interior, drainholes);

            sla::remove_inside_triangles(mesh_view, interior, exclude_mask);
        }
    } catch (const Slic3r::RuntimeError &) {
        throw Slic3r::SlicingError(L(
            "Drilling holes into the mesh failed. "
            "This is usually caused by broken model. Try to fix it first."));
    }

    if (hole_fail)
        po.active_step_add_warning(PrintStateBase::WarningLevel::NON_CRITICAL,
                                   L("Failed to drill some holes into the model"));
     */
}

// 切片将在从支撑模型周围的边界框底部开始的假想1D网格上执行。
// 因此通常较厚的第一层将是支撑的一部分，而不是模型几何体。
// 例外情况是模型不在空中（抬高为零）且未请求创建底座。
// 在这种情况下，模型几何体从地面层开始，初始层是其一部分。
// 无论如何，模型和支撑必须在相同的假想网格中切片（TriangleMeshSlicer 的高度向量参数）。
void SLAPrint::Steps::slice_model(SLAPrintObject &po)
{
    const TriangleMesh &mesh = po.get_mesh_to_slice();

    // We need to prepare the slice index...

    double  lhd  = m_print->m_objects.front()->m_config.layer_height.getFloat();
    float   lh   = float(lhd);
    coord_t lhs  = scaled(lhd);
    auto && bb3d = mesh.bounding_box();
    double  minZ = bb3d.min(Z) - po.get_elevation();
    double  maxZ = bb3d.max(Z);
    auto    minZf = float(minZ);
    coord_t minZs = scaled(minZ);
    coord_t maxZs = scaled(maxZ);

    po.m_slice_index.clear();

    size_t cap = size_t(1 + (maxZs - minZs - ilhs) / lhs);
    po.m_slice_index.reserve(cap);

    po.m_slice_index.emplace_back(minZs + ilhs, minZf + ilh / 2.f, ilh);

    for(coord_t h = minZs + ilhs + lhs; h <= maxZs; h += lhs)
        po.m_slice_index.emplace_back(h, unscaled<float>(h) - lh / 2.f, lh);

    // 获取来自模型的第一个记录：
    auto slindex_it =
        po.closest_slice_record(po.m_slice_index, float(bb3d.min(Z)));

    if(slindex_it == po.m_slice_index.end())
        //TRN To be shown at the status bar on SLA slicing error.
        throw Slic3r::RuntimeError(
            L("Slicing had to be stopped due to an internal error: "
              "Inconsistent slice index."));

    po.m_model_height_levels.clear();
    po.m_model_height_levels.reserve(po.m_slice_index.size());
    for(auto it = slindex_it; it != po.m_slice_index.end(); ++it)
        po.m_model_height_levels.emplace_back(it->slice_level());

    po.m_model_slices.clear();
    MeshSlicingParamsEx params;
    params.closing_radius = float(po.config().slice_closing_radius.value);
    //BBS: always regular mode
    //switch (po.config().slicing_mode.value) {
    //case SlicingMode::Regular:    params.mode = MeshSlicingParams::SlicingMode::Regular; break;
    //case SlicingMode::EvenOdd:    params.mode = MeshSlicingParams::SlicingMode::EvenOdd; break;
    //case SlicingMode::CloseHoles: params.mode = MeshSlicingParams::SlicingMode::Positive; break;
    //}
    params.mode = MeshSlicingParams::SlicingMode::Regular;
    auto  thr        = [this]() { m_print->throw_if_canceled(); };
    auto &slice_grid = po.m_model_height_levels;
    po.m_model_slices = slice_mesh_ex(mesh.its, slice_grid, params, thr);

    sla::Interior *interior = po.m_hollowing_data ?
                                  po.m_hollowing_data->interior.get() :
                                  nullptr;

    if (interior && ! sla::get_mesh(*interior).empty()) {
        indexed_triangle_set interiormesh = sla::get_mesh(*interior);
        sla::swap_normals(interiormesh);
        params.mode = MeshSlicingParams::SlicingMode::Regular;

        std::vector<ExPolygons> interior_slices = slice_mesh_ex(interiormesh, slice_grid, params, thr);

        sla::ccr::for_each(size_t(0), interior_slices.size(),
                           [&po, &interior_slices] (size_t i) {
                              const ExPolygons &slice = interior_slices[i];
                              po.m_model_slices[i] =
                                  diff_ex(po.m_model_slices[i], slice);
                           });
    }

    auto mit = slindex_it;
    for (size_t id = 0;
         id < po.m_model_slices.size() && mit != po.m_slice_index.end();
         id++) {
        mit->set_model_slice_idx(po, id); ++mit;
    }

    // We apply the printer correction offset here.
    apply_printer_corrections(po, soModel);

    if(po.m_config.supports_enable.getBool() || po.m_config.pad_enable.getBool())
    {
        po.m_supportdata.reset(new SLAPrintObject::SupportData(po.get_mesh_to_print()));
    }
}

// 在此步骤中，我们检查切片，识别孤岛并用支撑点覆盖它们。然后我们在网格的其余部分撒点。
void SLAPrint::Steps::support_points(SLAPrintObject &po)
{
    // 如果支撑禁用，我们可以跳过模型扫描。
    if(!po.m_config.supports_enable.getBool()) return;

    if (!po.m_supportdata)
        po.m_supportdata.reset(new SLAPrintObject::SupportData(po.get_mesh_to_print()));

    const ModelObject& mo = *po.m_model_object;

    BOOST_LOG_TRIVIAL(debug) << "Support point count "
                             << mo.sla_support_points.size();

    // 除非用户修改了点或我们已经完成了计算，
    // 否则我们将进行自动放置。否则我们将直接把前端数据复制到后端缓存。
    if (mo.sla_points_status != sla::PointsStatus::UserModified) {

        // 计算切片的高度（切片已经计算完成）
        const std::vector<float>& heights = po.m_model_height_levels;

        // 告诉网格排水孔的位置。虽然点在切片上计算，
        // 但算法随后对点进行射线投射，使它们实际位于网格上。
//        po.m_supportdata->emesh.load_holes(po.transformed_drainhole_points());

        throw_if_canceled();
        sla::SupportPointGenerator::Config config;
        const SLAPrintObjectConfig& cfg = po.config();

        // the density config value is in percents:
        config.density_relative = float(cfg.support_points_density_relative / 100.f);
        config.minimal_distance = float(cfg.support_points_minimal_distance);
        config.head_diameter    = float(cfg.support_head_front_diameter);

        // 子操作的缩放
        double d = objectstep_scale * OBJ_STEP_LEVELS[slaposSupportPoints] / 100.0;
        double init = current_status();

        auto statuscb = [this, d, init](unsigned st)
        {
            double current = init + st * d;
            if(std::round(current_status()) < std::round(current))
                report_status(current, OBJ_STEP_LABELS(slaposSupportPoints));
        };

        // 此对象的构造执行计算。
        throw_if_canceled();
        sla::SupportPointGenerator auto_supports(
            po.m_supportdata->emesh, po.get_model_slices(), heights, config,
            [this]() { throw_if_canceled(); }, statuscb);

        // 现在提取结果。
        const std::vector<sla::SupportPoint>& points = auto_supports.output();
        throw_if_canceled();
        po.m_supportdata->pts = points;

        BOOST_LOG_TRIVIAL(debug) << "Automatic support points: "
                                 << po.m_supportdata->pts.size();

        // 使用 RELOAD_SLA_SUPPORT_POINTS 告诉 Plater
        // 将更新状态传递给 GLGizmoSlaSupports
        report_status(-1, L("Generating support points"),
                      SlicingStatus::RELOAD_SLA_SUPPORT_POINTS);
    } else {
        // 前端有一些点，或者用户故意删除了它们。不会进行计算。
        po.m_supportdata->pts = po.transformed_support_points();
    }
}

void SLAPrint::Steps::support_tree(SLAPrintObject &po)
{
    if(!po.m_supportdata) return;

    sla::PadConfig pcfg = make_pad_cfg(po.m_config);

    if (pcfg.embed_object)
        po.m_supportdata->emesh.ground_level_offset(pcfg.wall_thickness_mm);

    // 如果启用了零抬高模式，我们必须过滤掉对象底部的所有点
    if (is_zero_elevation(po.config())) {
        remove_bottom_points(po.m_supportdata->pts,
                             float(po.m_supportdata->emesh.ground_level() + EPSILON));
    }

    po.m_supportdata->cfg = make_support_cfg(po.m_config);
//    po.m_supportdata->emesh.load_holes(po.transformed_drainhole_points());

    // scaling for the sub operations
    double d = objectstep_scale * OBJ_STEP_LEVELS[slaposSupportTree] / 100.0;
    double init = current_status();
    sla::JobController ctl;

    ctl.statuscb = [this, d, init](unsigned st, const std::string &logmsg) {
        double current = init + st * d;
        if (std::round(current_status()) < std::round(current))
            report_status(current, OBJ_STEP_LABELS(slaposSupportTree),
                          SlicingStatus::DEFAULT, logmsg);
    };
    ctl.stopcondition = [this]() { return canceled(); };
    ctl.cancelfn = [this]() { throw_if_canceled(); };

    po.m_supportdata->create_support_tree(ctl);

    if (!po.m_config.supports_enable.getBool()) return;

    throw_if_canceled();

    // 创建统一网格
    auto rc = SlicingStatus::RELOAD_SCENE;

    // 这是为了防止在 merged_mesh() 期间显示"完成"
    report_status(-1, L("Visualizing supports"));

    BOOST_LOG_TRIVIAL(debug) << "Processed support point count "
                             << po.m_supportdata->pts.size();

    // 检查网格以便后续故障排除。
    if(po.support_mesh().empty())
        BOOST_LOG_TRIVIAL(warning) << "Support mesh is empty";

    report_status(-1, L("Visualizing supports"), rc);
}

void SLAPrint::Steps::generate_pad(SLAPrintObject &po) {
    // 此步骤只能在支撑树创建之后、支撑切片之前进行。（或者切片必须重复）

    if(po.m_config.pad_enable.getBool()) {
        // Get the distilled pad configuration from the config
        sla::PadConfig pcfg = make_pad_cfg(po.m_config);

        ExPolygons bp; // This will store the base plate of the pad.
        double   pad_h             = pcfg.full_height();
        const TriangleMesh &trmesh = po.transformed_mesh();

        if (!po.m_config.supports_enable.getBool() || pcfg.embed_object) {
            // No support (thus no elevation) or zero elevation mode
            // we sometimes call it "builtin pad" is enabled so we will
            // get a sample from the bottom of the mesh and use it for pad
            // creation.
            sla::pad_blueprint(trmesh.its, bp, float(pad_h),
                               float(po.m_config.layer_height.getFloat()),
                               [this](){ throw_if_canceled(); });
        }

        po.m_supportdata->create_pad(bp, pcfg);

        if (!validate_pad(po.m_supportdata->support_tree_ptr->retrieve_mesh(sla::MeshType::Pad), pcfg))
            throw Slic3r::SlicingError(
                    L("No pad can be generated for this model with the "
                      "current configuration"));

    } else if(po.m_supportdata && po.m_supportdata->support_tree_ptr) {
        po.m_supportdata->support_tree_ptr->remove_pad();
    }

    throw_if_canceled();
    report_status(-1, L("Visualizing supports"), SlicingStatus::RELOAD_SCENE);
}

// 类似于模型切片过程对支撑几何体进行切片。
// 如果先前的底座步骤中已添加了底座，它将作为切片的一部分
void SLAPrint::Steps::slice_supports(SLAPrintObject &po) {
    auto& sd = po.m_supportdata;

    if(sd) sd->support_slices.clear();

    // 如果没有支撑也没有底座，不要费心。
    if (!po.m_config.supports_enable.getBool() && !po.m_config.pad_enable.getBool())
        return;

    if(sd && sd->support_tree_ptr) {
        auto heights = reserve_vector<float>(po.m_slice_index.size());

        for(auto& rec : po.m_slice_index) heights.emplace_back(rec.slice_level());

        sd->support_slices = sd->support_tree_ptr->slice(
            heights, float(po.config().slice_closing_radius.value));
    }

    for (size_t i = 0; i < sd->support_slices.size() && i < po.m_slice_index.size(); ++i)
        po.m_slice_index[i].set_support_slice_idx(po, i);

    apply_printer_corrections(po, soSupport);

    // 使用 RELOAD_SLA_PREVIEW 告诉 Plater 将更新状态传递给 3D 预览以加载 SLA 切片。
    report_status(-2, "", SlicingStatus::RELOAD_SLA_PREVIEW);
}

// get polygons for all instances in the object
static ExPolygons get_all_polygons(const SliceRecord& record, SliceOrigin o)
{
    if (!record.print_obj()) return {};

    ExPolygons polygons;
    auto &input_polygons = record.get_slice(o);
    auto &instances = record.print_obj()->instances();
    bool is_lefthanded = record.print_obj()->is_left_handed();
    polygons.reserve(input_polygons.size() * instances.size());

    for (const ExPolygon& polygon : input_polygons) {
        if(polygon.contour.empty()) continue;

        for (size_t i = 0; i < instances.size(); ++i)
        {
            ExPolygon poly;

            // We need to reverse if is_lefthanded is true but
            bool needreverse = is_lefthanded;

            // should be a move
            poly.contour.points.reserve(polygon.contour.size() + 1);

            auto& cntr = polygon.contour.points;
            if(needreverse)
                for(auto it = cntr.rbegin(); it != cntr.rend(); ++it)
                    poly.contour.points.emplace_back(it->x(), it->y());
            else
                for(auto& p : cntr)
                    poly.contour.points.emplace_back(p.x(), p.y());

            for(auto& h : polygon.holes) {
                poly.holes.emplace_back();
                auto& hole = poly.holes.back();
                hole.points.reserve(h.points.size() + 1);

                if(needreverse)
                    for(auto it = h.points.rbegin(); it != h.points.rend(); ++it)
                        hole.points.emplace_back(it->x(), it->y());
                else
                    for(auto& p : h.points)
                        hole.points.emplace_back(p.x(), p.y());
            }

            if(is_lefthanded) {
                for(auto& p : poly.contour) p.x() = -p.x();
                for(auto& h : poly.holes) for(auto& p : h) p.x() = -p.x();
            }

            poly.rotate(double(instances[i].rotation));
            poly.translate(Point{instances[i].shift.x(), instances[i].shift.y()});

            polygons.emplace_back(std::move(poly));
        }
    }

    return polygons;
}

void SLAPrint::Steps::initialize_printer_input()
{
    auto &printer_input = m_print->m_printer_input;

    // 清除光栅化输入
    printer_input.clear();

    size_t mx = 0;
    for(SLAPrintObject * o : m_print->m_objects) {
        if(auto m = o->get_slice_index().size() > mx) mx = m;
    }

    printer_input.reserve(mx);

    auto eps = coord_t(SCALED_EPSILON);

    for(SLAPrintObject * o : m_print->m_objects) {
        coord_t gndlvl = o->get_slice_index().front().print_level() - ilhs;

        for(const SliceRecord& slicerecord : o->get_slice_index()) {
            if (!slicerecord.is_valid())
                throw Slic3r::SlicingError(
                    L("There are unprintable objects. Try to "
                      "adjust support settings to make the "
                      "objects printable."));

            coord_t lvlid = slicerecord.print_level() - gndlvl;

            // 将层级别四舍五入到网格的巧妙技巧。
            lvlid = eps * (lvlid / eps);

            auto it = std::lower_bound(printer_input.begin(),
                                       printer_input.end(),
                                       PrintLayer(lvlid));

            if(it == printer_input.end() || it->level() != lvlid)
                it = printer_input.insert(it, PrintLayer(lvlid));


            it->add(slicerecord);
        }
    }
}

// 将所有打印对象的切片合并到一个切片网格中，
// 并从合并结果计算打印统计信息。
void SLAPrint::Steps::merge_slices_and_eval_stats() {

    initialize_printer_input();

    auto &print_statistics = m_print->m_print_statistics;
    auto &printer_config   = m_print->m_printer_config;
    auto &material_config  = m_print->m_material_config;
    auto &printer_input    = m_print->m_printer_input;

    print_statistics.clear();

    const double area_fill = printer_config.area_fill.getFloat()*0.01;// 0.5 (50%);
    const double fast_tilt = printer_config.fast_tilt_time.getFloat();// 5.0;
    const double slow_tilt = printer_config.slow_tilt_time.getFloat();// 8.0;

    const double init_exp_time = material_config.initial_exposure_time.getFloat();
    const double exp_time      = material_config.exposure_time.getFloat();

    const int fade_layers_cnt = m_print->m_default_object_config.faded_layers.getInt();// 10 // [3;20]

    const auto width          = scaled<double>(printer_config.display_width.getFloat());
    const auto height         = scaled<double>(printer_config.display_height.getFloat());
    const double display_area = width*height;

    double supports_volume(0.0);
    double models_volume(0.0);

    double estim_time(0.0);
    std::vector<double> layers_times;
    layers_times.reserve(printer_input.size());

    size_t slow_layers = 0;
    size_t fast_layers = 0;

    const double delta_fade_time = (init_exp_time - exp_time) / (fade_layers_cnt + 1);
    double fade_layer_time = init_exp_time;

    sla::ccr::SpinningMutex mutex;
    using Lock = std::lock_guard<sla::ccr::SpinningMutex>;

    // Going to parallel:
    auto printlayerfn = [this,
            // functions and read only vars
            area_fill, display_area, exp_time, init_exp_time, fast_tilt, slow_tilt, delta_fade_time,

            // write vars
            &mutex, &models_volume, &supports_volume, &estim_time, &slow_layers,
            &fast_layers, &fade_layer_time, &layers_times](size_t sliced_layer_cnt)
    {
        PrintLayer &layer = m_print->m_printer_input[sliced_layer_cnt];

        // vector of slice record references
        auto& slicerecord_references = layer.slices();

        if(slicerecord_references.empty()) return;

        // Layer height should match for all object slices for a given level.
        const auto l_height = double(slicerecord_references.front().get().layer_height());

        // Calculation of the consumed material

        ExPolygons model_polygons;
        ExPolygons supports_polygons;

        size_t c = std::accumulate(layer.slices().begin(),
                                   layer.slices().end(),
                                   size_t(0),
                                   [](size_t a, const SliceRecord &sr) {
            return a + sr.get_slice(soModel).size();
        });

        model_polygons.reserve(c);

        c = std::accumulate(layer.slices().begin(),
                            layer.slices().end(),
                            size_t(0),
                            [](size_t a, const SliceRecord &sr) {
            return a + sr.get_slice(soModel).size();
        });

        supports_polygons.reserve(c);

        for(const SliceRecord& record : layer.slices()) {

            ExPolygons modelslices = get_all_polygons(record, soModel);
            for(ExPolygon& p_tmp : modelslices) model_polygons.emplace_back(std::move(p_tmp));

            ExPolygons supportslices = get_all_polygons(record, soSupport);
            for(ExPolygon& p_tmp : supportslices) supports_polygons.emplace_back(std::move(p_tmp));

        }

        model_polygons = union_ex(model_polygons);
        double layer_model_area = 0;
        for (const ExPolygon& polygon : model_polygons)
            layer_model_area += area(polygon);

        if (layer_model_area < 0 || layer_model_area > 0) {
            Lock lck(mutex); models_volume += layer_model_area * l_height;
        }

        if(!supports_polygons.empty()) {
            if(model_polygons.empty()) supports_polygons = union_ex(supports_polygons);
            else supports_polygons = diff_ex(supports_polygons, model_polygons);
            // allegedly, union of subject is done withing the diff according to the pftPositive polyFillType
        }

        double layer_support_area = 0;
        for (const ExPolygon& polygon : supports_polygons)
            layer_support_area += area(polygon);

        if (layer_support_area < 0 || layer_support_area > 0) {
            Lock lck(mutex); supports_volume += layer_support_area * l_height;
        }

        // Here we can save the expensively calculated polygons for printing
        ExPolygons trslices;
        trslices.reserve(model_polygons.size() + supports_polygons.size());
        for(ExPolygon& poly : model_polygons) trslices.emplace_back(std::move(poly));
        for(ExPolygon& poly : supports_polygons) trslices.emplace_back(std::move(poly));

        layer.transformed_slices(union_ex(trslices));

        // Calculation of the slow and fast layers to the future controlling those values on FW

        const bool is_fast_layer = (layer_model_area + layer_support_area) <= display_area*area_fill;
        const double tilt_time = is_fast_layer ? fast_tilt : slow_tilt;

        { Lock lck(mutex);
            if (is_fast_layer)
                fast_layers++;
            else
                slow_layers++;

            // Calculation of the printing time

            double layer_times = 0.0;
            if (sliced_layer_cnt < 3)
                layer_times += init_exp_time;
            else if (fade_layer_time > exp_time) {
                fade_layer_time -= delta_fade_time;
                layer_times += fade_layer_time;
            }
            else
                layer_times += exp_time;
            layer_times += tilt_time;

            layers_times.push_back(layer_times);
            estim_time += layer_times;
        }
    };

    // 用于调试的顺序版本：
    // for(size_t i = 0; i < m_printer_input.size(); ++i) printlayerfn(i);
    sla::ccr::for_each(size_t(0), printer_input.size(), printlayerfn);

    auto SCALING2 = SCALING_FACTOR * SCALING_FACTOR;
    print_statistics.support_used_material = supports_volume * SCALING2;
    print_statistics.objects_used_material = models_volume  * SCALING2;

    // Estimated printing time
    // A layers count o the highest object
    if (printer_input.size() == 0)
        print_statistics.estimated_print_time = std::nan("");
    else {
        print_statistics.estimated_print_time = estim_time;
        print_statistics.layers_times = layers_times;
    }

    print_statistics.fast_layers_count = fast_layers;
    print_statistics.slow_layers_count = slow_layers;

    report_status(-2, "", SlicingStatus::RELOAD_SLA_PREVIEW);
}

// Rasterizing the model objects, and their supports
void SLAPrint::Steps::rasterize()
{
    if(canceled() || !m_print->m_printer) return;

    // 将光栅化状态（0-99）映射到进程状态的分配部分（插槽）的系数
    double sd = (100 - max_objstatus) / 100.0;

    // 插槽是与光栅化相关的100%的部分
    unsigned slot = PRINT_STEP_LEVELS[slapsRasterize];

    // pst: 先前状态
    double pst = current_status();

    double increment = (slot * sd) / m_print->m_printer_input.size();
    double dstatus = current_status();

    sla::ccr::SpinningMutex slck;
    using Lock = std::lock_guard<sla::ccr::SpinningMutex>;

    // procedure to process one height level. This will run in parallel
    auto lvlfn =
        [this, &slck, increment, &dstatus, &pst]
        (sla::RasterBase& raster, size_t idx)
    {
        PrintLayer& printlayer = m_print->m_printer_input[idx];
        if(canceled()) return;

        for (const ExPolygon& poly : printlayer.transformed_slices())
            raster.draw(poly);

        // Status indication guarded with the spinlock
        {
            Lock lck(slck);
            dstatus += increment;
            double st = std::round(dstatus);
            if(st > pst) {
                report_status(st, PRINT_STEP_LABELS(slapsRasterize));
                pst = st;
            }
        }
    };

    // last minute escape
    if(canceled()) return;

    // Print all the layers in parallel
    m_print->m_printer->draw_layers(m_print->m_printer_input.size(), lvlfn,
                                    [this]() { return canceled(); }, ex_tbb);
}

std::string SLAPrint::Steps::label(SLAPrintObjectStep step)
{
    return OBJ_STEP_LABELS(step);
}

std::string SLAPrint::Steps::label(SLAPrintStep step)
{
    return PRINT_STEP_LABELS(step);
}

double SLAPrint::Steps::progressrange(SLAPrintObjectStep step) const
{
    return OBJ_STEP_LEVELS[step] * objectstep_scale;
}

double SLAPrint::Steps::progressrange(SLAPrintStep step) const
{
    return PRINT_STEP_LEVELS[step] * (100 - max_objstatus) / 100.0;
}

void SLAPrint::Steps::execute(SLAPrintObjectStep step, SLAPrintObject &obj)
{
    switch(step) {
    case slaposHollowing: hollow_model(obj); break;
    case slaposDrillHoles: drill_holes(obj); break;
    case slaposObjectSlice: slice_model(obj); break;
    case slaposSupportPoints:  support_points(obj); break;
    case slaposSupportTree: support_tree(obj); break;
    case slaposPad: generate_pad(obj); break;
    case slaposSliceSupports: slice_supports(obj); break;
    case slaposCount: assert(false);
    }
}

void SLAPrint::Steps::execute(SLAPrintStep step)
{
    switch (step) {
    case slapsMergeSlicesAndEval: merge_slices_and_eval_stats(); break;
    case slapsRasterize: rasterize(); break;
    case slapsCount: assert(false);
    }
}

}
