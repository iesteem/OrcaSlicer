#ifndef _libslic3r_h_
#define _libslic3r_h_

#include "common_func/common_func.hpp"
#define SLIC3R_APP_FULL_NAME "Snapmaker Orca"
#define GCODEVIEWER_APP_NAME "Snapmaker_Orca G-code Viewer"
#define GCODEVIEWER_APP_KEY  "Snapmaker_OrcaGcodeViewer"
#define GCODEVIEWER_BUILD_ID std::string("Snapmaker_Orca G-code Viewer-") + std::string(SLIC3R_VERSION) + std::string("-RC")

// 这需要为MSVC早期包含（仅在Build.PL中列出是不够的）
#include <memory>
#include <array>
#include <algorithm>
#include <ostream>
#include <iostream>
#include <math.h>
#include <queue>
#include <sstream>
#include <cstdio>
#include <stdint.h>
#include <stdarg.h>
#include <vector>
#include <cassert>
#include <cmath>
#include <type_traits>
#include <optional>

#ifdef _WIN32
// 在MSVC上，std::deque退化为指针列表，这违背了其减少分配器负载和内存碎片的目的。
// https://github.com/microsoft/STL/issues/147#issuecomment-1090148740
// 因此建议使用boost::container::deque代替。
#include <boost/container/deque.hpp>
#endif // _WIN32

#include "Technologies.hpp"
#include "Semver.hpp"

#if 0
// 切片步骤后节省约32% RAM，G-code导出后节省6.7%（在PrusaSlicer 2.2.0正式版上测试）。
using coord_t = int32_t;
#else
//FIXME 至少FillRectilinear2和std::boost Voronoi要求coord_t为32位。
using coord_t = int64_t;
#endif

using coordf_t = double;

//FIXME 这个epsilon值用于许多不相关的目的：
// 作为欧几里得距离平方的阈值，
// 作为弧度差的阈值，
// 作为两个非归一化向量叉积的阈值等。
static constexpr double EPSILON = 1e-4;
// 从coord_t到coordf_t转换的缩放因子：10e-6
// 此缩放为32位整数生成以下定点表示：
// 0..4294mm，1nm分辨率
// int32_t适合(-2147.48mm, +2147.48mm)的区间
// 使用int64_t我们不必再担心int的大小。

// Orca todo: 可能最好全部使用1e-5，3D打印不需要纳米级分辨率
static constexpr double SCALING_FACTOR_INTERNAL = 0.000001;
static constexpr double SCALING_FACTOR_INTERNAL_LARGE_PRINTER = 0.00001;
static constexpr double LARGE_BED_THRESHOLD = 2147;

// Orca: maximum number of extruders is 64. For SEMM printers, it defines maximum filament number.
static constexpr size_t MAXIMUM_EXTRUDER_NUMBER = 64;

// Combined upper limit for physical + mixed filaments.
static constexpr size_t MAXIMUM_FILAMENT_NUMBER = 64;

// Orca: maximum line width is 5 times the nozzle diameter
static constexpr float MAX_LINE_WIDTH_MULTIPLIER = 5;

extern double SCALING_FACTOR;
static constexpr double PI = 3.141592653589793238;
#define POLY_SIDE_COUNT 24 // for brim ear circle
// 挤出闭合回路时，回路会被打断并缩短一点以减少接缝。
// SoftFever: 现已被seam_gap替代
// static constexpr double LOOP_CLIPPING_LENGTH_OVER_NOZZLE_DIAMETER = 0.15;
static constexpr double RESOLUTION = 0.0125;
#define                 SCALED_RESOLUTION (RESOLUTION / SCALING_FACTOR)
static constexpr double SPARSE_INFILL_RESOLUTION = 0.04;
#define                 SCALED_SPARSE_INFILL_RESOLUTION (SPARSE_INFILL_RESOLUTION / SCALING_FACTOR)

