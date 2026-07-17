#include <functional>
#include <optional>

#include <libslic3r/OpenVDBUtils.hpp>
#include <libslic3r/TriangleMesh.hpp>
#include <libslic3r/TriangleMeshSlicer.hpp>
#include <libslic3r/SLA/Hollowing.hpp>
#include <libslic3r/SLA/IndexedMesh.hpp>
#include <libslic3r/ClipperUtils.hpp>
#include <libslic3r/QuadricEdgeCollapse.hpp>
#include <libslic3r/SLA/SupportTreeMesher.hpp>

#include <boost/log/trivial.hpp>

#include <libslic3r/MTUtils.hpp>
#include <libslic3r/I18N.hpp>

//! 用于标记被本地化使用的字符串的宏，
//! 返回相同的字符串
#define L(s) Slic3r::I18N::translate(s)

namespace Slic3r {
namespace sla {

struct Interior {
    indexed_triangle_set mesh;
    openvdb::FloatGrid::Ptr gridptr;
    mutable std::optional<openvdb::FloatGrid::ConstAccessor> accessor;

    double closing_distance = 0.;
    double thickness = 0.;
    double voxel_scale = 1.;
    double nb_in = 3.;  // 向内窄带宽度
    double nb_out = 3.; // 向外窄带宽度
    // 全窄带是上述两个值的和。

