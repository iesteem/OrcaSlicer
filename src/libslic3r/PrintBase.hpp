#ifndef slic3r_PrintBase_hpp_
#define slic3r_PrintBase_hpp_

#include "libslic3r.h"
#include <set>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

#include "ObjectID.hpp"
#include "Model.hpp"
#include "PlaceholderParser.hpp"
#include "PrintConfig.hpp"

namespace Slic3r {

enum StringExceptionType {
    STRING_EXCEPT_NOT_DEFINED                   = 0,
    STRING_EXCEPT_FILAMENT_NOT_MATCH_BED_TYPE   = 1,
    STRING_EXCEPT_FILAMENTS_DIFFERENT_TEMP      = 2,
    STRING_EXCEPT_OBJECT_COLLISION_IN_SEQ_PRINT = 3,
    STRING_EXCEPT_OBJECT_COLLISION_IN_LAYER_PRINT = 4,
    STRING_EXCEPT_LAYER_HEIGHT_EXCEEDS_LIMIT = 5,
    STRING_EXCEPT_COUNT
};

// BBS: error with object
struct StringObjectException
{
    std::string string;
    ObjectBase const *object = nullptr;
    std::string opt_key;
    StringExceptionType         type;   // 警告类型提示
    std::vector<std::string>    params; // 警告参数提示
};

class CanceledException : public std::exception
{
public:
   const char* what() const throw() { return "Background processing has been canceled"; }
};

class PrintStateBase {
public:
    enum State {
        INVALID,
        STARTED,
        DONE,
    };

    enum class WarningLevel {
        NON_CRITICAL,
        CRITICAL
    };

    enum SlicingNotificationType
    {
        SlicingDefaultNotification = 0,    //normal status update, called by set_status
        SlicingReplaceInitEmptyLayers,
        SlicingNeedSupportOn,
        SlicingEmptyGcodeLayers,
        SlicingGcodeOverlap
    };

    typedef size_t TimeStamp;

    // 每次步骤更改状态时，都会分配一个新的唯一时间戳。
    struct StateWithTimeStamp
    {
        StateWithTimeStamp() : state(INVALID), timestamp(0) {}
        State       state;
        TimeStamp   timestamp;
    };

    struct Warning
    {
    	// 关键警告将在G-code导出时以模态对话框显示，以便用户不会错过它们。
        WarningLevel    level;
        // 如果警告不是当前的，则它处于未知状态。它可能有效也可能无效。
        // 当前警告在其里程碑失效时将变为非当前状态。
        // 非当前警告要么变为当前状态，要么在里程碑结束时被移除。
        bool 			current;
        // 要显示给用户的消息，UTF8编码，本地化。
        std::string     message;
        // 如果 message_id == 0，则期望消息能唯一标识警告。
        // 否则 message_id 标识消息。例如，如果消息包含变化数字，则
        // 它本身无法标识消息类型。
        int 			message_id;
    };

    struct StateWithWarnings : public StateWithTimeStamp
    {
    	void 	mark_warnings_non_current() { for (auto &w : warnings) w.current = false; }
        std::vector<Warning>    warnings;
    };

protected:
    //FIXME 最后一个时间戳在Print和SLAPrint之间共享，
    // 如果多个Print或SLAPrint实例并行执行，g_last_timestamp的修改
    // 不会被同步！
    static size_t g_last_timestamp;
};

// 将在PrintStep或PrintObjectStep枚举上实例化。
template <class StepType, size_t COUNT>
class PrintState : public PrintStateBase
{
public:
    PrintState() {}

    StateWithTimeStamp state_with_timestamp(StepType step, std::mutex &mtx) const {
        std::scoped_lock<std::mutex> lock(mtx);
        StateWithTimeStamp state = m_state[step];
        return state;
    }

    StateWithWarnings state_with_warnings(StepType step, std::mutex &mtx) const {
        std::scoped_lock<std::mutex> lock(mtx);
        StateWithWarnings state = m_state[step];
        return state;
    }

    bool is_started(StepType step, std::mutex &mtx) const {
        return this->state_with_timestamp(step, mtx).state == STARTED;
    }

