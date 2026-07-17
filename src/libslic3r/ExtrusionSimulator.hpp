#ifndef slic3r_ExtrusionSimulator_hpp_
#define slic3r_ExtrusionSimulator_hpp_

#include "libslic3r.h"
#include "ExtrusionEntity.hpp"
#include "BoundingBox.hpp"

namespace Slic3r {

enum ExtrusionSimulationType
{
    ExtrusionSimulationSimple,
    ExtrusionSimulationDontSpread,
    ExtrisopmSimulationSpreadNotOverfilled,
    ExtrusionSimulationSpreadFull,
    ExtrusionSimulationSpreadExcess
};

// 一个不透明类，用于将 boost 内容与头文件隔离。
class ExtrusionSimulatorImpl;

class ExtrusionSimulator
{
public:
    ExtrusionSimulator();
    ~ExtrusionSimulator();

    // 由 image_ptr() 返回的图像大小。
    // 图像可能比视口大，因为许多图形驱动程序
    // 期望纹理的大小被舍入为 2 的幂。
    void  		set_image_size(const Point &image_size);
    // 图像的哪一部分应该被渲染？
    void  		set_viewport(const BoundingBox &viewport);
    // 渲染的挤出路径到视口的平移和缩放。
    void		set_bounding_box(const BoundingBox &bbox);

    // 将所有桶的挤出累加器重置为零。
    void		reset_accumulator();
    // 将粗路径绘制到挤出缓冲区。
    // 目前提供了一个简单的实现，为每个线性段溅射一个矩形挤出。
    // 未来将模拟材料的扩散和挤压。
    void		extrude_to_accumulator(const ExtrusionPath &path, const Point &shift, ExtrusionSimulationType simulationType);
    // 评估累加器的内容并将其绘制到视口中。
    // 在此调用之后，image_ptr() 调用将返回有效的图像。
    void		evaluate_accumulator(ExtrusionSimulationType simulationType);
    // 一个 image_size 的 RGBA 图像，用于加载到 GPU 纹理中。
    const void* image_ptr() const;

private:
	Point					    	image_size;
	BoundingBox				    	viewport;
	BoundingBox 					bbox;

	ExtrusionSimulatorImpl		   *pimpl;
};

}

#endif /* slic3r_ExtrusionSimulator_hpp_ */
