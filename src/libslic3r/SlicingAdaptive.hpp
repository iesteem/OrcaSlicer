// 基于@platsch的实现

#ifndef slic3r_SlicingAdaptive_hpp_
#define slic3r_SlicingAdaptive_hpp_

#include "Slicing.hpp"
#include "admesh/stl.h"

namespace Slic3r
{

class ModelVolume;

class SlicingAdaptive
{
public:
    void  clear();
    void  set_slicing_parameters(SlicingParameters params) { m_slicing_params = params; }
    void  prepare(const ModelObject &object);
    // 从上一个print_z返回下一个层高，使用质量度量
    //（质量范围从0到1，0 - 低层高时的最高质量，1 - 高层高时的最低打印质量）。
    // 对于质量0.5，层高曲线应大致以默认配置文件的层高为中心。
	float next_layer_height(const float print_z, float quality, size_t &current_facet);
    float horizontal_facet_distance(float z);

	struct FaceZ {
		std::pair<float, float> z_span;
		// Cosine of the normal vector towards the Z axis.
		float					n_cos;
		// Sine of the normal vector towards the Z axis.
		float					n_sin;
	};

protected:
	SlicingParameters 		m_slicing_params;

	std::vector<FaceZ>		m_faces;
};

}; // namespace Slic3r

#endif /* slic3r_SlicingAdaptive_hpp_ */
