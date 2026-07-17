#ifndef SLA_JOBCONTROLLER_HPP
#define SLA_JOBCONTROLLER_HPP

#include <functional>
#include <string>

namespace Slic3r { namespace sla {

/// 支撑计算的控制结构。包含状态指示回调函数和停止条件谓词。
struct JobController
{
    using StatusFn = std::function<void(unsigned, const std::string&)>;
    using StopCond = std::function<bool(void)>;
    using CancelFn = std::function<void(void)>;
    
    // 这将向前端发送计算状态信号
    StatusFn statuscb = [](unsigned, const std::string&){};

    // 如果计算应中止，则返回 true。
    StopCond stopcondition = [](){ return false; };

    // 类似于取消回调。这应检查停止条件，
    // 如果为 true，则抛出适当的异常。（TriangleMeshSlicer 需要这个）
    // 将其视为硬中止。stopcondition 允许算法自行终止
    CancelFn cancelfn = [](){};
};

}} // namespace Slic3r::sla

#endif // JOBCONTROLLER_HPP