    bool is_done(StepType step, std::mutex &mtx) const {
        return this->state_with_timestamp(step, mtx).state == DONE;
    }

    StateWithTimeStamp state_with_timestamp_unguarded(StepType step) const {
        return m_state[step];
    }

    bool is_started_unguarded(StepType step) const {
        return this->state_with_timestamp_unguarded(step).state == STARTED;
    }

    bool is_done_unguarded(StepType step) const {
        return this->state_with_timestamp_unguarded(step).state == DONE;
    }

    // 将步骤标记为已启动。在Print/PrintObject/PrintRegion对象被
    // UI线程修改时，在互斥锁上阻塞。
    // 这是必要的，以阻塞直到Print::apply()更新其状态，这可能会
    // 影响正在进入的处理步骤。
    template<typename ThrowIfCanceled>
    bool set_started(StepType step, std::mutex &mtx, ThrowIfCanceled throw_if_canceled) {
        std::scoped_lock<std::mutex> lock(mtx);
        // 如果已取消，在更改步骤状态之前抛出异常。
        throw_if_canceled();
#ifndef NDEBUG
// 以下测试在后台处理线程被throw_if_canceled()停止后不一定有效，
// 因为CanceledException没有被Print或PrintObject捕获
// 来更新m_step_active或m_state[...].state。
// 只要调用者一致地调用set_started()/set_done()/
// active_step_add_warning()，这应该不是问题。从健壮性的角度来看，
// 捕获CanceledException并进行更新会更好。从性能的角度来看，
// 当前的实现是最优的。
//
//        assert(m_step_active == -1);
//        for (int i = 0; i < int(COUNT); ++ i)
//            assert(m_state[i].state != STARTED);
#endif // NDEBUG
        if (m_state[step].state == DONE)
            return false;
        PrintStateBase::StateWithWarnings &state = m_state[step];
        state.state = STARTED;
        state.timestamp = ++ g_last_timestamp;
        state.mark_warnings_non_current();
        m_step_active = static_cast<int>(step);
        return true;
    }

    // 将步骤标记为已完成。在Print/PrintObject/PrintRegion对象被
    // UI线程修改时，在互斥锁上阻塞。
    // 返回值：
    // 		时间戳，表示此步骤进入DONE状态的时间。
    // 		bool，表示UI是否必须更新此步骤的切片警告。
	template<typename ThrowIfCanceled>
	std::pair<TimeStamp, bool> set_done(StepType step, std::mutex &mtx, ThrowIfCanceled throw_if_canceled) {
        std::scoped_lock<std::mutex> lock(mtx);
        // 如果已取消，在更改步骤状态之前抛出异常。
        throw_if_canceled();
        assert(m_state[step].state == STARTED);
        assert(m_step_active == static_cast<int>(step));
        PrintStateBase::StateWithWarnings &state = m_state[step];
        state.state = DONE;
        state.timestamp = ++ g_last_timestamp;
        m_step_active = -1;
        // 移除所有非当前警告。
    	auto it = std::remove_if(state.warnings.begin(), state.warnings.end(), [](const auto &w) { return ! w.current; });
    	bool update_warning_ui = false;
        if (it != state.warnings.end()) {
        	state.warnings.erase(it, state.warnings.end());
        	update_warning_ui = true;
        }
        return std::make_pair(state.timestamp, update_warning_ui);
    }

    // 使步骤无效。
    // 此时应锁定PrintBase::m_state_mutex，保护对m_state的访问。
    // 如果步骤已经进入或完成，通过调用取消回调来取消后台处理。
    template<typename CancelationCallback>
    bool invalidate(StepType step, CancelationCallback cancel) {
        bool invalidated = m_state[step].state != INVALID;
        if (invalidated) {
#if 0
            if (mtx.state != mtx.HELD) {
                printf("Not held!\n");
            }
#endif
            PrintStateBase::StateWithWarnings &state = m_state[step];
            state.state = INVALID;
            state.timestamp = ++ g_last_timestamp;
            // 提升互斥锁，以便以下cancel()回调可以取消
            // 后台处理。
            // 内部地，cancel()回调应解锁PrintBase::m_status_mutex以让
            // 工作线程继续执行。
            cancel();
            // 现在工作线程应已停止，因此它无法写入警告字段。
            // 修改它是安全的。
            state.mark_warnings_non_current();
            m_step_active = -1;
        }
        return invalidated;
    }

