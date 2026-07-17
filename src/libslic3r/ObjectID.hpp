#ifndef slic3r_ObjectID_hpp_
#define slic3r_ObjectID_hpp_

#include <cereal/access.hpp>
#include <cereal/types/base_class.hpp>

namespace Slic3r {

namespace UndoRedo {
	class StackImpl;
};

// 跨应用程序的可变对象的唯一标识符。
// 用于将前端（UI）与后端（BackgroundSlicingProcess / Print / PrintObject）同步
// （针对 Model、ModelObject、ModelVolume、ModelInstance 或 ModelMaterial 类）
// 以及将对象序列化/反序列化到撤销/重做栈上。
// 有效的 ID 严格为正（非零）。
// 它被声明为一个对象，因为某些编译器（特别是 msvcc）认为 typedef size_t 等价于 size_t
// 用于参数重载。
class ObjectID
{
public:
	ObjectID(size_t id) : id(id) {}
	// 默认构造函数构造一个无效的 ObjectID。
	ObjectID() : id(0) {}

	bool operator==(const ObjectID &rhs) const { return this->id == rhs.id; }
	bool operator!=(const ObjectID &rhs) const { return this->id != rhs.id; }
	bool operator< (const ObjectID &rhs) const { return this->id <  rhs.id; }
	bool operator> (const ObjectID &rhs) const { return this->id >  rhs.id; }
	bool operator<=(const ObjectID &rhs) const { return this->id <= rhs.id; }
	bool operator>=(const ObjectID &rhs) const { return this->id >= rhs.id; }

    bool valid() const { return id != 0; }
    bool invalid() const { return id == 0; }

	size_t	id;

private:
	friend class cereal::access;
	template<class Archive> void serialize(Archive &ar) { ar(id); }
};

// Model、ModelObject、ModelVolume、ModelInstance 或 ModelMaterial 的基类，提供唯一 ID
// 以同步前端（UI）与后端（BackgroundSlicingProcess / Print / PrintObject）。
// 同时也是 Print、PrintObject、SLAPrint、SLAPrintObject 的基类，提供唯一 ID，
// 用于在处理后端警告时，通过 UI 的通知中心将 Model / ModelObject 与对应的 Print / PrintObject 对象匹配。
// 注意！s_last_id 计数器不是线程安全的，因此预期 ObjectBase 派生实例
// 仅从主线程实例化。
class ObjectBase
{
public:
    using Timestamp = uint64_t;

    ObjectID     		id() const { return m_id; }
    // 返回此对象的可选时间戳。
    // 如果返回的时间戳非零，则序列化框架将仅在时间戳与撤销/重做栈顶部的对象时间戳不同时，
    // 才将此对象保存到撤销/重做栈上。
    virtual Timestamp	timestamp() const { return 0; }

protected:
    // 构造函数仅由派生类调用。
    // 默认构造函数分配唯一 ID。
    ObjectBase() : m_id(generate_new_id()) {}
    // 带有忽略的 int 参数的构造函数，分配无效 ID，将由从别处复制的现有 ID 替换。
    ObjectBase(int) : m_id(ObjectID(0)) {}

    ObjectBase(const ObjectID id) : m_id(id) {}
	// 类树将具有虚函数表和类型信息。
	virtual ~ObjectBase() = default;

    // Use with caution!
    void        set_new_unique_id() { m_id = generate_new_id(); }
    void        set_invalid_id()    { m_id = 0; }
    // Use with caution!
    void        copy_id(const ObjectBase &rhs) { m_id = rhs.id(); }

    // Override this method if a ObjectBase derived class owns other ObjectBase derived instances.
    virtual void assign_new_unique_ids_recursive() { this->set_new_unique_id(); }

private:
    ObjectID                m_id;

	static inline ObjectID  generate_new_id() { return ObjectID(++ s_last_id); }
    static size_t           s_last_id;
	
	friend ObjectID wipe_tower_object_id();
	friend ObjectID wipe_tower_instance_id();

