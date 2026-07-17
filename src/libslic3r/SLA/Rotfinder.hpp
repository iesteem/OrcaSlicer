#ifndef SLA_ROTFINDER_HPP
#define SLA_ROTFINDER_HPP

#include <functional>
#include <array>

#include <libslic3r/Point.hpp>

namespace Slic3r {

class ModelObject;
class SLAPrintObject;
class TriangleMesh;
class DynamicPrintConfig;

namespace sla {

using RotOptimizeStatusCB = std::function<bool(int)>;

class RotOptimizeParams {
    float m_accuracy = 1.;
    const DynamicPrintConfig *m_print_config = nullptr;
    RotOptimizeStatusCB m_statuscb = [](int) { return true; };

public:

    RotOptimizeParams &accuracy(float a) { m_accuracy = a; return *this; }
    RotOptimizeParams &print_config(const DynamicPrintConfig *c)
    {
        m_print_config = c;
        return *this;
    }
    RotOptimizeParams &statucb(RotOptimizeStatusCB cb)
    {
        m_statuscb = std::move(cb);
        return *this;
    }

    float accuracy() const { return m_accuracy; }
    const DynamicPrintConfig * print_config() const { return m_print_config; }
    const RotOptimizeStatusCB &statuscb() const { return m_statuscb; }
};

/**
  * 该函数应找到 SLA 倒置打印的最佳旋转角度。
  *
  * @param modelobj 表示 3D 网格的模型对象。
  * @param accuracy 优化精度，范围从 0.0f 到 1.0f。当前，
  * 使用 nlopt 遗传优化器，迭代次数为 accuracy * 100000。未来可能改变。
  * @param statuscb 状态指示回调函数，参数为 0 到 100 的整数。
  * 如果优化在达到最大迭代次数前找到最优解，可能不会达到 100。
  * 应返回布尔值，指示操作是否可以继续（true）或停止（false）。
  * 低于 0 的状态值不应更新状态，但仍返回有效的继续指示。
  *
  * @return 返回绕每个轴（x, y, z）的旋转角度
  */
Vec2d find_best_misalignment_rotation(const ModelObject &modelobj,
                                      const RotOptimizeParams & = {});

Vec2d find_least_supports_rotation(const ModelObject &modelobj,
                                   const RotOptimizeParams & = {});

Vec2d find_min_z_height_rotation(const ModelObject &mo,
                                 const RotOptimizeParams &params = {});

} // namespace sla
} // namespace Slic3r

#endif // SLAROTFINDER_HPP
