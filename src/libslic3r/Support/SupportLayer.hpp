#ifndef slic3r_SupportLayer_hpp_
#define slic3r_SupportLayer_hpp_

#include <oneapi/tbb/scalable_allocator.h>
#include <oneapi/tbb/spin_mutex.h>
// for Slic3r::deque
#include "../libslic3r.h"
#include "../ClipperUtils.hpp"
#include "../Polygon.hpp"

namespace Slic3r {

// SupportGeneratorLayer使用的支撑层类型。此类型携带比PrintObject中存储的最终支撑层更详细的支撑层类型信息。
enum class SupporLayerType {
	Unknown = 0,
	// 筏基底层，使用支撑材料打印。
	RaftBase,
	// 筏界面层，使用支撑界面材料打印。
	RaftInterface,
	// 放置在对象顶面上的底部接触层。使用支撑界面材料打印。
	BottomContact,
	// 致密界面层，使用支撑界面材料打印。
	// 此层通过BottomContact层与对象分离。
	BottomInterface,
	// 稀疏基础支撑层，使用支撑材料打印。
	Base,
	// 致密界面层，使用支撑界面材料打印。
	// 此层通过TopContact层与对象分离。
	TopInterface,
	// 直接支撑悬垂的顶部接触层。使用支撑界面材料打印。
	TopContact,
	// 尚未确定的类型。它将首先变为Base，然后可能变为BottomInterface或TopInterface。
	Intermediate,
};

// SupportMaterial类内部使用的支撑层类型。此类携带比PrintObject中存储的层更详细的支撑层信息，
// 主要SupportGeneratorLayer了解桥接流量以及对象与支撑之间的界面间隙。
class SupportGeneratorLayer
{
public:
	void reset() {
		*this = SupportGeneratorLayer();
	}

	bool operator==(const SupportGeneratorLayer &layer2) const {
		return print_z == layer2.print_z && height == layer2.height && bridging == layer2.bridging;
	}

	// 按print_z递增和layer_height递减的字典序对层排序。
	bool operator<(const SupportGeneratorLayer &layer2) const {
		if (print_z < layer2.print_z) {
			return true;
		} else if (print_z == layer2.print_z) {
		 	if (height > layer2.height)
		 		return true;
		 	else if (height == layer2.height) {
		 		// 桥接层优先。
		 	 	return bridging && ! layer2.bridging;
		 	} else
		 		return false;
		} else
			return false;
	}

	void merge(SupportGeneratorLayer &&rhs) {
        // union_()尚不支持移动语义，但也许有一天它会支持。
        this->polygons = union_(this->polygons, std::move(rhs.polygons));
        auto merge = [](std::unique_ptr<Polygons> &dst, std::unique_ptr<Polygons> &src) {
        	if (! dst || dst->empty())
        		dst = std::move(src);
        	else if (src && ! src->empty())
    			*dst = union_(*dst, std::move(*src));
        };
        merge(this->contact_polygons,  rhs.contact_polygons);
        merge(this->overhang_polygons, rhs.overhang_polygons);
        merge(this->enforcer_polygons, rhs.enforcer_polygons);
        rhs.reset();
    }

	// 对于桥接流，bottom_print_z将高于bottom_z以考虑垂直分离。
	// 对于非桥接流，bottom_print_z将等于bottom_z。
	coordf_t bottom_print_z() const { return print_z - height; }

	// 用于排序顶部/底部界面层的极值。
	coordf_t extreme_z() const { return (this->layer_type == SupporLayerType::TopContact) ? this->bottom_z : this->print_z; }

	SupporLayerType layer_type { SupporLayerType::Unknown };
	// 用于打印的Z坐标，未缩放坐标。
	coordf_t print_z { 0 };
	// 此层的底部Z。对于可溶性层，bottom_z + height = print_z，
	// 否则 bottom_z + gap + height = print_z。
	coordf_t bottom_z { 0 };
	// 未缩放坐标中的层高。
	coordf_t height { 0 };
	// 此层支撑的PrintObject层索引。仅对顶部接触层设置。
	// 如果不是接触层，则设置为size_t(-1)。
	size_t 	 idx_object_layer_above { size_t(-1) };
	// 支撑此层的PrintObject层索引。仅对底部接触层设置。
	// 如果不是接触层，则设置为size_t(-1)。
	size_t 	 idx_object_layer_below { size_t(-1) };
	// 打印此支撑层时使用桥接流。
	bool 	 bridging { false };

	// 由支撑图案填充的多边形。
	Polygons polygons;
	// 目前仅用于接触层。
	std::unique_ptr<Polygons> contact_polygons;
	std::unique_ptr<Polygons> overhang_polygons;
	// 在启用"仅打印板支撑"选项时，强制器需要独立传播。
	std::unique_ptr<Polygons> enforcer_polygons;
};

// 层由deque分配和拥有。一旦分配了层，它将一直维护到generate()方法的末尾。
// 未来层存储可能由分配器类替换，该类将通过多个块分配层。
class SupportGeneratorLayerStorage {
public:
	SupportGeneratorLayer& allocate_unguarded(SupporLayerType layer_type) { 
		m_storage.emplace_back();
		m_storage.back().layer_type = layer_type;
	    return m_storage.back();
	}

	SupportGeneratorLayer& allocate(SupporLayerType layer_type)
	{ 
		m_mutex.lock();
		m_storage.emplace_back();
	    SupportGeneratorLayer *layer_new = &m_storage.back();
		m_mutex.unlock();
	    layer_new->layer_type = layer_type;
	    return *layer_new;
	}

private:
	template<typename BaseType>
	using Allocator = tbb::scalable_allocator<BaseType>;
	Slic3r::deque<SupportGeneratorLayer, Allocator<SupportGeneratorLayer>> 		m_storage;
	tbb::spin_mutex                         									m_mutex;
};
using SupportGeneratorLayersPtr		= std::vector<SupportGeneratorLayer*>;

} // namespace Slic3r

#endif /* slic3r_SupportLayer_hpp_ */
