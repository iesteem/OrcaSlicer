#include "SupportTreeMesher.hpp"

namespace Slic3r { namespace sla {

indexed_triangle_set sphere(double rho, Portion portion, double fa) {

    indexed_triangle_set ret;

    // 禁止接近零的半径
    if(rho <= 1e-6 && rho >= -1e-6) return ret;

    auto& vertices = ret.vertices;
    auto& facets = ret.indices;

    // 算法：
    // 逐个向球体网格添加点，并使用相对坐标形成三角面。
    // 球体实际上由堆叠圆的网格组成。

    // 通过四舍五入调整，以获得任何提供角度的偶数倍。
    double angle = (2 * PI / floor(2*PI / fa) );

    // 要缩放的环，用于生成球体的步进
    std::vector<double> ring;

    for (double i = 0; i < 2*PI; i+=angle) ring.emplace_back(i);

    const auto sbegin = size_t(2*std::get<0>(portion)/angle);
    const auto send = size_t(2*std::get<1>(portion)/angle);

    const size_t steps = ring.size();
    const double increment = 1.0 / double(steps);

    // 特殊情况：第一个环连接到 0,0,0
    // 插入并形成三角面。
    if (sbegin == 0)
        vertices.emplace_back(
            Vec3f(0.f, 0.f, float(-rho + increment * sbegin * 2. * rho)));

    auto id = coord_t(vertices.size());
    for (size_t i = 0; i < ring.size(); i++) {
        // 固定缩放
        const double z = -rho + increment*rho*2.0 * (sbegin + 1.0);
        // 此步骤的圆的半径。
        const double r = std::sqrt(std::abs(rho*rho - z*z));
        Vec2d b = Eigen::Rotation2Dd(ring[i]) * Eigen::Vector2d(0, r);
        vertices.emplace_back(Vec3d(b(0), b(1), z).cast<float>());

        if (sbegin == 0)
            (i == 0) ? facets.emplace_back(coord_t(ring.size()), 0, 1) :
                       facets.emplace_back(id - 1, 0, id);
        ++id;
    }

    // 一般情况：为每一步插入并形成三角面，
    // 将其连接到下方的环。
    for (size_t s = sbegin + 2; s < send - 1; s++) {
        const double z = -rho + increment * double(s * 2. * rho);
        const double r = std::sqrt(std::abs(rho*rho - z*z));

        for (size_t i = 0; i < ring.size(); i++) {
            Vec2d b = Eigen::Rotation2Dd(ring[i]) * Eigen::Vector2d(0, r);
            vertices.emplace_back(Vec3d(b(0), b(1), z).cast<float>());
            auto id_ringsize = coord_t(id - int(ring.size()));
            if (i == 0) {
                // 环绕
                facets.emplace_back(id - 1, id, id + coord_t(ring.size() - 1) );
                facets.emplace_back(id - 1, id_ringsize, id);
            } else {
                facets.emplace_back(id_ringsize - 1, id_ringsize, id);
                facets.emplace_back(id - 1, id_ringsize - 1, id);
            }
            id++;
        }
    }

    // 特殊情况：最后一个环连接到 0,0,rho*2.0
    // 仅形成三角面。
    if(send >= size_t(2*PI / angle)) {
        vertices.emplace_back(0.f, 0.f, float(-rho + increment*send*2.0*rho));
        for (size_t i = 0; i < ring.size(); i++) {
            auto id_ringsize = coord_t(id - int(ring.size()));
            if (i == 0) {
                // 第三个顶点在环的另一侧。
                facets.emplace_back(id - 1, id_ringsize, id);
            } else {
                auto ci = coord_t(id_ringsize + coord_t(i));
                facets.emplace_back(ci - 1, ci, id);
            }
        }
    }
    id++;

    return ret;
}

indexed_triangle_set cylinder(double r, double h, size_t ssteps, const Vec3d &sp)
{
    assert(ssteps > 0);

    indexed_triangle_set ret;

    auto steps = int(ssteps);
    auto& points = ret.vertices;
    auto& indices = ret.indices;
    points.reserve(2*ssteps);
    double a = 2*PI/steps;

    Vec3d jp = sp;
    Vec3d endp = {sp(X), sp(Y), sp(Z) + h};

    // 上圆点
    for(int i = 0; i < steps; ++i) {
        double phi = i*a;
        auto ex = float(endp(X) + r*std::cos(phi));
        auto ey = float(endp(Y) + r*std::sin(phi));
        points.emplace_back(ex, ey, float(endp(Z)));
    }

    // 下圆点
    for(int i = 0; i < steps; ++i) {
        double phi = i*a;
        auto x = float(jp(X) + r*std::cos(phi));
        auto y = float(jp(Y) + r*std::sin(phi));
        points.emplace_back(x, y, float(jp(Z)));
    }

    // 现在创建连接上下圆的长三角形
    indices.reserve(2*ssteps);
    auto offs = steps;
    for(int i = 0; i < steps - 1; ++i) {
        indices.emplace_back(i, i + offs, offs + i + 1);
        indices.emplace_back(i, offs + i + 1, i + 1);
    }

    // 连接第一个和最后一个顶点的最后一个三角形
    auto last = steps - 1;
    indices.emplace_back(0, last, offs);
    indices.emplace_back(last, offs + last, offs);

    // 根据切片算法，我们需要帮助它们生成水密体。
    // 所以我们为圆柱的上下端创建三角形扇以封闭几何体。
    points.emplace_back(jp.cast<float>()); int ci = int(points.size() - 1);
    for(int i = 0; i < steps - 1; ++i)
        indices.emplace_back(i + offs + 1, i + offs, ci);

    indices.emplace_back(offs, steps + offs - 1, ci);

    points.emplace_back(endp.cast<float>()); ci = int(points.size() - 1);
    for(int i = 0; i < steps - 1; ++i)
        indices.emplace_back(ci, i, i + 1);

    indices.emplace_back(steps - 1, 0, ci);

    return ret;
}

indexed_triangle_set pinhead(double r_pin,
                             double r_back,
                             double length,
                             size_t steps)
{
    assert(steps > 0);
    assert(length >= 0.);
    assert(r_back > 0.);
    assert(r_pin > 0.);

    indexed_triangle_set mesh;

    // 我们创建两个球体，它们将由一个完美贴合两个圆的连接体连接。

    // 设置模型细节级别
    const double detail = 2 * PI / steps;

    // 我们不生成整个圆。相反，我们只生成可见的部分（未被连接体覆盖的部分）
    // 要知道底部和顶部圆的确切部分，我们需要使用一些
    // 相切圆的规则，从中我们可以推导出（使用简单的三角形）以下关系：

    // 整个网格的高度
    const double h   = r_back + r_pin + length;
    double       phi = PI / 2. - std::acos((r_back - r_pin) / h);

    // 要生成整个圆，我们传递 (0, Pi) 的部分
    // 要仅生成一半水平圆，我们可以传递 (0, Pi/2)
    // 计算出的 phi 是对半圆的偏移量，用于平滑
    // 从圆到连接体几何的过渡

    auto &&s1 = sphere(r_back, make_portion(0, PI / 2 + phi), detail);
    auto &&s2 = sphere(r_pin, make_portion(PI / 2 + phi, PI), detail);

    for (auto &p : s2.vertices) p.z() += h;

    its_merge(mesh, s1);
    its_merge(mesh, s2);

    for (size_t idx1 = s1.vertices.size() - steps, idx2 = s1.vertices.size();
         idx1 < s1.vertices.size() - 1; idx1++, idx2++) {
        coord_t i1s1 = coord_t(idx1), i1s2 = coord_t(idx2);
        coord_t i2s1 = i1s1 + 1, i2s2 = i1s2 + 1;

        mesh.indices.emplace_back(i1s1, i2s1, i2s2);
        mesh.indices.emplace_back(i1s1, i2s2, i1s2);
    }

    auto i1s1 = coord_t(s1.vertices.size()) - coord_t(steps);
    auto i2s1 = coord_t(s1.vertices.size()) - 1;
    auto i1s2 = coord_t(s1.vertices.size());
    auto i2s2 = coord_t(s1.vertices.size()) + coord_t(steps) - 1;

    mesh.indices.emplace_back(i2s2, i2s1, i1s1);
    mesh.indices.emplace_back(i1s2, i2s2, i1s1);

    return mesh;
}

indexed_triangle_set halfcone(double       baseheight,
                              double       r_bottom,
                              double       r_top,
                              const Vec3d &pos,
                              size_t       steps)
{
    assert(steps > 0);

    if (baseheight <= 0 || steps <= 0) return {};

    indexed_triangle_set base;

    double a    = 2 * PI / steps;
    auto   last = int(steps - 1);
    Vec3d  ep{pos.x(), pos.y(), pos.z() + baseheight};
    for (size_t i = 0; i < steps; ++i) {
        double phi = i * a;
        auto x   = float(pos.x() + r_top * std::cos(phi));
        auto y   = float(pos.y() + r_top * std::sin(phi));
        base.vertices.emplace_back(x, y, float(ep.z()));
    }

    for (size_t i = 0; i < steps; ++i) {
        double phi = i * a;
        auto x   = float(pos.x() + r_bottom * std::cos(phi));
        auto y   = float(pos.y() + r_bottom * std::sin(phi));
        base.vertices.emplace_back(x, y, float(pos.z()));
    }

    base.vertices.emplace_back(pos.cast<float>());
    base.vertices.emplace_back(ep.cast<float>());

    auto &indices = base.indices;
    auto  hcenter = int(base.vertices.size() - 1);
    auto  lcenter = int(base.vertices.size() - 2);
    auto  offs    = int(steps);
    for (int i = 0; i < last; ++i) {
        indices.emplace_back(i, i + offs, offs + i + 1);
        indices.emplace_back(i, offs + i + 1, i + 1);
        indices.emplace_back(i, i + 1, hcenter);
        indices.emplace_back(lcenter, offs + i + 1, offs + i);
    }

    indices.emplace_back(0, last, offs);
    indices.emplace_back(last, offs + last, offs);
    indices.emplace_back(hcenter, last, 0);
    indices.emplace_back(offs, offs + last, lcenter);

    return base;
}

}} // namespace Slic3r::sla