    template<typename CancelationCallback, typename StepTypeIterator>
    bool invalidate_multiple(StepTypeIterator step_begin, StepTypeIterator step_end, CancelationCallback cancel) {
        bool invalidated = false;
        for (StepTypeIterator it = step_begin; it != step_end; ++ it) {
            StateWithTimeStamp &state = m_state[*it];
            if (state.state != INVALID) {
                invalidated = true;
                state.state = INVALID;
                state.timestamp = ++ g_last_timestamp;
            }
        }
        if (invalidated) {
#if 0
            if (mtx.state != mtx.HELD) {
                printf("Not held!\n");
            }
#endif
            // 提升互斥锁，以便以下cancel()回调可以取消
            // 后台处理。
            // 内部地，cancel()回调应解锁PrintBase::m_status_mutex以让
            // 工作线程继续执行。
            cancel();
            // 现在工作线程应已停止，因此它无法写入警告字段。
            // 修改警告是安全的。
            for (StepTypeIterator it = step_begin; it != step_end; ++ it)
                m_state[*it].mark_warnings_non_current();
            m_step_active = -1;
        }
        return invalidated;
    }

    // 使所有步骤无效。
    // 此时应锁定PrintBase::m_state_mutex，保护对m_state的访问。
    // 如果任何步骤已经进入或完成，通过调用取消回调来取消后台处理。
    template<typename CancelationCallback>
    bool invalidate_all(CancelationCallback cancel) {
        bool invalidated = false;
        for (size_t i = 0; i < COUNT; ++ i) {
            StateWithTimeStamp &state = m_state[i];
            if (state.state != INVALID) {
                invalidated = true;
                state.state = INVALID;
                state.timestamp = ++ g_last_timestamp;
            }
        }
        if (invalidated) {
            cancel();
            // 现在工作线程应已停止，因此它无法写入警告字段。
            // 修改警告是安全的。
            for (size_t i = 0; i < COUNT; ++ i)
                m_state[i].mark_warnings_non_current();
            m_step_active = -1;
        }
        return invalidated;
    }

    // 使用当前里程碑的新警告更新警告列表。
    // 该警告可能已存在于列表中，标记为当前或非当前。
    // 如果已存在，将其标记为当前。
    // 返回值：
    // 		当前里程碑（StepType）。
    // 		bool 表示UI是否需要更新。
    std::pair<StepType, bool> active_step_add_warning(PrintStateBase::WarningLevel warning_level, const std::string &message, int message_id, std::mutex &mtx)
    {
        std::scoped_lock<std::mutex> lock(mtx);
        assert(m_step_active != -1);
        StateWithWarnings &state = m_state[m_step_active];
        assert(state.state == STARTED);
        std::pair<StepType, bool> retval(static_cast<StepType>(m_step_active), true);
        // 是否存在相同级别和消息或message_id的警告？
        auto it = (message_id == 0) ?
            std::find_if(state.warnings.begin(), state.warnings.end(), [&message](const auto &w) { return w.message_id == 0 && w.message == message; }) :
            std::find_if(state.warnings.begin(), state.warnings.end(), [message_id](const auto& w) { return w.message_id == message_id; });
        if (it == state.warnings.end())
            // 否，创建新警告并更新UI。
            state.warnings.emplace_back(PrintStateBase::Warning{ warning_level, true, message, message_id });
        else if (it->message != message || it->level != warning_level) {
            // 是，但需要更新。
            it->message = message;
            it->level 	= warning_level;
            it->current = true;
        } else if (it->current)
            // 是，且是当前的。不更新UI。
            retval.second = false;
        else
            // 是，但不是当前的。将其标记为当前。
            it->current = true;
        return retval;
    }

private:
    StateWithWarnings   m_state[COUNT];
    // 活动的StepType类，如果无活动则为-1。
    // 如果后台处理被取消，m_step_active可能不会重置
    // 为-1，请参见this->set_started()中的注释。
    int                 m_step_active = -1;
};

class PrintBase;

class PrintObjectBase : public ObjectBase
{
public:
    const ModelObject*      model_object() const    { return m_model_object; }
    ModelObject*            model_object()          { return m_model_object; }

protected:
    PrintObjectBase(ModelObject *model_object) : m_model_object(model_object) {}
    virtual ~PrintObjectBase() {}
    // 在此声明以允许通过友元关系从PrintBase访问。
	static std::mutex&                  state_mutex(PrintBase *print);
	static std::function<void()>        cancel_callback(PrintBase *print);
	// 通知UI关于此PrintObjectBase上里程碑"step"的新警告。
	// UI将通过调用在print上注册的状态回调来通知。
	// 如果没有注册状态回调，消息将打印到控制台。
    void status_update_warnings(PrintBase *print, int step, PrintStateBase::WarningLevel warning_level,
        const std::string &message, PrintStateBase::SlicingNotificationType message_id = PrintStateBase::SlicingDefaultNotification);
    void emptylayer_update_msg(PrintBase* print, int type, const std::string& message, bool overwrite);