static constexpr double SUPPORT_RESOLUTION = 0.0375;
#define                 SCALED_SUPPORT_RESOLUTION (SUPPORT_RESOLUTION / SCALING_FACTOR)
// 回路应用小周长速度的最大周长长度。 
#define                 SMALL_PERIMETER_LENGTH(LENGTH)  (((LENGTH) / SCALING_FACTOR) * 2 * PI)
static constexpr double INSET_OVERLAP_TOLERANCE = 0.4;
// 顶部/底部/桥接区域周围的3mm环。
//FIXME 这相当多。
static constexpr double EXTERNAL_INFILL_MARGIN = 3;
static constexpr double BRIDGE_INFILL_MARGIN = 1;
static constexpr double WIPE_TOWER_MARGIN = 1.;
//FIXME 最好使用具有显式返回类型的内联函数。
//inline coord_t scale_(coordf_t v) { return coord_t(floor(v / SCALING_FACTOR + 0.5f)); }
#define scale_(val) ((val) / SCALING_FACTOR)
#define unscale_(val) ((val) * SCALING_FACTOR)

#define SCALED_EPSILON scale_(EPSILON)

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif /* UNUSED */

//BBS: some global const config which user can not change, but developer can
static constexpr bool g_config_support_sharp_tails = true;
static constexpr bool g_config_remove_small_overhangs = true;
static constexpr float g_config_tree_support_collision_resolution = 0.2;

// Write slices as SVG images into out directory during the 2D processing of the slices.
//#define SLIC3R_DEBUG_SLICE_PROCESSING