    void reset_accessor() const  // 重置访问器及其缓存
    // 不是线程安全的调用！
    {
        if (gridptr)
            accessor = gridptr->getConstAccessor();
    }
};

void InteriorDeleter::operator()(Interior *p)
{
    delete p;
}

indexed_triangle_set &get_mesh(Interior &interior)
{
    return interior.mesh;
}

const indexed_triangle_set &get_mesh(const Interior &interior)
{
    return interior.mesh;
}

static InteriorPtr generate_interior_verbose(const TriangleMesh & mesh,
                                             const JobController &ctl,
                                             double min_thickness,
                                             double voxel_scale,
                                             double closing_dist)
{
    double offset = voxel_scale * min_thickness;
    double D = voxel_scale * closing_dist;
    float  out_range = 0.1f * float(offset);
    float  in_range = 1.1f * float(offset + D);

    if (ctl.stopcondition()) return {};
    else ctl.statuscb(0, L("Hollowing"));

    auto gridptr = mesh_to_grid(mesh.its, {}, voxel_scale, out_range, in_range);

    assert(gridptr);

    if (!gridptr) {
        BOOST_LOG_TRIVIAL(error) << "Returned OpenVDB grid is NULL";
        return {};
    }

    if (ctl.stopcondition()) return {};
    else ctl.statuscb(30, L("Hollowing"));

    double iso_surface = D;
    auto   narrowb = double(in_range);
    gridptr = redistance_grid(*gridptr, -(offset + D), narrowb, narrowb);

    if (ctl.stopcondition()) return {};
    else ctl.statuscb(70, L("Hollowing"));

    double adaptivity = 0.;
    InteriorPtr interior = InteriorPtr{new Interior{}};

    interior->mesh = grid_to_mesh(*gridptr, iso_surface, adaptivity);
    interior->gridptr = gridptr;

    if (ctl.stopcondition()) return {};
    else ctl.statuscb(100, L("Hollowing"));

    interior->closing_distance = D;
    interior->thickness = offset;
    interior->voxel_scale = voxel_scale;
    interior->nb_in = narrowb;
    interior->nb_out = narrowb;

    return interior;
}

InteriorPtr generate_interior(const TriangleMesh &   mesh,
                              const HollowingConfig &hc,
                              const JobController &  ctl)
{
    static const double MIN_OVERSAMPL = 3.5;
    static const double MAX_OVERSAMPL = 8.;

    // 我无法通过 openvdb API 增加网格分辨率，
    // 因此模型在转换前会被放大，结果再缩小。
    // 体素具有单位大小。如果我将 voxelSize 设置得更小，
    // 它会将整个几何体缩小，而不会增加体素数量。
    //
    // 最大 8 倍放大，最小为原始体素大小
    auto voxel_scale = MIN_OVERSAMPL + (MAX_OVERSAMPL - MIN_OVERSAMPL) * hc.quality;

    InteriorPtr interior =
        generate_interior_verbose(mesh, ctl, hc.min_thickness, voxel_scale,
                                  hc.closing_distance);

    if (interior && !interior->mesh.empty()) {

        // 将法线翻转回来...
        swap_normals(interior->mesh);

        // 无损简化网格
        float loss_less_max_error = 2*std::numeric_limits<float>::epsilon();
        its_quadric_edge_collapse(interior->mesh, 0U, &loss_less_max_error);

        its_compactify_vertices(interior->mesh);
        its_merge_vertices(interior->mesh);

        // 将法线翻转回来...
        swap_normals(interior->mesh);
    }

    return interior;
}

indexed_triangle_set DrainHole::to_mesh() const
{
    auto r = double(radius);
    auto h = double(height);
    indexed_triangle_set hole = sla::cylinder(r, h, steps);
    Eigen::Quaternionf q;
    q.setFromTwoVectors(Vec3f{0.f, 0.f, 1.f}, normal);
    for(auto& p : hole.vertices) p = q * p + pos;
    
    return hole;
}

bool DrainHole::operator==(const DrainHole &sp) const
{
    return (pos == sp.pos) && (normal == sp.normal) &&
            is_approx(radius, sp.radius) &&
            is_approx(height, sp.height);
}

bool DrainHole::is_inside(const Vec3f& pt) const
{
    Eigen::Hyperplane<float, 3> plane(normal, pos);
    float dist = plane.signedDistance(pt);
    if (dist < float(EPSILON) || dist > height)
        return false;

    Eigen::ParametrizedLine<float, 3> axis(pos, normal);
    if ( axis.squaredDistance(pt) < pow(radius, 2.f))
        return true;

    return false;
}


// 给定直线 s+dir*t，找出与孔洞相交的参数 t 和法线（孔洞内的点）。
// 通过 out 引用输出，如果找到两个交点则返回 true。
bool DrainHole::get_intersections(const Vec3f& s, const Vec3f& dir,
                                  std::array<std::pair<float, Vec3d>, 2>& out)
                                  const
{
    assert(is_approx(normal.norm(), 1.f));
    const Eigen::ParametrizedLine<float, 3> ray(s, dir.normalized());

    for (size_t i=0; i<2; ++i)
        out[i] = std::make_pair(sla::IndexedMesh::hit_result::infty(), Vec3d::Zero());

    const float sqr_radius = pow(radius, 2.f);

    // 首先检查孔洞的包围球：
    Vec3f center = pos+normal*height/2.f;
    float sqr_dist_limit = pow(height/2.f, 2.f) + sqr_radius ;
    if (ray.squaredDistance(center) > sqr_dist_limit)
        return false;

    // 直线与包围球相交，寻找与圆柱体底面的交点。

    size_t found = 0; // 记录找到了多少个交点
    Eigen::Hyperplane<float, 3> base;
    if (! is_approx(ray.direction().dot(normal), 0.f)) {
        for (size_t i=1; i<=1; --i) {
            Vec3f cylinder_center = pos+i*height*normal;
            if (i == 0) {
                // 孔洞底面可能与网格表面重合（如果是平面），
                // 我们最好将底面向外移动一点
                cylinder_center -= EPSILON*normal;
            }
            base = Eigen::Hyperplane<float, 3>(normal, cylinder_center);
            Vec3f intersection = ray.intersectionPoint(base);
            // 只有当点在圆柱体底面内时才接受该点。
            if ((cylinder_center-intersection).squaredNorm() < sqr_radius) {
                out[found].first = ray.intersectionParameter(base);
                out[found].second = (i==0 ? 1. : -1.) * normal.cast<double>();
                ++found;
            }
        }
    }
    else
    {
        // 如果直线垂直于圆柱轴线，则跳过前面的代码块，
        // 但之后假定 base 是有效的。
        base = Eigen::Hyperplane<float, 3>(normal, pos-EPSILON*normal);
    }

    // 如果还有待寻找的交点，检查壁面
    if (found != 2 && ! is_approx(std::abs(ray.direction().dot(normal)), 1.f)) {
        // 将射线投影到基平面上
        Vec3f proj_origin = base.projection(ray.origin());
        Vec3f proj_dir = base.projection(ray.origin()+ray.direction())-proj_origin;
        // 保存参数的缩放方式并归一化投影方向
        float par_scale = proj_dir.norm();
        proj_dir = proj_dir/par_scale;
        Eigen::ParametrizedLine<float, 3> projected_ray(proj_origin, proj_dir);
        // 计算割线上离圆心最近的点
        // 以及其沿投影线到圆的距离
        Vec3f closest = projected_ray.projection(pos);
        float dist = sqrt((sqr_radius - (closest-pos).squaredNorm()));
        // 将两个交点反投影到原始直线上并检查
        // 它们是否在圆柱体上且没有超出：
        for (int i=-1; i<=1 && found !=2; i+=2) {
            Vec3f isect = closest + i*dist * projected_ray.direction();
            Vec3f to_isect = isect-proj_origin;
            float par = to_isect.norm() / par_scale;
            if (to_isect.normalized().dot(proj_dir.normalized()) < 0.f)
                par *= -1.f;
            Vec3d hit_normal = (pos-isect).normalized().cast<double>();
            isect = ray.pointAt(par);
            // 检查交点是否在两个基平面之间：
            float vert_dist = base.signedDistance(isect);
            if (vert_dist > 0.f && vert_dist < height) {
                out[found].first = par;
                out[found].second = hit_normal;
                ++found;
            }
        }
    }

    // 如果只找到一个交点，则属于某种边界情况，
    // 将不返回任何交点：
    if (found != 2)
        return false;

    // 对交点进行排序：
    if (out[0].first > out[1].first)
        std::swap(out[0], out[1]);

    return true;
}

void cut_drainholes(std::vector<ExPolygons> & obj_slices,
                    const std::vector<float> &slicegrid,
                    float                     closing_radius,
                    const sla::DrainHoles &   holes,
                    std::function<void(void)> thr)
{
    TriangleMesh mesh;
    for (const sla::DrainHole &holept : holes)
        mesh.merge(TriangleMesh{holept.to_mesh()});
    
    if (mesh.empty()) return;
    
    std::vector<ExPolygons> hole_slices = slice_mesh_ex(mesh.its, slicegrid, closing_radius, thr);
    
    if (obj_slices.size() != hole_slices.size())
        BOOST_LOG_TRIVIAL(warning)
            << "Sliced object and drain-holes layer count does not match!";

    size_t until = std::min(obj_slices.size(), hole_slices.size());
    
    for (size_t i = 0; i < until; ++i)
        obj_slices[i] = diff_ex(obj_slices[i], hole_slices[i]);
}

void hollow_mesh(TriangleMesh &mesh, const HollowingConfig &cfg, int flags)
{
    InteriorPtr interior = generate_interior(mesh, cfg, JobController{});
    if (!interior) return;

    hollow_mesh(mesh, *interior, flags);
}

void hollow_mesh(TriangleMesh &mesh, const Interior &interior, int flags)
{
    if (mesh.empty() || interior.mesh.empty()) return;

    if (flags & hfRemoveInsideTriangles && interior.gridptr)
        remove_inside_triangles(mesh, interior);

    mesh.merge(TriangleMesh{interior.mesh});
}

// 获取点 p 到内部零等值面的距离。内部的零等值面
// 应位于从模型表面向内偏移 offset + closing_distance 的位置。
static double get_distance_raw(const Vec3f &p, const Interior &interior)
{
    assert(interior.gridptr);

    if (!interior.accessor) interior.reset_accessor();

    auto v       = (p * interior.voxel_scale).cast<double>();
    auto grididx = interior.gridptr->transform().worldToIndexCellCentered(
        {v.x(), v.y(), v.z()});

    return interior.accessor->getValue(grididx) ;
}

struct TriangleBubble { Vec3f center; double R; };

// 返回气泡中心到内部边界的距离，如果三角形太大无法测量则返回 NaN。
static double get_distance(const TriangleBubble &b, const Interior &interior)
{
    double R = b.R * interior.voxel_scale;
    double D = get_distance_raw(b.center, interior);

    return (D > 0. && R >= interior.nb_out) ||
           (D < 0. && R >= interior.nb_in)  ||
           ((D - R) < 0. && 2 * R > interior.thickness) ?
                std::nan("") :
                // FIXME: 添加 interior.voxel_scale 是一种折中方案，
                // 旨在防止构成内部本身的三角形被删除。
                // 这有一个副作用，即小部分劣质三角形仍然可见。
                D - interior.closing_distance /*+ 2 * interior.voxel_scale*/;
}

double get_distance(const Vec3f &p, const Interior &interior)
{
    double d = get_distance_raw(p, interior) - interior.closing_distance;
    return d / interior.voxel_scale;
}

// 一个可被分割的面。如果属于原始网格，则存储其在原始网格中的索引以及其组成的顶点。
enum { NEW_FACE = -1};
struct DivFace {
    Vec3i32 indx;
    std::array<Vec3f, 3> verts;
    long faceid = NEW_FACE;
    long parent = NEW_FACE;
};

// Divide a face recursively and call visitor on all the sub-faces.
template<class Fn>
void divide_triangle(const DivFace &face, Fn &&visitor)
{
    std::array<Vec3f, 3> edges = {(face.verts[0] - face.verts[1]),
                                  (face.verts[1] - face.verts[2]),
                                  (face.verts[2] - face.verts[0])};

    std::array<size_t, 3> edgeidx = {0, 1, 2};

    std::sort(edgeidx.begin(), edgeidx.end(), [&edges](size_t e1, size_t e2) {
        return edges[e1].squaredNorm() > edges[e2].squaredNorm();
    });

    DivFace child1, child2;

    child1.parent   = face.faceid == NEW_FACE ? face.parent : face.faceid;
    child1.indx(0)  = -1;
    child1.indx(1)  = face.indx(edgeidx[1]);
    child1.indx(2)  = face.indx((edgeidx[1] + 1) % 3);
    child1.verts[0] = (face.verts[edgeidx[0]] + face.verts[(edgeidx[0] + 1) % 3]) / 2.;
    child1.verts[1] = face.verts[edgeidx[1]];
    child1.verts[2] = face.verts[(edgeidx[1] + 1) % 3];

    if (visitor(child1))
        divide_triangle(child1, std::forward<Fn>(visitor));

    child2.parent   = face.faceid == NEW_FACE ? face.parent : face.faceid;
    child2.indx(0)  = -1;
    child2.indx(1)  = face.indx(edgeidx[2]);
    child2.indx(2)  = face.indx((edgeidx[2] + 1) % 3);
    child2.verts[0] = child1.verts[0];
    child2.verts[1] = face.verts[edgeidx[2]];
    child2.verts[2] = face.verts[(edgeidx[2] + 1) % 3];

    if (visitor(child2))
        divide_triangle(child2, std::forward<Fn>(visitor));
}

void remove_inside_triangles(TriangleMesh &mesh, const Interior &interior,
                             const std::vector<bool> &exclude_mask)
{
    enum TrPos { posInside, posTouch, posOutside };

    auto &faces       = mesh.its.indices;
    auto &vertices    = mesh.its.vertices;
    auto bb           = mesh.bounding_box();

    bool use_exclude_mask = faces.size() == exclude_mask.size();
    auto is_excluded = [&exclude_mask, use_exclude_mask](size_t face_id) {
        return use_exclude_mask && exclude_mask[face_id];
    };

    // TODO: 并行模式尚不可用
    using exec_policy = ccr_seq;

    // 关于输入网格所需修改的信息。
    struct MeshMods {

        // 一个线程安全的三角形向量包装器。
        struct {
            std::vector<std::array<Vec3f, 3>> data;
            exec_policy::SpinningMutex        mutex;

            void emplace_back(const std::array<Vec3f, 3> &pts)
            {
                std::lock_guard lk{mutex};
                data.emplace_back(pts);
            }

            size_t size() const { return data.size(); }
            const std::array<Vec3f, 3>& operator[](size_t idx) const
            {
                return data[idx];
            }

        } new_triangles;

        // 一个布尔向量，指示所有面是否需要被移除。
        std::vector<bool> to_remove;

        MeshMods(const TriangleMesh &mesh):
            to_remove(mesh.its.indices.size(), false) {}

        // 需要移除的三角形数量。
        size_t to_remove_cnt() const
        {
            return std::accumulate(to_remove.begin(), to_remove.end(), size_t(0));
        }

    } mesh_mods{mesh};

    // 如果需要进一步分割面，则必须返回 true。
    auto divfn = [&interior, bb, &mesh_mods](const DivFace &f) {
        BoundingBoxf3 facebb { f.verts.begin(), f.verts.end() };

        // 该面肯定在空腔外部
        if (! facebb.intersects(bb) && f.faceid != NEW_FACE) {
            return false;
        }

        TriangleBubble bubble{facebb.center().cast<float>(), facebb.radius()};

        double D = get_distance(bubble, interior);
        double R = bubble.R * interior.voxel_scale;

        if (std::isnan(D)) // 无法测量距离，三角形太大
            return true;

        // 气泡壁到内部壁的距离。如果为负值，则表示
        // 气泡与内部重叠
        double bubble_distance = D - R;

        // 该面穿过内部或在内部，必须移除，
        // 并重新添加位于内部之外的部分
        if (bubble_distance < 0.) {
            if (f.faceid != NEW_FACE)
                mesh_mods.to_remove[f.faceid] = true;

            if (f.parent != NEW_FACE) // 顶层父面也需要被移除
                mesh_mods.to_remove[f.parent] = true;

            // 如果外部部分位于内部与外部之间（墙内不可见），则无需进一步分割。
            if ((R + D) < interior.thickness)
                return false;

            return true;
        } else if (f.faceid == NEW_FACE) {
            // 完全在外的新面需要重新添加。
            mesh_mods.new_triangles.emplace_back(f.verts);
        }

        return false;
    };

    interior.reset_accessor();

    exec_policy::for_each(size_t(0), faces.size(), [&] (size_t face_idx) {
        const Vec3i32 &face = faces[face_idx];

        // 如果三角形被排除，我们需要保留它。
        if (is_excluded(face_idx))
            return;

        std::array<Vec3f, 3> pts =
            { vertices[face(0)], vertices[face(1)], vertices[face(2)] };

        BoundingBoxf3 facebb { pts.begin(), pts.end() };

        // 该面肯定在空腔外部
        if (! facebb.intersects(bb)) return;

        DivFace df{face, pts, long(face_idx)};

        if (divfn(df))
            divide_triangle(df, divfn);

    }, exec_policy::max_concurreny());

    auto new_faces = reserve_vector<Vec3i32>(faces.size() +
                                           mesh_mods.new_triangles.size());

    for (size_t face_idx = 0; face_idx < faces.size(); ++face_idx) {
        if (!mesh_mods.to_remove[face_idx])
            new_faces.emplace_back(faces[face_idx]);
    }

    for(size_t i = 0; i < mesh_mods.new_triangles.size(); ++i) {
        size_t o = vertices.size();
        vertices.emplace_back(mesh_mods.new_triangles[i][0]);
        vertices.emplace_back(mesh_mods.new_triangles[i][1]);
        vertices.emplace_back(mesh_mods.new_triangles[i][2]);
        new_faces.emplace_back(int(o), int(o + 1), int(o + 2));
    }

    BOOST_LOG_TRIVIAL(info)
            << "Trimming: " << mesh_mods.to_remove_cnt() << " triangles removed";
    BOOST_LOG_TRIVIAL(info)
            << "Trimming: " << mesh_mods.new_triangles.size() << " triangles added";

    faces.swap(new_faces);
    new_faces = {};

    mesh = TriangleMesh{mesh.its};
    //FIXME 我们是否要修复网格？是否存在重复顶点或翻转三角形？
}

}} // namespace Slic3r::sla