    ModelObject                  *m_model_object;
};

// 围绕私有PrintBase.throw_if_canceled()的包装器，以便取消对象可以由
// PrintBase派生对象传递给PrintBase的非友元。
class PrintTryCancel
{
public:
    // 调用 print.throw_if_canceled()。
    void operator()();
private:
    friend PrintBase;
    PrintTryCancel() = delete;
    PrintTryCancel(const PrintBase *print) : m_print(print) {}
    const PrintBase *m_print;
};

/**
 * @brief 打印涉及切片和导出设备相关指令。
 *
 * 每种技术对于切片、支撑结构和输出打印指令都可能有不同的要求。
 * 然而，处理流程大致相同：
 *      切片 -> 转换为指令 -> 发送到打印机
 *
 * PrintBase类将为不同的技术抽象此流程。
 *
 */
class PrintBase : public ObjectBase
{
public:
	PrintBase() : m_placeholder_parser(&m_full_print_config) { this->restart(); }
    inline virtual ~PrintBase() {}

    virtual PrinterTechnology technology() const noexcept = 0;

    // 重置打印状态，包括Model/ModelObject层次结构的副本。
    virtual void            clear() = 0;
    // 打印在clear()后、对空模型应用apply()后、或对没有可打印对象的模型应用apply()后（所有对象都在打印体积之外）为空。
    virtual bool            empty() const = 0;
    // 现有PrintObject ID的列表，用于移除不存在的ID的通知。
    virtual std::vector<ObjectID> print_object_ids() const = 0;

    // 验证打印，如果有效则返回空字符串，如果process()无法（或不应该）启动则返回错误。
    //BBS: add more paremeters to validate
    virtual StringObjectException validate(StringObjectException *warning = nullptr, Polygons* collison_polygons = nullptr, std::vector<std::pair<Polygon, float>>* height_polygons = nullptr) const { return {}; }

    enum ApplyStatus {
        // 在Print::apply()调用后无变化。
        APPLY_STATUS_UNCHANGED,
        // 一些Print/PrintObject/PrintObjectInstance数据被更改，
        // 但没有结果被失效（仅影响尚未计算的结果的数据被更改）。
        APPLY_STATUS_CHANGED,
        // 一些数据被更改，这反过来使得已计算的步骤失效。
        APPLY_STATUS_INVALIDATED,
    };
    virtual ApplyStatus     apply(const Model &model, DynamicPrintConfig config) = 0;
    const Model&            model() const { return m_model; }

