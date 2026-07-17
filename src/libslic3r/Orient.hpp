#ifndef ORIENT_HPP
#define ORIENT_HPP

#include "libslic3r/Model.hpp"

namespace Slic3r {

namespace orientation {


/// 表示尚未定向对象的逻辑热床。定向尚未在此 OrientPolygon 上成功运行，
/// 或者由于尺寸过大或几何形状无效而无法容纳该对象。
static const constexpr int UNORIENTD = -1;

/// orient() 函数的输入/输出结构。mesh 字段在定向过程中不会被修改。
/// 相反，translation 和 rotation 字段将标记多边形所需变换到定向位置的转换。
/// 这些也可以设置为初始偏移和旋转。
///
/// bed_idx 字段将指示多边形所属的逻辑热床：UNORIENTD 表示多边形没有位置
///（也是定向前的初始状态），0..N 表示热床的索引。
/// 0 是物理热床，大于 0 表示虚拟热床。
struct OrientMesh {
    TriangleMesh mesh;              /// 实际的网格数据
    double overhang_angle = 30;
    double angle{ 0 };
    Vec3d axis{ 0,0,1 };
    Vec3d orientation{ 0,0,1 };
    Matrix3d rotation_matrix;
    Vec3d euler_angles;
    std::string name;

    /// 可选的设置器函数，可以在其闭包中存储任意数据
    std::function<void(const OrientMesh&)> setter = nullptr;

    /// 使用定向数据参数调用设置器的辅助函数
    void apply() const { if (setter) setter(*this); }

};

// 用于最小化支撑面积的参数
struct OrientParamsArea {
    float TAR_A = 0.015f;
    float TAR_B = 0.177f;
    float RELATIVE_F = 20;
    float CONTOUR_F = 0.5f;
    float BOTTOM_F = 2.5f;
    float BOTTOM_HULL_F = 0.1f;
    float TAR_C = 0.1f;
    float TAR_D = 1;
    float TAR_E = 0.0115f;
    float FIRST_LAY_H = 0.2f;//0.0475;
    float VECTOR_TOL = -0.00083f;
    float NEGL_FACE_SIZE = 0.01f;
    float ASCENT = -0.5f;
    float PLAFOND_ADV = 0.0599f;
    float CONTOUR_AMOUNT = 0.0182427f;
    float OV_H = 2.574f;
    float height_offset = 2.3728f;
    float height_log = 0.041375f;
    float height_log_k = 1.9325457f;
    float LAF_MAX = 0.999f; // 低角度面的 cos(1.4\degree) 0.9997f
    float LAF_MIN = 0.97f;  // cos(14\degree) 0.9703f
    float TAR_LAF = 0.001f; //0.01f
    float TAR_PROJ_AREA = 0.1f;
    float BOTTOM_MIN = 0.1f;  // 最小底部面积。如果低于此值，物体可能不稳定
    float BOTTOM_MAX = 2000;  // 最大底部面积。如果达到此值，物体足够稳定（进一步增加底部面积不会有更多帮助）
    float height_to_bottom_hull_ratio_MIN = 1;
    float BOTTOM_HULL_MAX = 2000;// 最大底部包络面积
    float APPERANCE_FACE_SUPP=3; // 在外观面上生成支撑的惩罚

    float overhang_angle = 60.f;
    bool use_low_angle_face = true;
    bool min_volume = false;
    Eigen::Vector3f fun_dir;

    /// 允许并行执行。
    bool parallel = true;

    /// 当对象被包装时调用的进度指示器回调。
    /// unsigned 参数是剩余要包装的项目数。
    std::function<void(unsigned, std::string)> progressind = {};

    /// 如果需要中止则返回 true 的谓词。
    std::function<bool(void)>     stopcondition = {};

    OrientParamsArea() = default;
};

struct OrientParams {
    float TAR_A = 0.01f;//0.128f;
    float TAR_B = 0.177f;
    float RELATIVE_F= 6.610621027964314f;
    float CONTOUR_F = 0.23228623269775997f;
    float BOTTOM_F = 1.167152017941474f;
    float BOTTOM_HULL_F = 0.1f;
    float TAR_C = 0.24308070476924726f;
    float TAR_D = 0.6284515508160871f;
    float TAR_E = 0;//0.032157292647062234;
    float FIRST_LAY_H = 0.2f;//0.029;
    float VECTOR_TOL = -0.0011163303070972383f;
    float NEGL_FACE_SIZE = 0.1f;
    float ASCENT= -0.5f;
    float PLAFOND_ADV = 0.04079208948120519f;
    float CONTOUR_AMOUNT = 0.0101472219892684f;
    float OV_H = 1.0370178217794535f;
    float height_offset = 2.7417608343142073f;
    float height_log = 0.06442030687034085f;
    float height_log_k = 0.3933594673063997f;
    float LAF_MAX = 0.999f; // 低角度面的 cos(1.4\degree) //0.9997f;
    float LAF_MIN= 0.9703f;  // cos(14\degree) 0.9703f;
    float TAR_LAF = 0.01f; //0.1f
    float TAR_PROJ_AREA = 0.1f;
    float BOTTOM_MIN = 0.1f;  // min bottom area. If lower than it the objects may be unstable
    float BOTTOM_MAX = 2000; //400
    float height_to_bottom_hull_ratio_MIN = 1;
    float BOTTOM_HULL_MAX = 2000;// 最大底部包络面积 to clip //600
    float APPERANCE_FACE_SUPP=3; // 在外观面上生成支撑的惩罚

    float overhang_angle = 60.f;
    bool use_low_angle_face = true;
    bool min_volume = false;
    Eigen::Vector3f fun_dir;


    /// 允许并行执行。
    bool parallel = false;

    /// 当对象被包装时调用的进度指示器回调。
    /// unsigned 参数是剩余要包装的项目数。
    std::function<void(unsigned, std::string)> progressind = {};

    /// 如果需要中止则返回 true 的谓词。
    std::function<bool(void)>     stopcondition = {};

    OrientParams() = default;
};

using OrientMeshs = std::vector<OrientMesh>;

/**
 * \brief Orients the input polygons.

 * \param items Input vector of OrientMeshs. The transformation, rotation
 * and bin_idx fields will be changed after the call finished and can be used
 * to apply the result on the input polygon.
 */
void orient(OrientMeshs &items, const OrientMeshs &excludes, const OrientParams &params = {});

// this function should be deleted, since rotating objects are so complicated that its inherited transformation may be a trouble
void orient(ModelObject* obj);

void orient(ModelInstance* instance);

}} // namespace Slic3r::orientment

#endif // MODELORIENT_HPP
