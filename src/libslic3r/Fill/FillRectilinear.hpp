#ifndef slic3r_FillRectilinear_hpp_
#define slic3r_FillRectilinear_hpp_

#include "../libslic3r.h"

#include "FillBase.hpp"

namespace Slic3r {

class PrintRegionConfig;
class Surface;

class FillRectilinear : public Fill
{
public:
    Fill* clone() const override { return new FillRectilinear(*this); }
    ~FillRectilinear() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool is_self_crossing() override { return false; }

protected:
    // 通过单向线填充，沿周长互连线。
	bool fill_surface_by_lines(const Surface *surface, const FillParams &params, float angleBase, float pattern_shift, Polylines &polylines_out);


    // 通过不同方向的多重扫描填充。
    struct SweepParams {
        float angle_base;
        float pattern_shift;
    };
    bool fill_surface_by_multilines(const Surface *surface, FillParams params, const std::initializer_list<SweepParams> &sweep_params, Polylines &polylines_out);

    // 覆盖每层任意旋转的整个物体的扩展边界框。
    BoundingBox extended_object_bounding_box() const;
};

class FillAlignedRectilinear : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillAlignedRectilinear(*this); }
    ~FillAlignedRectilinear() override = default;

protected:
    // 始终以相同角度生成填充。
    virtual float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillMonotonic : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillMonotonic(*this); }
    ~FillMonotonic() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
	bool no_sort() const override { return true; }
};

class FillMonotonicLine : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillMonotonicLine(*this); }
    ~FillMonotonicLine() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool no_sort() const override { return true; }
};

class FillGrid : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillGrid(*this); }
    ~FillGrid() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool is_self_crossing() override { return true; }

protected:
	// 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillLateralLattice : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillLateralLattice(*this); }
    ~FillLateralLattice() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;

protected:
	// 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillTriangles : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillTriangles(*this); }
    ~FillTriangles() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool is_self_crossing() override { return true; }

protected:
	// 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillStars : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillStars(*this); }
    ~FillStars() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool is_self_crossing() override { return true; }

protected:
    // 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillCubic : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillCubic(*this); }
    ~FillCubic() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool is_self_crossing() override { return true; }

protected:
	// 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

// Added QuarterCubic pattern from Cura
class FillQuarterCubic : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillQuarterCubic(*this); }
    ~FillQuarterCubic() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;

protected:
	// 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

class FillLateralHoneycomb : public FillAlignedRectilinear
{
public:
    Fill* clone() const override { return new FillLateralHoneycomb(*this); }
    ~FillLateralHoneycomb() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
};


class FillSupportBase : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillSupportBase(*this); }
    ~FillSupportBase() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;

protected:
    // 网格填充将在层之间保持角度恒定，参见 Slic3r::Fill 的实现。
    float _layer_angle(size_t idx) const override { return 0.f; }
};

// Orca: 从 Prusa slicer 引入 FillMonotonicLines，继承自 FillRectilinear
// 这替代了 BBS 的 FillMonotonicLineWGapFill
class FillMonotonicLines : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillMonotonicLines(*this); }
    ~FillMonotonicLines() override = default;
    Polylines fill_surface(const Surface *surface, const FillParams &params) override;
    bool no_sort() const override { return true; }
};

//Orca: Replaced with FillMonotonicLines, inheriting from FillRectilinear
/*class FillMonotonicLineWGapFill : public Fill
{
public:
    ~FillMonotonicLineWGapFill() override = default;
    void fill_surface_extrusion(const Surface *surface, const FillParams &params, ExtrusionEntitiesPtr &out) override;
    bool is_self_crossing() override { return false; }

protected:
    Fill* clone() const override { return new FillMonotonicLineWGapFill(*this); };
    bool no_sort() const override { return true; }

private:
    void fill_surface_by_lines(const Surface* surface, const FillParams& params, Polylines& polylines_out);
};*/

class FillZigZag : public FillRectilinear
{
public:
    Fill* clone() const override { return new FillZigZag(*this); }
    ~FillZigZag() override = default;

    bool has_consistent_pattern() const override { return true; }
};

class FillCrossZag : public FillRectilinear
{
public:
    Fill *clone() const override { return new FillCrossZag(*this); }
    ~FillCrossZag() override = default;

    bool has_consistent_pattern() const override { return true; }
};

class FillLockedZag : public FillRectilinear
{
public:
    Fill *clone() const override { return new FillLockedZag(*this); }
    ~FillLockedZag() override = default;
    LockRegionParam lock_param;

    void fill_surface_extrusion(const Surface *surface, const FillParams &params, ExtrusionEntitiesPtr &out) override;

    bool has_consistent_pattern() const override { return true; }
    void set_lock_region_param(const LockRegionParam &lock_param) override { this->lock_param = lock_param;};
    void fill_surface_locked_zag(const Surface *                          surface,
                                  const FillParams &                       params,
                                  std::vector<std::pair<Polylines, Flow>> &multi_width_polyline);
};

Points sample_grid_pattern(const ExPolygon &expolygon, coord_t spacing, const BoundingBox &global_bounding_box);
Points sample_grid_pattern(const ExPolygons &expolygons, coord_t spacing, const BoundingBox &global_bounding_box);
Points sample_grid_pattern(const Polygons &polygons, coord_t spacing, const BoundingBox &global_bounding_box);

} // namespace Slic3r

#endif // slic3r_FillRectilinear_hpp_