    struct TaskParams {
		TaskParams() : single_model_object(0), single_model_instance_only(false), to_object_step(-1), to_print_step(-1) {}
        // 如果非空，将处理限制为此ModelObject。
        ObjectID                single_model_object;
		// If set, only process single_model_object. Otherwise process everything, but single_model_object first.
		bool					single_model_instance_only;
        // 如果非负，在后续对象步骤处停止处理。
        int                     to_object_step;
        // 如果非负，在后续打印步骤处停止处理。
        int                     to_print_step;
    };
    // 在调用apply()函数后，调用set_task()来限制由process()处理的任务。
    virtual void            set_task(const TaskParams &params) {}
    // 执行计算。这是唯一在工作线程上调用的方法。
    virtual void            process(long long *time_cost_with_cache = nullptr, bool use_cache = false) = 0;
    virtual int             export_cached_data(const std::string& dir_path, bool with_space=false) { return 0;}
    virtual int            load_cached_data(const std::string& directory) { return 0;}
    // 在process()完成后清理，无论是成功、出错还是被取消。
    // 由于set_task()对Print/PrintObject数据的调整将在此处恢复。
    virtual void            finalize() {}

    struct SlicingStatus {
        SlicingStatus(int percent, const std::string &text, unsigned int flags = 0, int warning_step = -1,
            PrintStateBase::SlicingNotificationType  msg_type = PrintStateBase::SlicingDefaultNotification, PrintStateBase::WarningLevel warning_level = PrintStateBase::WarningLevel::NON_CRITICAL) :
            percent(percent), text(text), flags(flags), warning_step(warning_step), message_type(msg_type), warning_level(warning_level)
        {
        }
        SlicingStatus(const PrintBase &print, int warning_step, const std::string& text,
            PrintStateBase::SlicingNotificationType  msg_type = PrintStateBase::SlicingDefaultNotification, PrintStateBase::WarningLevel warning_level = PrintStateBase::WarningLevel::NON_CRITICAL) :
            flags(UPDATE_PRINT_STEP_WARNINGS), warning_object_id(print.id()), text(text), warning_step(warning_step), message_type(msg_type), warning_level(warning_level)
        {
        }
        SlicingStatus(const PrintObjectBase &print_object, int warning_step, const std::string& text,
            PrintStateBase::SlicingNotificationType  msg_type = PrintStateBase::SlicingDefaultNotification, PrintStateBase::WarningLevel warning_level = PrintStateBase::WarningLevel::NON_CRITICAL) :
            flags(UPDATE_PRINT_OBJECT_STEP_WARNINGS), warning_object_id(print_object.id()), text(text), warning_step(warning_step), message_type(msg_type), warning_level(warning_level)
        {
        }
        int             percent { -1 };
        std::string     text;
        // 标志位图。
        enum FlagBits {
            DEFAULT                             = 0,
            RELOAD_SCENE                        = 1 << 1,
            RELOAD_SLA_SUPPORT_POINTS           = 1 << 2,
            RELOAD_SLA_PREVIEW                  = 1 << 3,
            // UPDATE_PRINT_STEP_WARNINGS 与 UPDATE_PRINT_OBJECT_STEP_WARNINGS 互斥。
            UPDATE_PRINT_STEP_WARNINGS          = 1 << 4,
            UPDATE_PRINT_OBJECT_STEP_WARNINGS   = 1 << 5
        };
        // FlagBits 的位图
        unsigned int    flags;
        // 根据标志设置为Print或PrintObject的ObjectID
        // （是设置了UPDATE_PRINT_STEP_WARNINGS还是UPDATE_PRINT_OBJECT_STEP_WARNINGS）。
        ObjectID        warning_object_id;
        // 正在为哪个Print或PrintObject步骤发布新警告？
        int             warning_step { -1 };

        PrintStateBase::SlicingNotificationType  message_type {PrintStateBase::SlicingDefaultNotification};
        PrintStateBase::WarningLevel  warning_level {PrintStateBase::WarningLevel::NON_CRITICAL};
    };
    typedef std::function<void(const SlicingStatus&)>  status_callback_type;
    // 默认状态控制台输出，格式为百分比 => 消息。
    void                    set_status_default() { m_status_callback = nullptr; }
    // 没有任何状态输出或回调，主要用于自动测试。
    void                    set_status_silent() { m_status_callback = [](const SlicingStatus&){}; }
    // 注册自定义状态回调。
    void                    set_status_callback(status_callback_type cb) { m_status_callback = cb; }
    // 调用注册的回调来更新状态，或打印默认消息。
    void                    set_status(int percent, const std::string &message, unsigned int flags = SlicingStatus::DEFAULT, int warning_step = -1) const;

