#ifndef slic3r_FillTpmsFK_hpp_
#define slic3r_FillTpmsFK_hpp_

#include <utility>

#include "libslic3r/libslic3r.h"
#include "FillBase.hpp"
#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Polyline.hpp"

namespace Slic3r {
class Point;

class FillTpmsFK : public Fill
{
public:
    FillTpmsFK() {}
    Fill* clone() const override { return new FillTpmsFK(*this); }

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

};

} // namespace Slic3r

#endif // slic3r_FillTpmsFK_hpp_