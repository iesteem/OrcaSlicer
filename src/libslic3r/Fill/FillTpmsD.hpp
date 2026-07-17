#ifndef slic3r_FillTpmsD_hpp_
#define slic3r_FillTpmsD_hpp_

#include <utility>

#include "libslic3r/libslic3r.h"
#include "FillBase.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Polyline.hpp"

namespace Slic3r {
class Point;

class FillTpmsD : public Fill
{
public:
    FillTpmsD() {}
    Fill* clone() const override { return new FillTpmsD(*this); }

    // 需要桥接流量，因为此图案大部分悬空
    bool use_bridge_flow() const override { return false; }
  

    // 应用于常规填充角度的校正，以最大化默认配置下的打印速度（度）
    static constexpr float CorrectionAngle = -45.;

    void _fill_surface_single(const FillParams&              params,
                              unsigned int                   thickness_layers,
                              const std::pair<float, Point>& direction,
                              ExPolygon                      expolygon,
                              Polylines&                     polylines_out) override;

    bool is_self_crossing() override { return false; }

    // 密度调整以获得良好的重量百分比。
    static constexpr double DensityAdjust = 2.1;

    // 陀螺形上部分辨率容差 (mm^-2)
    static constexpr double PatternTolerance = 0.1;

};

} // namespace Slic3r

#endif // slic3r_FillTpmsD_hpp_