    typedef std::function<void()>  cancel_callback_type;
    // 各种方法将调用此回调来停止后台处理（Print::process()调用）
    // 如果Print/PrintObject/PrintRegion实例的连续更改改变了
    // 已完成或正在运行的计算的状态。
    void                       set_cancel_callback(cancel_callback_type cancel_callback) { m_cancel_callback = cancel_callback; }
    // Has the calculation been canceled?
	enum CancelStatus {
		// 未取消，后台处理应运行。
		NOT_CANCELED = 0,
		// Canceled by user from the user interface (user pressed the "Cancel" button or user closed the application).
		CANCELED_BY_USER = 1,
		// Canceled internally from Print::apply() through the Print/PrintObject::invalidate_step() or ::invalidate_all_steps().
		CANCELED_INTERNAL = 2
	};
    CancelStatus               cancel_status() const { return m_cancel_status.load(std::memory_order_acquire); }
    // Has the calculation been canceled?
	bool                       canceled() const { return m_cancel_status.load(std::memory_order_acquire) != NOT_CANCELED; }
    // Cancel the running computation. Stop execution of all the background threads.
	void                       cancel() { m_cancel_status = CANCELED_BY_USER; }
	void                       cancel_internal() { m_cancel_status = CANCELED_INTERNAL; }
    // Cancel the running computation. Stop execution of all the background threads.
	void                       restart() { m_cancel_status = NOT_CANCELED; }
    // Returns true if the last step was finished with success.
    virtual bool               finished() const = 0;

    const PlaceholderParser&   placeholder_parser() const { return m_placeholder_parser; }
    const DynamicPrintConfig&  full_print_config() const { return m_full_print_config; }

    virtual std::string        output_filename(const std::string &filename_base = std::string()) const = 0;
    // 如果设置了filename_base，它将被用作模板处理的输入。在这种情况下，路径应为目录（可能为空）。
    // 如果filename_set为空，则路径可以是文件或目录。如果是文件，则宏将不会被处理。
    std::string                output_filepath(const std::string &path, const std::string &filename_base = std::string()) const;

    //BBS: get/set plate id
    int get_plate_index() const { return m_plate_index; }
    void set_plate_index(int index) { m_plate_index = index; }
    bool get_no_check_flag() const { return m_no_check; }
    void set_no_check_flag(bool no_check) { m_no_check = no_check; }

    //SoftFever plate name
    std::string get_plate_name() const { return m_plate_name; }
    void set_plate_name(const std::string& name) { m_plate_name = name; }
protected:
	friend class PrintObjectBase;
    friend class BackgroundSlicingProcess;

    std::mutex&            state_mutex() const { return m_state_mutex; }
    std::function<void()>  cancel_callback() { return m_cancel_callback; }
	void				   call_cancel_callback() { m_cancel_callback(); }
	// 通知UI关于此PrintBase上里程碑"step"的新警告。
	// UI将通过调用状态回调来通知。
	// 如果没有注册状态回调，消息将打印到控制台。
    void 				   status_update_warnings(int step, PrintStateBase::WarningLevel warning_level,
        const std::string &message, const PrintObjectBase* print_object = nullptr, PrintStateBase::SlicingNotificationType message_id = PrintStateBase::SlicingDefaultNotification);
    //BBS: add api to update printobject's warnings
	void                   status_update_warnings(int step, PrintStateBase::WarningLevel warning_level,
	    const std::string& message, PrintObjectBase &object, PrintStateBase::SlicingNotificationType message_id = PrintStateBase::SlicingDefaultNotification);

    // 如果请求停止后台处理，则抛出CanceledException。
    // 由工作线程及其子线程（主要在TBB线程池上启动）定期调用。
    void                   throw_if_canceled() const { if (m_cancel_status.load(std::memory_order_acquire)) throw CanceledException(); }
    // 围绕this->throw_if_canceled()的包装器，以便throw_if_canceled()可以传递给函数而无需公开throw_if_canceled()。
    PrintTryCancel         make_try_cancel() const { return PrintTryCancel(this); }

