#ifndef CSGMESH_HPP
#define CSGMESH_HPP

#include <libslic3r/AnyPtr.hpp>
#include <admesh/stl.h>

namespace Slic3r { namespace csg {

//// 一个 CSGPartT 应为一个对象，该可以提供至少一个 mesh + trafo 和一个
//// 关联的 csg 操作。一组 CSGPartT 对象可以则
//// 被解释为一个 model 并在各种上下文（contexts）中使用。它可以被组装
//// 使用 CGAL 或 OpenVDB，使用 OpenCSG 渲染或提供给光线追踪器以
//// 根据支持的 CSG 类型处理其各个部分...
//
//// 一些简单的模板化接口函数在此提供，以及一个默认
//// CSGPart 类，该类实现了作为 CSGPartT 对象使用的必要手段。

//// 支持的 CSG 操作类型
enum class CSGType { Union, Difference, Intersection };

//// 一个 CSG part 可以指示处理程序将子结果推入栈中，直到一个
//// 带有弹出指令的新 csg part 出现。这可以用于实现
//// 由一组 csg parts 表示的 CSG 表达式中的括号。
//// 一个 CSG part 不能包含另一个 CSG 集合，仅一个 mesh，这就是为什么
// 在数据定义中做这种栈操作比递归更容易。
//// CSGStackOp::Continue 表示无需栈操作。
//// 当一个 CSG part 包含 Push 指令时，它包含的 CSG
// 操作涉及直到最近的带有 Pop 指令的 part 的整个集合。
//// 例如：
//// {
//// CUBE1: { mesh: cube, op: Union, 栈操作: Continue },
//// CUBE2: { mesh: cube, op: 差异, 栈操作: Push},
//// CUBE3: { mesh: cube, op: Union, 栈操作: Pop}
//// }
//// 是一组 csg parts 表示表达式 CUBE1 - (CUBE2 + CUBE3)
enum class CSGStackOp { Push, Continue, Pop };

//// 获取 part 的 CSG 操作。可以被任何类型重写
template<class CSGPartT> CSGType get_operation(const CSGPartT &part)
{
    return part.operation;
}

//// 获取 CSG part 所需的栈操作。
template<class CSGPartT> CSGStackOp get_stack_operation(const CSGPartT &part)
{
    return part.stack_operation;
}

//// 获取 part 的网格。可以被任何类型重写
template<class CSGPartT>
const indexed_triangle_set *get_mesh(const CSGPartT &part)
{
    return part.its_ptr.get();
}

//// 获取与 CSGPartT 对象中的网格关联的变换。
//// 可以被任何类型重写。
template<class CSGPartT>
Transform3f get_transform(const CSGPartT &part)
{
    return part.trafo;
}

//// 默认实现
struct CSGPart {
    AnyPtr<const indexed_triangle_set> its_ptr;
    Transform3f trafo;
    CSGType operation;
    CSGStackOp stack_operation;
    std::string name;

    CSGPart(AnyPtr<const indexed_triangle_set> ptr = {},
            CSGType                            op  = CSGType::Union,
            const Transform3f                 &tr  = Transform3f::Identity())
        : its_ptr{std::move(ptr)}
        , operation{op}
        , stack_operation{CSGStackOp::Continue}
        , trafo{tr}
    {}
};

//// Prusa
//// 检查集合中是否只有正 parts（Union）。
template<class Cont> bool is_all_positive(const Cont &csgmesh)
{
    bool is_all_pos =
        std::all_of(csgmesh.begin(),
                    csgmesh.end(),
                    [](auto &part) {
                        return csg::get_operation(part) == csg::CSGType::Union;
                    });

    return is_all_pos;
}

//// Prusa
//// 合并集合中所有正 parts 为一个三角网格，不执行任何布尔运算。
template<class Cont>
indexed_triangle_set csgmesh_merge_positive_parts(const Cont &csgmesh)
{
    indexed_triangle_set m;
    for (auto &csgpart : csgmesh) {
        auto op = csg::get_operation(csgpart);
        const indexed_triangle_set * pmesh = csg::get_mesh(csgpart);
        if (pmesh && op == csg::CSGType::Union) {
            indexed_triangle_set mcpy = *pmesh;
            its_transform(mcpy, csg::get_transform(csgpart), true);
            its_merge(m, mcpy);
        }
    }

    return m;
}

}} // namespace Slic3r::csg

#endif // CSGMESH_HPP
