#ifndef slic3r_FillLightning_hpp_
#define slic3r_FillLightning_hpp_

#include "FillBase.hpp"

namespace Slic3r {

class PrintObject;

namespace FillLightning {

class Generator;
// 为了保持 Octree 定义的不透明性，我们必须定义自定义删除器。
struct GeneratorDeleter { void operator()(Generator *p); };
using  GeneratorPtr = std::unique_ptr<Generator, GeneratorDeleter>;

GeneratorPtr build_generator(const PrintObject &print_object, const std::function<void()> &throw_on_cancel_callback);

class Filler : public Slic3r::Fill
{
public:
    ~Filler() override = default;
    bool is_self_crossing() override { return false; }

    Generator   *generator { nullptr };
protected:
    Fill* clone() const override { return new Filler(*this); }

    void _fill_surface_single(const FillParams              &params,
                              unsigned int                   thickness_layers,
                              const std::pair<float, Point> &direction,
                              ExPolygon                      expolygon,
                              Polylines &polylines_out) override;

    // 让 G 代码导出器重新排序填充线。
	bool no_sort() const override { return false; }
};

} // namespace FillAdaptive
} // namespace Slic3r

#endif // slic3r_FillLightning_hpp_