    // 由this->output_filename()调用，格式字符串从配置层提取。
    std::string            output_filename(const std::string &format, const std::string &default_ext, const std::string &filename_base, const DynamicConfig *config_override = nullptr) const;
    // 从当前可打印的ModelObjects更新"scale"、"input_filename"、"input_filename_base"占位符。
    void                   update_object_placeholders(DynamicConfig &config, const std::string &default_ext) const;

	Model                                   m_model;
	DynamicPrintConfig						m_full_print_config;
    PlaceholderParser                       m_placeholder_parser;

    //BBS: add plate id into print base
    int m_plate_index{ 0 };
    bool m_no_check = false;

    // SoftFever: current plate name
    std::string m_plate_name;

    // 定期触发的回调，用于更新UI线程的状态。
    status_callback_type                    m_status_callback;

private:
    std::atomic<CancelStatus>               m_cancel_status;

    // 触发的回调，用于在状态更新前停止后台处理。
    cancel_callback_type                    m_cancel_callback = [](){};

    // 用于工作线程与UI线程同步的互斥锁：
    // 互斥锁将用于保护工作线程，防止在影响阶段的数据被修改时进入该阶段。
    mutable std::mutex                      m_state_mutex;

    friend PrintTryCancel;
};

template<typename PrintStepEnum, const size_t COUNT>
class PrintBaseWithState : public PrintBase
{
public:
    bool            is_step_done(PrintStepEnum step) const { return m_state.is_done(step, this->state_mutex()); }
	PrintStateBase::StateWithTimeStamp step_state_with_timestamp(PrintStepEnum step) const { return m_state.state_with_timestamp(step, this->state_mutex()); }
    PrintStateBase::StateWithWarnings  step_state_with_warnings(PrintStepEnum step) const { return m_state.state_with_warnings(step, this->state_mutex()); }
    // 向活动Print步骤添加切片警告并发送状态通知。
    // 此方法可在this->set_started()和this->set_done()之间多次调用。
    void            active_step_add_warning(PrintStateBase::WarningLevel warning_level, const std::string &message,
                            PrintStateBase::SlicingNotificationType message_id = PrintStateBase::SlicingDefaultNotification)
    {
        std::pair<PrintStepEnum, bool> active_step = m_state.active_step_add_warning(warning_level, message, (int)message_id, this->state_mutex());
        if (active_step.second)
            // 更新UI。
            this->status_update_warnings(static_cast<int>(active_step.first), warning_level, message, nullptr, message_id);
    }
protected:
    bool            set_started(PrintStepEnum step) { return m_state.set_started(step, this->state_mutex(), [this](){ this->throw_if_canceled(); }); }
	PrintStateBase::TimeStamp set_done(PrintStepEnum step) {
		std::pair<PrintStateBase::TimeStamp, bool> status = m_state.set_done(step, this->state_mutex(), [this](){ this->throw_if_canceled(); });
        if (status.second)
            this->status_update_warnings(static_cast<int>(step), PrintStateBase::WarningLevel::NON_CRITICAL, std::string());
        return status.first;
	}
    bool            invalidate_step(PrintStepEnum step)
		{ return m_state.invalidate(step, this->cancel_callback()); }
    template<typename StepTypeIterator>
    bool            invalidate_steps(StepTypeIterator step_begin, StepTypeIterator step_end)
        { return m_state.invalidate_multiple(step_begin, step_end, this->cancel_callback()); }
    bool            invalidate_steps(std::initializer_list<PrintStepEnum> il)
        { return m_state.invalidate_multiple(il.begin(), il.end(), this->cancel_callback()); }
    bool            invalidate_all_steps()
        { return m_state.invalidate_all(this->cancel_callback()); }

