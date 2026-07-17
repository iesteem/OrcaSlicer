#ifndef slic3r_FillGyroid_hpp_
#define slic3r_FillGyroid_hpp_

#include "../libslic3r.h"

#include "FillBase.hpp"

namespace Slic3r {

class FillGyroid : public Fill
{
public:
    FillGyroid() {}
    Fill* clone() const override { return new FillGyroid(*this); }

    // 需要桥接流量，因为此图案大部分悬空
    bool use_bridge_flow() const override { return false; }
    bool is_self_crossing() override { return false; }

    // 应用于常规填充角度的校正，以最大化默认配置下的打印速度（度）
    static constexpr float CorrectionAngle = -45.;

    // 密度调整以获得良好的重量百分比。
    static constexpr double DensityAdjust = 2.44;

    // 陀螺形上部分辨率容差 (mm^-2)
    static constexpr double PatternTolerance = 0.2;


protected:
    void _fill_surface_single(
        const FillParams                &params, 
        unsigned int                     thickness_layers,
        const std::pair<float, Point>   &direction, 
        ExPolygon                        expolygon,
        Polylines                       &polylines_out) override;
};

} // namespace Slic3r

#endif // slic3r_FillGyroid_hpp_
