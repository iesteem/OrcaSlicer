#ifndef MARCHINGSQUARES_HPP
#define MARCHINGSQUARES_HPP

#include <type_traits>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cassert>

namespace marchsq {

// 标记网格中的一个方块
struct Coord {
    long r = 0, c = 0;
    
    Coord() = default;
    explicit Coord(long s) : r(s), c(s) {}
    Coord(long _r, long _c): r(_r), c(_c) {}
    
    size_t seq(const Coord &res) const { return r * res.c + c; }
    Coord& operator+=(const Coord& b) { r += b.r; c += b.c; return *this; }
    Coord operator+(const Coord& b) const { Coord a = *this; a += b; return a; }
};

// 单元格坐标的闭合环
using Ring = std::vector<Coord>;

// 特化此结构体以注册 Marching squares 算法的栅格类型
template<class T, class Enable = void> struct _RasterTraits {
    
    // 栅格中像素单元的类型
    using ValueType = typename T::ValueType;
    
    // 给定位置的值
    static ValueType get(const T &raster, size_t row, size_t col);
    
    // 栅格的行数和列数
    static size_t rows(const T &raster);
    static size_t cols(const T &raster);
};

// 特化此结构以在算法中使用并行循环
template<class ExecutionPolicy, class Enable = void> struct _Loop {
    template<class It, class Fn> static void for_each(It from, It to, Fn &&fn)
    {
        for (auto it = from; it < to; ++it) fn(*it, size_t(it - from));
    }
};

namespace __impl {

template<class T> using RasterTraits = _RasterTraits<std::decay_t<T>>;
template<class T> using TRasterValue = typename RasterTraits<T>::ValueType;

template<class T> size_t rows(const T &raster)
{
    return RasterTraits<T>::rows(raster);
}

template<class T> size_t cols(const T &raster)
{
    return RasterTraits<T>::cols(raster);
}

template<class T> TRasterValue<T> isoval(const T &rst, const Coord &crd)
{
    return RasterTraits<T>::get(rst, crd.r, crd.c);
}

template<class ExecutionPolicy, class It, class Fn>
void for_each(ExecutionPolicy&& policy, It from, It to, Fn &&fn)
{
    _Loop<ExecutionPolicy>::for_each(from, to, fn);
}

// 方块（瓦片）的类型，取决于哪些顶点在 ROI 内部
// 顶点按逆时针顺序标记为 a, b, c, d，从方块的
// 左下顶点开始。
// d --- c
// |     |
// |     |
// a --- b
enum class SquareTag : uint8_t {
//     0, 1, 2,  3, 4,  5,  6,   7, 8,  9, 10,  11, 12,  13,  14,  15
    none, a, b, ab, c, ac, bc, abc, d, ad, bd, abd, cd, acd, bcd, full
};

template<class E> constexpr std::underlying_type_t<E> _t(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

enum class Dir: uint8_t { left, down, right, up, none};

static const constexpr Dir NEXT_CCW[] = {
    /* 00 */ Dir::none,      // SquareTag::none（空方块，无处可去）
    /* 01 */ Dir::left,      // SquareTag::a
    /* 02 */ Dir::down,      // SquareTag::b
    /* 03 */ Dir::left,      // SquareTag::ab
    /* 04 */ Dir::right,     // SquareTag::c
    /* 05 */ Dir::none,      // SquareTag::ac（歧义情况）
    /* 06 */ Dir::down,      // SquareTag::bc
    /* 07 */ Dir::left,      // SquareTag::abc
    /* 08 */ Dir::up,        // SquareTag::d
    /* 09 */ Dir::up,        // SquareTag::ad
    /* 10 */ Dir::none,      // SquareTag::bd（歧义情况）
    /* 11 */ Dir::up,        // SquareTag::abd
    /* 12 */ Dir::right,     // SquareTag::cd
    /* 13 */ Dir::right,     // SquareTag::acd
    /* 14 */ Dir::down,      // SquareTag::bcd
    /* 15 */ Dir::none       // SquareTag::full（完全覆盖，无处可去）
};

static const constexpr uint8_t PREV_CCW[] = {
    /* 00 */ 1 << _t(Dir::none),
    /* 01 */ 1 << _t(Dir::up),      
    /* 02 */ 1 << _t(Dir::left),
    /* 03 */ 1 << _t(Dir::left),     
    /* 04 */ 1 << _t(Dir::down),     
    /* 05 */ 1 << _t(Dir::up) | 1 << _t(Dir::down),      
    /* 06 */ 1 << _t(Dir::down),
    /* 07 */ 1 << _t(Dir::down),
    /* 08 */ 1 << _t(Dir::right),
    /* 09 */ 1 << _t(Dir::up),
    /* 10 */ 1 << _t(Dir::left) | 1 << _t(Dir::right), 
    /* 11 */ 1 << _t(Dir::left),   
    /* 12 */ 1 << _t(Dir::right),
    /* 13 */ 1 << _t(Dir::up),
    /* 14 */ 1 << _t(Dir::right), 
    /* 15 */ 1 << _t(Dir::none)  
};

const constexpr uint8_t DIRMASKS[] = {
    /*left: */ 0x01, /*down*/ 0x12, /*right */0x21, /*up*/ 0x10, /*none*/ 0x00
};

inline Coord step(const Coord &crd, Dir d)
{
    uint8_t dd = DIRMASKS[uint8_t(d)];
    return {crd.r - 1 + (dd & 0x0f), crd.c - 1 + (dd >> 4)};
}

template<class Rst> class Grid {
    const Rst *            m_rst = nullptr;
    Coord                  m_cellsize, m_res_1, m_window, m_gridsize, m_grid_1;
    std::vector<uint8_t>   m_tags;     // 为每个方块分配标签

    Coord rastercoord(const Coord &crd) const
    {
        return {(crd.r - 1) * m_window.r, (crd.c - 1) * m_window.c};
    }

    Coord bl(const Coord &crd) const { return tl(crd) + Coord{m_res_1.r, 0}; }
    Coord br(const Coord &crd) const { return tl(crd) + Coord{m_res_1.r, m_res_1.c}; }
    Coord tr(const Coord &crd) const { return tl(crd) + Coord{0, m_res_1.c}; }
    Coord tl(const Coord &crd) const { return rastercoord(crd); }
    
    bool is_within(const Coord &crd)
    {
        long R = rows(*m_rst), C = cols(*m_rst);
        return crd.r >= 0 && crd.r < R && crd.c >= 0 && crd.c < C;
    };

    // 计算单元格（或方块）的标签。单元格坐标标记
    // 栅格中方块的左上顶点。v 是等值
    uint8_t get_tag_for_cell(const Coord &cell, TRasterValue<Rst> v)
    {        
        Coord sqr[] = {bl(cell), br(cell), tr(cell), tl(cell)};
        
        uint8_t t = ((is_within(sqr[0]) && isoval(*m_rst, sqr[0]) >= v)) +
                    ((is_within(sqr[1]) && isoval(*m_rst, sqr[1]) >= v) << 1) +
                    ((is_within(sqr[2]) && isoval(*m_rst, sqr[2]) >= v) << 2) +
                    ((is_within(sqr[3]) && isoval(*m_rst, sqr[3]) >= v) << 3);
        
        assert(t < 16);
        return t;
    }
    
    // 从顺序索引获取单元格坐标
    Coord coord(size_t i) const
    {
        return {long(i) / m_gridsize.c, long(i) % m_gridsize.c};
    }

    size_t seq(const Coord &crd) const { return crd.seq(m_gridsize); }
    
    bool is_visited(size_t idx, Dir d = Dir::none) const
    {
        SquareTag t = get_tag(idx);
        uint8_t ref = d == Dir::none ? PREV_CCW[_t(t)] : uint8_t(1 << _t(d));
        return t == SquareTag::full || t == SquareTag::none ||
               ((m_tags[idx] & 0xf0) >> 4) == ref;
    }
    
    void set_visited(size_t idx, Dir d = Dir::none)
    {
        m_tags[idx] |= (1 << (_t(d)) << 4);
    }
    
    bool is_ambiguous(size_t idx) const
    {
        SquareTag t = get_tag(idx);
        return t == SquareTag::ac || t == SquareTag::bd;
    }

    // 搜索新的起始方块
    size_t search_start_cell(size_t i = 0) const
    {
        // 由于前一个方向未知，跳过歧义标签作为起始标签。
        while ((i < m_tags.size()) && (is_visited(i) || is_ambiguous(i))) ++i;
        
        return i;
    }
    
    SquareTag get_tag(size_t idx) const { return SquareTag(m_tags[idx] & 0x0f); }
        
    Dir next_dir(Dir prev, SquareTag tag) const
    {
        // 将歧义情况视为一个方块中的两个独立区域。
        switch (tag) {
        case SquareTag::ac:
            switch (prev) {
            case Dir::down: return Dir::right;
            case Dir::up:   return Dir::left;
            default:        assert(false); return Dir::none;
            }
        case SquareTag::bd:
            switch (prev) {
            case Dir::right: return Dir::up;
            case Dir::left:  return Dir::down;
            default:         assert(false); return Dir::none;
            }
        default:
            return NEXT_CCW[uint8_t(tag)];
        }
        
        return Dir::none;
    }
    
    struct CellIt {
        Coord crd; Dir dir= Dir::none; const Rst *grid = nullptr;
        
        TRasterValue<Rst> operator*() const { return isoval(*grid, crd); }
        CellIt& operator++() { crd = step(crd, dir); return *this; }
        CellIt operator++(int) { CellIt it = *this; ++(*this); return it; }
        bool operator!=(const CellIt &it) { return crd.r != it.crd.r || crd.c != it.crd.c; }
        
        using value_type        = TRasterValue<Rst>;
        using pointer           = TRasterValue<Rst> *;
        using reference         = TRasterValue<Rst> &;
        using difference_type   = long;
        using iterator_category = std::forward_iterator_tag;
    };
    
    // 两个单元格迭代器表示方块的边。然后用于二分搜索边上的第一个活动像素。
    struct Edge { CellIt from, to; };
    
    Edge _edge(const Coord &ringvertex) const
    {
        size_t idx = ringvertex.r;
        Coord cell = coord(idx);
        uint8_t tg = m_tags[ringvertex.r];
        SquareTag t = SquareTag(tg & 0x0f);
        
        switch (t) {
        case SquareTag::a:
        case SquareTag::ab:
        case SquareTag::abc:
            return {{tl(cell), Dir::down,  m_rst}, {bl(cell)}};
        case SquareTag::b:
        case SquareTag::bc:
        case SquareTag::bcd:
            return {{bl(cell), Dir::right, m_rst}, {br(cell)}};
        case SquareTag::c:
            return {{br(cell), Dir::up,    m_rst}, {tr(cell)}};
        case SquareTag::ac:
            switch (Dir(ringvertex.c)) {
            case Dir::left:  return {{tl(cell), Dir::down, m_rst}, {bl(cell)}};
            case Dir::right: return {{br(cell), Dir::up,   m_rst}, {tr(cell)}};
            default: assert(false);
            }
        case SquareTag::d:
        case SquareTag::ad:
        case SquareTag::abd:
            return {{tr(cell), Dir::left, m_rst}, {tl(cell)}};
        case SquareTag::bd:
            switch (Dir(ringvertex.c)) {
            case Dir::down: return {{bl(cell), Dir::right, m_rst}, {br(cell)}};
            case Dir::up:   return {{tr(cell), Dir::left,  m_rst}, {tl(cell)}};
            default: assert(false);
            }
        case SquareTag::cd:
        case SquareTag::acd:
            return {{br(cell), Dir::up, m_rst}, {tr(cell)}};
        case SquareTag::full:
        case SquareTag::none: {
            Coord crd{tl(cell) + Coord{m_cellsize.r / 2, m_cellsize.c / 2}};
            return {{crd, Dir::none, m_rst}, {crd}};
        }
        }
        
        return {}; 
    }
    
    Edge edge(const Coord &ringvertex) const
    {
        const long R = rows(*m_rst), C = cols(*m_rst);
        const long R_1 = R - 1, C_1 = C - 1;
        
        Edge e = _edge(ringvertex);
        e.to.dir = e.from.dir;
        ++e.to;
        
        e.from.crd.r = std::min(e.from.crd.r, R_1);
        e.from.crd.r = std::max(e.from.crd.r, 0l);
        e.from.crd.c = std::min(e.from.crd.c, C_1);
        e.from.crd.c = std::max(e.from.crd.c, 0l);
        
        e.to.crd.r = std::min(e.to.crd.r, R);
        e.to.crd.r = std::max(e.to.crd.r, 0l);
        e.to.crd.c = std::min(e.to.crd.c, C);
        e.to.crd.c = std::max(e.to.crd.c, 0l);
        
        return e;
    }
    
public:
    explicit Grid(const Rst &rst, const Coord &cellsz, const Coord &overlap)
        : m_rst{&rst}
        , m_cellsize{cellsz}
        , m_res_1{m_cellsize.r - 1, m_cellsize.c - 1}
        , m_window{overlap.r < cellsz.r ? cellsz.r - overlap.r : cellsz.r,
                   overlap.c < cellsz.c ? cellsz.c - overlap.c : cellsz.c}
        , m_gridsize{2 + (long(rows(rst)) - overlap.r) / m_window.r,
                     2 + (long(cols(rst)) - overlap.c) / m_window.c}
        , m_tags(m_gridsize.r * m_gridsize.c, 0)
    {}
    
    // 遍历单元格并用适当的标签标记它们。
    template<class ExecutionPolicy>
    void tag_grid(ExecutionPolicy &&policy, TRasterValue<Rst> isoval)
    {        
        // 对 r 进行并行循环
        for_each (std::forward<ExecutionPolicy>(policy),
                 m_tags.begin(), m_tags.end(),
                 [this, isoval](uint8_t& tag, size_t idx) {
            tag = get_tag_for_cell(coord(idx), isoval);
        });
    }
    
    // 在标记的网格上扫描环。每个环顶点存储单元格的顺序索引和下一个方向（Dir）。
    // 此信息稍后可用于计算精确的栅格坐标。
    std::vector<Ring> scan_rings()
    {
        std::vector<Ring> rings;
        size_t startidx = 0;
        while ((startidx = search_start_cell(startidx)) < m_tags.size()) {
            Ring ring;
            
            size_t idx = startidx;
            Dir prev = Dir::none, next = next_dir(prev, get_tag(idx));
            
            while (next != Dir::none && !is_visited(idx, prev)) {
                Coord ringvertex{long(idx), long(next)};
                ring.emplace_back(ringvertex);
                set_visited(idx, prev);
                
                idx  = seq(step(coord(idx), next));
                prev = next;
                next = next_dir(next, get_tag(idx));
            }
            
            // 防止在退化输入的情况下出现无限循环
            if (next == Dir::none) m_tags[startidx] = _t(SquareTag::none);
            
            if (ring.size() > 1) {
                ring.pop_back();
                rings.emplace_back(ring);
            }
        }
        
        return rings;
    }
    
    // 从存储方块顺序索引和下一个方向的单元格计算精确的栅格位置
    template<class ExecutionPolicy>
    void interpolate_rings(ExecutionPolicy && policy,
                           std::vector<Ring> &rings,
                           TRasterValue<Rst>  isov)
    {
        for_each(std::forward<ExecutionPolicy>(policy),
                 rings.begin(), rings.end(), [this, isov] (Ring &ring, size_t)
        {
            for (Coord &ringvertex : ring) {
                Edge e = edge(ringvertex);
                
                CellIt found = std::lower_bound(e.from, e.to, isov);
                ringvertex = found.crd;
            }
        });
    }
};

template<class Raster, class ExecutionPolicy>
std::vector<marchsq::Ring> execute_with_policy(ExecutionPolicy &&   policy,
                                               const Raster &       raster,
                                               TRasterValue<Raster> isoval,
                                               Coord windowsize = {})
{
    if (!rows(raster) || !cols(raster)) return {};
    
    size_t ratio = cols(raster) / rows(raster);
    
    if (!windowsize.r) windowsize.r = 2;
    if (!windowsize.c)
        windowsize.c = std::max(2l, long(windowsize.r * ratio));
    
    Coord overlap{1};
    
    Grid<Raster> grid{raster, windowsize, overlap};
    
    grid.tag_grid(std::forward<ExecutionPolicy>(policy), isoval);
    std::vector<marchsq::Ring> rings = grid.scan_rings();
    grid.interpolate_rings(std::forward<ExecutionPolicy>(policy), rings, isoval);
    
    return rings;
}

template<class Raster>
std::vector<marchsq::Ring> execute(const Raster &raster,
                                   TRasterValue<Raster> isoval,
                                   Coord                windowsize = {})
{
    return execute_with_policy(nullptr, raster, isoval, windowsize);
}

} // namespace __impl

using __impl::execute_with_policy;
using __impl::execute;

} // namespace marchsq

#endif // MARCHINGSQUARES_HPP