	bool            is_step_started_unguarded(PrintStepEnum step) const { return m_state.is_started_unguarded(step); }
	bool            is_step_done_unguarded(PrintStepEnum step) const { return m_state.is_done_unguarded(step); }


private:
    PrintState<PrintStepEnum, COUNT> m_state;
};

template<typename PrintType, typename PrintObjectStepEnum, const size_t COUNT>
class PrintObjectBaseWithState : public PrintObjectBase
{
public:
    PrintType*       print()         { return m_print; }
    const PrintType* print() const   { return m_print; }

    typedef PrintState<PrintObjectStepEnum, COUNT> PrintObjectState;
    bool            is_step_done(PrintObjectStepEnum step) const { return m_state.is_done(step, PrintObjectBase::state_mutex(m_print)); }
    PrintStateBase::StateWithTimeStamp step_state_with_timestamp(PrintObjectStepEnum step) const { return m_state.state_with_timestamp(step, PrintObjectBase::state_mutex(m_print)); }
    PrintStateBase::StateWithWarnings  step_state_with_warnings(PrintObjectStepEnum step) const { return m_state.state_with_warnings(step, PrintObjectBase::state_mutex(m_print)); }

protected:
	PrintObjectBaseWithState(PrintType *print, ModelObject *model_object) : PrintObjectBase(model_object), m_print(print) {}

    bool            set_started(PrintObjectStepEnum step)
        { return m_state.set_started(step, PrintObjectBase::state_mutex(m_print), [this](){ this->throw_if_canceled(); }); }
	PrintStateBase::TimeStamp set_done(PrintObjectStepEnum step) {
		std::pair<PrintStateBase::TimeStamp, bool> status = m_state.set_done(step, PrintObjectBase::state_mutex(m_print), [this](){ this->throw_if_canceled(); });
        if (status.second)
            this->status_update_warnings(m_print, static_cast<int>(step), PrintStateBase::WarningLevel::NON_CRITICAL, std::string());
        return status.first;
	}

    bool            invalidate_step(PrintObjectStepEnum step)
        { return m_state.invalidate(step, PrintObjectBase::cancel_callback(m_print)); }
    template<typename StepTypeIterator>
    bool            invalidate_steps(StepTypeIterator step_begin, StepTypeIterator step_end)
        { return m_state.invalidate_multiple(step_begin, step_end, PrintObjectBase::cancel_callback(m_print)); }
    bool            invalidate_steps(std::initializer_list<PrintObjectStepEnum> il)
        { return m_state.invalidate_multiple(il.begin(), il.end(), PrintObjectBase::cancel_callback(m_print)); }
    bool            invalidate_all_steps()
        { return m_state.invalidate_all(PrintObjectBase::cancel_callback(m_print)); }
    bool            invalidate_all_steps_without_cancel()
        { return m_state.invalidate_all([](){}); }

    bool            is_step_started_unguarded(PrintObjectStepEnum step) const { return m_state.is_started_unguarded(step); }
    bool            is_step_done_unguarded(PrintObjectStepEnum step) const { return m_state.is_done_unguarded(step); }

    // 向活动PrintObject步骤添加切片警告并发送状态通知。
    // 此方法可在this->set_started()和this->set_done()之间多次调用。
    void            active_step_add_warning(PrintStateBase::WarningLevel warning_level, const std::string &message,
                        PrintStateBase::SlicingNotificationType message_id = PrintStateBase::SlicingDefaultNotification) {
    	std::pair<PrintObjectStepEnum, bool> active_step = m_state.active_step_add_warning(warning_level, message,(int) message_id, PrintObjectBase::state_mutex(m_print));
    	if (active_step.second)
    		this->status_update_warnings(m_print, static_cast<int>(active_step.first), warning_level, message, message_id);
    }

protected:
    // 如果请求停止后台处理，则抛出CanceledException。
    // 由工作线程及其子线程（主要在TBB线程池上启动）定期调用。
    void            throw_if_canceled() { if (m_print->canceled()) throw CanceledException(); }

    friend PrintType;
    PrintType                               *m_print;

private:
    PrintState<PrintObjectStepEnum, COUNT>   m_state;
};

} // namespace Slic3r

#endif /* slic3r_PrintBase_hpp_ */