	friend class cereal::access;
	friend class Slic3r::UndoRedo::StackImpl;
	template<class Archive> void serialize(Archive &ar) { ar(m_id); }
  	template<class Archive> static void load_and_construct(Archive & ar, cereal::construct<ObjectBase> &construct) { ObjectID id; ar(id); construct(id); }
};

class ObjectWithTimestamp : public ObjectBase
{
protected:
    // Constructors to be only called by derived classes.
    // Default constructor to assign a new timestamp unique to this object's history.
	ObjectWithTimestamp() = default;
    // 带有忽略的 int 参数的构造函数，分配无效 ID，将由从别处复制的现有 ID 替换。
    ObjectWithTimestamp(int) : ObjectBase(-1) {}
	// 类树将具有虚函数表和类型信息。
	virtual ~ObjectWithTimestamp() = default;

    // 时间戳唯一标识派生类数据的内容，因此如果内容数据被复制，复制时间戳是有意义的。
    void                copy_timestamp(const ObjectWithTimestamp& rhs) { m_timestamp = rhs.m_timestamp; }

public:
    // 返回此对象的可选时间戳。
    // 如果返回的时间戳非零，则序列化框架将仅在时间戳与撤销/重做栈顶部的对象时间戳不同时，
    // 才将此对象保存到撤销/重做栈上。
    Timestamp	        timestamp() const throw() override { return m_timestamp; }
    bool 				timestamp_matches(const ObjectWithTimestamp &rhs) const throw() { return m_timestamp == rhs.m_timestamp; }
    bool 				object_id_and_timestamp_match(const ObjectWithTimestamp &rhs) const throw() { return this->id() == rhs.id() && m_timestamp == rhs.m_timestamp; }
    void 				touch() { m_timestamp = ++ s_last_timestamp; }

private:
	// The first timestamp is non-zero, as zero timestamp means the timestamp is not reliable.
	Timestamp 			m_timestamp { 1 };
    static Timestamp    s_last_timestamp;
	
	friend class cereal::access;
	friend class Slic3r::UndoRedo::StackImpl;
	template<class Archive> void serialize(Archive &ar) { ar(m_timestamp); }
};

class CutObjectBase : public ObjectBase
{
    // check sum of CutParts in initial Object
    size_t m_check_sum{1};
    // connectors count
    size_t m_connectors_cnt{0};

public:
    // Default Constructor to assign an invalid ID
    CutObjectBase() : ObjectBase(-1) {}
    // 带有忽略的 int 参数的构造函数，分配无效 ID，将由从别处复制的现有 ID 替换。
    CutObjectBase(int) : ObjectBase(-1) {}
    // Constructor to initialize full information from 3mf
    CutObjectBase(ObjectID id, size_t check_sum, size_t connectors_cnt) : ObjectBase(id), m_check_sum(check_sum), m_connectors_cnt(connectors_cnt) {}
    // 类树将具有虚函数表和类型信息。
    virtual ~CutObjectBase() = default;

    bool operator<(const CutObjectBase &other) const { return other.id() > this->id(); }
    bool operator==(const CutObjectBase &other) const { return other.id() == this->id(); }

    void copy(const CutObjectBase &rhs)
    {
        this->copy_id(rhs);
        this->m_check_sum      = rhs.check_sum();
        this->m_connectors_cnt = rhs.connectors_cnt();
    }
    CutObjectBase &operator=(const CutObjectBase &other)
    {
        this->copy(other);
        return *this;
    }

    void invalidate()
    {
        set_invalid_id();
        m_check_sum      = 1;
        m_connectors_cnt = 0;
    }

    void init() { this->set_new_unique_id(); }
    bool has_same_id(const CutObjectBase &rhs) { return this->id() == rhs.id(); }
    bool is_equal(const CutObjectBase &rhs) { return this->id() == rhs.id() && this->check_sum() == rhs.check_sum() && this->connectors_cnt() == rhs.connectors_cnt(); }

    size_t check_sum() const { return m_check_sum; }
    void   set_check_sum(size_t cs) { m_check_sum = cs; }
    void   increase_check_sum(size_t cnt) { m_check_sum += cnt; }

    size_t connectors_cnt() const { return m_connectors_cnt; }
    void   increase_connectors_cnt(size_t connectors_cnt) { m_connectors_cnt += connectors_cnt; }

private:
    friend class cereal::access;
    template<class Archive> void serialize(Archive &ar)
    {
        ar(cereal::base_class<ObjectBase>(this));
        ar(m_check_sum, m_connectors_cnt);
    }
};


// Unique object / instance ID for the wipe tower.
extern ObjectID wipe_tower_object_id();
extern ObjectID wipe_tower_instance_id();

} // namespace Slic3r

#endif /* slic3r_ObjectID_hpp_ */