namespace Slic3r {

extern Semver SEMVER;

// On MSVC, std::deque degenerates to a list of pointers, which defeats its purpose of reducing allocator load and memory fragmentation.
template<class T, class Allocator = std::allocator<T>>
using deque =
#ifdef _WIN32
    // Use boost implementation, which allocates blocks of 512 bytes instead of blocks of 8 bytes.
    boost::container::deque<T, Allocator>;
#else  // _WIN32
    std::deque<T, Allocator>;
#endif // _WIN32

template<typename T, typename Q>
inline T unscale(Q v) { return T(v) * T(SCALING_FACTOR); }

enum Axis { 
	X=0,
	Y,
	Z,
	E,
	F,
    //BBS: add I, J, P axis
    I,
    J,
    P,
	NUM_AXES,
	// For the GCodeReader to mark a parsed axis, which is not in "XYZEF", it was parsed correctly.
	UNKNOWN_AXIS = NUM_AXES,
	NUM_AXES_WITH_UNKNOWN,
};

template <typename T, typename Alloc, typename Alloc2>
inline void append(std::vector<T, Alloc> &dest, const std::vector<T, Alloc2> &src)
{
    if (dest.empty())
        dest = src;
    else
        dest.insert(dest.end(), src.begin(), src.end());
}

template <typename T, typename Alloc>
inline void append(std::vector<T, Alloc> &dest, std::vector<T, Alloc> &&src)
{
    if (dest.empty())
        dest = std::move(src);
    else {
        dest.reserve(dest.size() + src.size());
        std::move(std::begin(src), std::end(src), std::back_inserter(dest));
    }
    src.clear();
    src.shrink_to_fit();
}

template<class T, class... Args> // Arbitrary allocator can be used
void clear_and_shrink(std::vector<T, Args...>& vec)
{
    // shrink_to_fit does not garantee the release of memory nor does it clear()
    std::vector<T, Args...> tmp;
    vec.swap(tmp);
    assert(vec.capacity() == 0);
}

// Append the source in reverse.
template <typename T>
inline void append_reversed(std::vector<T>& dest, const std::vector<T>& src)
{
    if (dest.empty())
        dest = src;
    else
        dest.insert(dest.end(), src.rbegin(), src.rend());
}

// Append the source in reverse.
template <typename T>
inline void append_reversed(std::vector<T>& dest, std::vector<T>&& src)
{
    if (dest.empty())
        dest = std::move(src);
    else {
        dest.reserve(dest.size() + src.size());
        std::move(std::rbegin(src), std::rend(src), std::back_inserter(dest));
    }
    src.clear();
    src.shrink_to_fit();
}

// Casting an std::vector<> from one type to another type without warnings about a loss of accuracy.
template<typename T_TO, typename T_FROM>
std::vector<T_TO> cast(const std::vector<T_FROM> &src) 
{
    std::vector<T_TO> dst;
    dst.reserve(src.size());
    for (const T_FROM &a : src)
        dst.emplace_back((T_TO)a);
    return dst;
}

template <typename T>
inline void remove_nulls(std::vector<T*> &vec)
{
	vec.erase(
    	std::remove_if(vec.begin(), vec.end(), [](const T *ptr) { return ptr == nullptr; }),
    	vec.end());
}

template <typename T>
inline void sort_remove_duplicates(std::vector<T> &vec)
{
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

// Older compilers do not provide a std::make_unique template. Provide a simple one.
template<typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Variant of std::lower_bound() with compare predicate, but without the key.
// This variant is very useful in case that the T type is large or it does not even have a public constructor.
template<class ForwardIt, class LowerThanKeyPredicate>
ForwardIt lower_bound_by_predicate(ForwardIt first, ForwardIt last, LowerThanKeyPredicate lower_than_key)
{
    ForwardIt it;
    typename std::iterator_traits<ForwardIt>::difference_type count, step;
    count = std::distance(first, last);
 
    while (count > 0) {
        it = first;
        step = count / 2;
        std::advance(it, step);
        if (lower_than_key(*it)) {
            first = ++it;
            count -= step + 1;
        }
        else
            count = step;
    }
    return first;
}

// from https://en.cppreference.com/w/cpp/algorithm/lower_bound
template<class ForwardIt, class T, class Compare=std::less<>>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp={})
{
    // Note: BOTH type T and the type after ForwardIt is dereferenced 
    // must be implicitly convertible to BOTH Type1 and Type2, used in Compare. 
    // This is stricter than lower_bound requirement (see above)
 
    first = std::lower_bound(first, last, value, comp);
    return first != last && !comp(value, *first) ? first : last;
}

// from https://en.cppreference.com/w/cpp/algorithm/lower_bound
template<class ForwardIt, class LowerThanKeyPredicate, class EqualToKeyPredicate>
ForwardIt binary_find_by_predicate(ForwardIt first, ForwardIt last, LowerThanKeyPredicate lower_thank_key, EqualToKeyPredicate equal_to_key)
{
    // Note: BOTH type T and the type after ForwardIt is dereferenced 
    // must be implicitly convertible to BOTH Type1 and Type2, used in Compare. 
    // This is stricter than lower_bound requirement (see above)
 
    first = lower_bound_by_predicate(first, last, lower_thank_key);
    return first != last && equal_to_key(*first) ? first : last;
}

template<typename ContainerType, typename ValueType> inline bool contains(const ContainerType &c, const ValueType &v)
    { return std::find(c.begin(), c.end(), v) != c.end(); }
template<typename T> inline bool contains(const std::initializer_list<T> &il, const T &v)
    { return std::find(il.begin(), il.end(), v) != il.end(); }

template<typename ContainerType, typename ValueType> inline bool one_of(const ValueType &v, const ContainerType &c)
    { return contains(c, v); }
template<typename T> inline bool one_of(const T& v, const std::initializer_list<T>& il)
    { return contains(il, v); }

template<typename T>
constexpr inline T sqr(T x)
{
    return x * x;
}

template<typename Number> constexpr 
inline bool is_zero(Number value)
{
    return std::fabs(double(value)) < 1e-6;
}

template <typename T, typename Number>
constexpr inline T lerp(const T& a, const T& b, Number t)
{
    assert((t >= Number(-EPSILON)) && (t <= Number(1) + Number(EPSILON)));
    return (Number(1) - t) * a + t * b;
}

template <typename Number>
constexpr inline bool is_approx(Number value, Number test_value, Number precision = EPSILON)
{
    return std::fabs(double(value) - double(test_value)) < double(precision);
}

template<typename Number>
constexpr inline bool is_approx(const std::optional<Number> &value,
                                const std::optional<Number> &test_value)
{
    return (!value.has_value() && !test_value.has_value()) ||
        (value.has_value() && test_value.has_value() && is_approx<Number>(*value, *test_value));
}

// A meta-predicate which is true for integers wider than or equal to coord_t
template<class I> struct is_scaled_coord
{
    static const constexpr bool value =
        std::is_integral<I>::value &&
        std::numeric_limits<I>::digits >=
            std::numeric_limits<coord_t>::digits;
};

// Meta predicates for floating, 'scaled coord' and generic arithmetic types
// Can be used to restrict templates to work for only the specified set of types.
// parameter T is the type we want to restrict
// parameter O (Optional defaults to T) is the type that the whole expression
// will be evaluated to.
// e.g. template<class T> FloatingOnly<T, bool> is_nan(T val);
// The whole template will be defined only for floating point types and the
// return type will be bool.
// For more info how to use, see docs for std::enable_if
//
template<class T, class O = T> 
using FloatingOnly = std::enable_if_t<std::is_floating_point<T>::value, O>;

template<class T, class O = T>
using ScaledCoordOnly = std::enable_if_t<is_scaled_coord<T>::value, O>;

template<class T, class O = T>
using IntegerOnly = std::enable_if_t<std::is_integral<T>::value, O>;

template<class T, class O = T>
using ArithmeticOnly = std::enable_if_t<std::is_arithmetic<T>::value, O>;

template<class T, class O = T>
using IteratorOnly = std::enable_if_t<
    !std::is_same_v<typename std::iterator_traits<T>::value_type, void>, O
>;

template<class T, class I, class... Args> // Arbitrary allocator can be used
IntegerOnly<I, std::vector<T, Args...>> reserve_vector(I capacity)
{
    std::vector<T, Args...> ret;
    if (capacity > I(0)) ret.reserve(size_t(capacity));

    return ret;
}

// Borrowed from C++20
template<class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// A very simple range concept implementation with iterator-like objects.
// This should be replaced by std::ranges::subrange (C++20)
template<class It> class Range
{
    It from, to;
public:

    // The class is ready for range based for loops.
    It begin() const { return from; }
    It end() const { return to; }

    // The iterator type can be obtained this way.
    using iterator = It;
    using value_type = typename std::iterator_traits<It>::value_type;

    Range() = default;
    Range(It b, It e) : from(std::move(b)), to(std::move(e)) {}

    // Some useful container-like methods...
    inline size_t size() const { return std::distance(from, to); }
    inline bool   empty() const { return from == to; }
};

template<class Cont> auto range(Cont &&cont)
{
    return Range{std::begin(cont), std::end(cont)};
}

template<class T, class = FloatingOnly<T>>
constexpr T NaN = std::numeric_limits<T>::quiet_NaN();

constexpr float  NaNf = NaN<float>;
constexpr double NaNd = NaN<double>;

// Rounding up.
// 1.5 is rounded to 2
// 1.49 is rounded to 1
// 0.5 is rounded to 1,
// 0.49 is rounded to 0
// -0.5 is rounded to 0,
// -0.51 is rounded to -1,
// -1.5 is rounded to -1.
// -1.51 is rounded to -2.
// If input is not a valid float (it is infinity NaN or if it does not fit)
// the float to int conversion produces a max int on Intel and +-max int on ARM.
template<typename I>
inline IntegerOnly<I, I> fast_round_up(double a)
{
    // Why does Java Math.round(0.49999999999999994) return 1?
    // https://stackoverflow.com/questions/9902968/why-does-math-round0-49999999999999994-return-1
    return a == 0.49999999999999994 ? I(0) : I(floor(a + 0.5));
}

template<class T> using SamePair = std::pair<T, T>;

} // namespace Slic3r

#endif
