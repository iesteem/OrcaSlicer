#ifndef slic3r_SLAPrint_hpp_
#define slic3r_SLAPrint_hpp_

#include <cstdint>
#include <mutex>
#include "PrintBase.hpp"
#include "SLA/RasterBase.hpp"
#include "SLA/SupportTree.hpp"
#include "Execution/ExecutionTBB.hpp"
#include "Point.hpp"
#include "MTUtils.hpp"
#include "Zipper.hpp"

namespace Slic3r {

enum SLAPrintStep : unsigned int {
    slapsMergeSlicesAndEval,
    slapsRasterize,
	slapsCount
};

enum SLAPrintObjectStep : unsigned int {
    slaposHollowing,
    slaposDrillHoles,
	slaposObjectSlice,
	slaposSupportPoints,
	slaposSupportTree,
	slaposPad,
    slaposSliceSupports,
	slaposCount
};

class SLAPrint;
class GLCanvas;

using _SLAPrintObjectBase =
    PrintObjectBaseWithState<SLAPrint, SLAPrintObjectStep, slaposCount>;

// Layers according to quantized height levels. This will be consumed by
// the printer (rasterizer) in the SLAPrint class.
// using coord_t = int64_t;

enum SliceOrigin { soSupport, soModel };

class SLAPrintObject : public _SLAPrintObjectBase
{
private: // Prevents erroneous use by other classes.
    using Inherited = _SLAPrintObjectBase;

public:

    // I refuse to grantee copying (Tamas)
    SLAPrintObject(const SLAPrintObject&) = delete;
    SLAPrintObject& operator=(const SLAPrintObject&) = delete;

    const SLAPrintObjectConfig& config() const { return m_config; }
    const Transform3d&          trafo()  const { return m_trafo; }
    bool                        is_left_handed() const { return m_left_handed; }

    struct Instance {
        Instance(ObjectID inst_id, const Point &shft, float rot) : instance_id(inst_id), shift(shft), rotation(rot) {}
        bool operator==(const Instance &rhs) const { return this->instance_id == rhs.instance_id && this->shift == rhs.shift && this->rotation == rhs.rotation; }
        // 对应的ModelInstance的ID。
        ObjectID instance_id;
        // 缩放G-code坐标中的Slic3r::Point对象
        Point 	shift;
        // 沿Z轴的旋转，弧度为单位。
        float 	rotation;
    };
    const std::vector<Instance>& instances() const { return m_instances; }

    bool                    has_mesh(SLAPrintObjectStep step) const;
    TriangleMesh            get_mesh(SLAPrintObjectStep step) const;

    // 获取围绕XY原点居中且绕Z轴零旋转的支撑网格。
    // 仅当this->is_step_done(slaposSupportTree)为true时支撑网格才有效。
    const TriangleMesh&     support_mesh() const;
    // 获取围绕XY原点居中且绕Z轴零旋转的底座网格。
    // 仅当this->is_step_done(slaposPad)为true时底座网格才有效。
    const TriangleMesh&     pad_mesh() const;

    // 当this->is_step_done(slaposDrillHoles)为true后准备就绪
    const indexed_triangle_set &hollowed_interior_mesh() const;

    // 获取将要打印的带有所有修改的网格
    // 例如中空化和钻孔。
    const TriangleMesh & get_mesh_to_print() const {
        return (m_hollowing_data && is_step_done(slaposDrillHoles)) ? m_hollowing_data->hollow_mesh_with_holes_trimmed : transformed_mesh();
    }

    const TriangleMesh & get_mesh_to_slice() const {
        return (m_hollowing_data && is_step_done(slaposDrillHoles)) ? m_hollowing_data->hollow_mesh_with_holes : transformed_mesh();
    }

    // 这将返回已缓存的变换后的网格
    const TriangleMesh&     transformed_mesh() const;

    sla::SupportPoints      transformed_support_points() const;
    sla::DrainHoles         transformed_drainhole_points() const;

    // 如果需要显示支撑，获取模型几何所需的高度提升。
    // 此Z偏移也应应用于支撑几何。
    // 注意，这与配置中存储的值不同，
    // 因为底座高度也需要考虑。
    double get_elevation() const;

    // 此方法根据处理状态返回所需的高度提升。
    // 如果支撑未就绪，则为零；如果支撑已就绪但底座未就绪，则不带底座；
    // 否则返回完整值。
    double get_current_elevation() const;

    // 此方法返回此 SLAPrintObject 的支撑点。
    const std::vector<sla::SupportPoint>& get_support_points() const;

    // 公共切片记录结构。对应一个可打印层。
    class SliceRecord {
    public:
        // 这是 size_t 的最大限制
        static const size_t NONE = size_t(-1);

        static const SliceRecord EMPTY;

    private:
        coord_t   m_print_z = 0;      // 层的顶部
        float     m_slice_z = 0.f;    // 切片的精确高度
        float     m_height  = 0.f;     // 切片层的高度

        size_t m_model_slices_idx = NONE;
        size_t m_support_slices_idx = NONE;
        const SLAPrintObject *m_po = nullptr;

    public:

        SliceRecord(coord_t key, float slicez, float height):
            m_print_z(key), m_slice_z(slicez), m_height(height) {}

        // 键将是层顶部的整数高度级别。
        coord_t print_level() const { return m_print_z; }

        // 返回切片的精确浮点Z坐标
        float slice_level() const { return m_slice_z; }

        // 返回当前层高
        float layer_height() const { return m_height; }

        bool is_valid() const { return m_po && ! std::isnan(m_slice_z); }

        const SLAPrintObject* print_obj() const { return m_po; }

        // 用于设置切片向量索引的方法。
        void set_model_slice_idx(const SLAPrintObject &po, size_t id) {
            m_po = &po; m_model_slices_idx = id;
        }

        void set_support_slice_idx(const SLAPrintObject& po, size_t id) {
            m_po = &po; m_support_slices_idx = id;
        }

        const ExPolygons& get_slice(SliceOrigin o) const;
        size_t            get_slice_idx(SliceOrigin o) const
        {
            return o == soModel ? m_model_slices_idx : m_support_slices_idx;
        }
    };

private:
    template<class T> inline static T level(const SliceRecord &sr)
    {
        static_assert(std::is_arithmetic<T>::value, "Arithmetic only!");
        return std::is_integral<T>::value ? T(sr.print_level())
                                          : T(sr.slice_level());
    }

    template<class T> inline static SliceRecord create_slice_record(T val)
    {
        static_assert(std::is_arithmetic<T>::value, "Arithmetic only!");
        return std::is_integral<T>::value
                   ? SliceRecord{coord_t(val), 0.f, 0.f}
                   : SliceRecord{0, float(val), 0.f};
    }

    // 这是一个模板方法，用于通过整数键（print_level）或浮点键（slice_level）搜索切片索引。
    // eps 参数给出了正负方向的最大偏差。
    //
    // 此方法可在 const 或非 const 上下文中使用。
    template<class Container, class T>
    static auto closest_slice_record(
            Container& cont,
            T lvl,
            T eps = std::numeric_limits<T>::max()) -> decltype (cont.begin())
    {
        if(cont.empty()) return cont.end();
        if(cont.size() == 1 && std::abs(level<T>(cont.front()) - lvl) > eps)
            return cont.end();

        SliceRecord query = create_slice_record(lvl);

        auto it = std::lower_bound(cont.begin(), cont.end(), query,
                                   [](const SliceRecord& r1,
                                      const SliceRecord& r2)
        {
            return level<T>(r1) < level<T>(r2);
        });

        if(it == cont.end()) return it;

        T diff = std::abs(level<T>(*it) - lvl);

        if(it != cont.begin()) {
            auto it_prev = std::prev(it);
            T diff_prev = std::abs(level<T>(*it_prev) - lvl);
            if(diff_prev < diff) { diff = diff_prev; it = it_prev; }
        }

        if(diff > eps) it = cont.end();

        return it;
    }

    const std::vector<ExPolygons>& get_model_slices() const { return m_model_slices; }
    const std::vector<ExPolygons>& get_support_slices() const;

public:

    // /////////////////////////////////////////////////////////////////////////
    //
    // These methods should be callable on the client side (e.g. UI thread)
    // when the appropriate steps slaposObjectSlice and slaposSliceSupports
    // are ready. All the print objects are processed before slapsRasterize so
    // it is safe to call them during and/or after slapsRasterize.
    //
    // /////////////////////////////////////////////////////////////////////////

    // Retrieve the slice index.
    const std::vector<SliceRecord>& get_slice_index() const {
        return m_slice_index;
    }

    // Search slice index for the closest slice to given print_level.
    // max_epsilon gives the allowable deviation of the returned slice record's
    // level.
    const SliceRecord& closest_slice_to_print_level(
            coord_t print_level,
            coord_t max_epsilon = std::numeric_limits<coord_t>::max()) const
    {
        auto it = closest_slice_record(m_slice_index, print_level, max_epsilon);
        return it == m_slice_index.end() ? SliceRecord::EMPTY : *it;
    }

    // Search slice index for the closest slice to given slice_level.
    // max_epsilon gives the allowable deviation of the returned slice record's
    // level. Use SliceRecord::is_valid() to check the result.
    const SliceRecord& closest_slice_to_slice_level(
            float slice_level,
            float max_epsilon = std::numeric_limits<float>::max()) const
    {
        auto it = closest_slice_record(m_slice_index, slice_level, max_epsilon);
        return it == m_slice_index.end() ? SliceRecord::EMPTY : *it;
    }

protected:
    // to be called from SLAPrint only.
    friend class SLAPrint;

	SLAPrintObject(SLAPrint* print, ModelObject* model_object);
    ~SLAPrintObject();

    void                    config_apply(const ConfigBase &other, bool ignore_nonexistent = false) { m_config.apply(other, ignore_nonexistent); }
    void                    config_apply_only(const ConfigBase &other, const t_config_option_keys &keys, bool ignore_nonexistent = false)
        { m_config.apply_only(other, keys, ignore_nonexistent); }

    void                    set_trafo(const Transform3d& trafo, bool left_handed) {
        m_transformed_rmesh.invalidate([this, &trafo, left_handed](){ m_trafo = trafo; m_left_handed = left_handed; });
    }

    template<class InstVec> inline void set_instances(InstVec&& instances) { m_instances = std::forward<InstVec>(instances); }

    // Invalidates the step, and its depending steps in SLAPrintObject and SLAPrint.
    bool                    invalidate_step(SLAPrintObjectStep step);
    bool                    invalidate_all_steps();
    // Invalidate steps based on a set of parameters changed.
    bool                    invalidate_state_by_config_options(const std::vector<t_config_option_key> &opt_keys);

    // Which steps have to be performed. Implicitly: all
    // to be accessible from SLAPrint
    std::vector<bool>                       m_stepmask;

private:
    // Object specific configuration, pulled from the configuration layer.
    SLAPrintObjectConfig                    m_config;

    // Translation in Z + Rotation by Y and Z + Scaling / Mirroring.
    Transform3d                             m_trafo = Transform3d::Identity();
    // m_trafo is left handed -> 3x3 affine transformation has negative determinant.
    bool                                    m_left_handed = false;

    std::vector<Instance> 					m_instances;

    // Individual 2d slice polygons from lower z to higher z levels
    std::vector<ExPolygons>                 m_model_slices;

    // Exact (float) height levels mapped to the slices. Each record contains
    // the index to the model and the support slice vectors.
    std::vector<SliceRecord>                m_slice_index;

    std::vector<float>                      m_model_height_levels;

    // Caching the transformed (m_trafo) raw mesh of the object
    mutable CachedObject<TriangleMesh>      m_transformed_rmesh;

    class SupportData : public sla::SupportableMesh
    {
    public:
        sla::SupportTree::UPtr  support_tree_ptr; // the supports
        std::vector<ExPolygons> support_slices;   // sliced supports
        TriangleMesh tree_mesh, pad_mesh, full_mesh;

        inline SupportData(const TriangleMesh &t)
            : sla::SupportableMesh{t.its, {}, {}}
        {}

        sla::SupportTree::UPtr &create_support_tree(const sla::JobController &ctl)
        {
            support_tree_ptr = sla::SupportTree::create(*this, ctl);
            tree_mesh = TriangleMesh{support_tree_ptr->retrieve_mesh(sla::MeshType::Support)};
            return support_tree_ptr;
        }

        void create_pad(const ExPolygons &blueprint, const sla::PadConfig &pcfg)
        {
            if (!support_tree_ptr)
                return;

            support_tree_ptr->add_pad(blueprint, pcfg);
            pad_mesh = TriangleMesh{support_tree_ptr->retrieve_mesh(sla::MeshType::Pad)};
        }
    };

    std::unique_ptr<SupportData> m_supportdata;

    class HollowingData
    {
    public:

        sla::InteriorPtr interior;
        mutable TriangleMesh hollow_mesh_with_holes; // caching the complete hollowed mesh
        mutable TriangleMesh hollow_mesh_with_holes_trimmed;
    };

    std::unique_ptr<HollowingData> m_hollowing_data;
};

using PrintObjects = std::vector<SLAPrintObject*>;

using SliceRecord  = SLAPrintObject::SliceRecord;

class TriangleMesh;

struct SLAPrintStatistics
{
    SLAPrintStatistics() { clear(); }
    double                          estimated_print_time;
    double                          objects_used_material;
    double                          support_used_material;
    size_t                          slow_layers_count;
    size_t                          fast_layers_count;
    double                          total_cost;
    double                          total_weight;
    std::vector<double>             layers_times;

    // Config with the filled in print statistics.
    DynamicConfig           config() const;
    // Config with the statistics keys populated with placeholder strings.
    static DynamicConfig    placeholders();
    // Replace the print statistics placeholders in the path.
    std::string             finalize_output_path(const std::string &path_in) const;

    void clear() {
        estimated_print_time = 0.;
        objects_used_material = 0.;
        support_used_material = 0.;
        slow_layers_count = 0;
        fast_layers_count = 0;
        total_cost = 0.;
        total_weight = 0.;
        layers_times.clear();
    }
};

class SLAArchive {
protected:
    std::vector<sla::EncodedRaster> m_layers;

    virtual std::unique_ptr<sla::RasterBase> create_raster() const = 0;
    virtual sla::RasterEncoder get_encoder() const = 0;

public:
    virtual ~SLAArchive() = default;

    virtual void apply(const SLAPrinterConfig &cfg) = 0;

    // Fn have to be thread safe: void(sla::RasterBase& raster, size_t lyrid);
    template<class Fn, class CancelFn, class EP = ExecutionTBB>
    void draw_layers(
        size_t     layer_num,
        Fn &&      drawfn,
        CancelFn cancelfn = []() { return false; },
        const EP & ep       = {})
    {
        m_layers.resize(layer_num);
        execution::for_each(
            ep, size_t(0), m_layers.size(),
            [this, &drawfn, &cancelfn](size_t idx) {
                if (cancelfn()) return;

                sla::EncodedRaster &enc = m_layers[idx];
                auto                rst = create_raster();
                drawfn(*rst, idx);
                enc = rst->encode(get_encoder());
            },
            execution::max_concurrency(ep));
    }
};

/**
 * @brief This class is the high level FSM for the SLA printing process.
 *
 * It should support the background processing framework and contain the
 * metadata for the support geometries and their slicing. It should also
 * dispatch the SLA printing configuration values to the appropriate calculation
 * steps.
 */
class SLAPrint : public PrintBaseWithState<SLAPrintStep, slapsCount>
{
private: // Prevents erroneous use by other classes.
    typedef PrintBaseWithState<SLAPrintStep, slapsCount> Inherited;

    class Steps; // See SLAPrintSteps.cpp

public:

    SLAPrint(): m_stepmask(slapsCount, true) {}

    virtual ~SLAPrint() override { this->clear(); }

    PrinterTechnology	technology() const noexcept override { return ptSLA; }

    void                clear() override;
    bool                empty() const override { return m_objects.empty(); }
    // List of existing PrintObject IDs, to remove notifications for non-existent IDs.
    std::vector<ObjectID> print_object_ids() const override;
    ApplyStatus         apply(const Model &model, DynamicPrintConfig config) override;
    void                set_task(const TaskParams &params) override;
    void                process(long long *time_cost_with_cache = nullptr, bool use_cache = false) override;
    void                finalize() override;
    // Returns true if an object step is done on all objects and there's at least one object.
    bool                is_step_done(SLAPrintObjectStep step) const;
    // Returns true if the last step was finished with success.
    bool                finished() const override { return this->is_step_done(slaposSliceSupports) && this->Inherited::is_step_done(slapsRasterize); }

    const PrintObjects& objects() const { return m_objects; }
    // PrintObject by its ObjectID, to be used to uniquely bind slicing warnings to their source PrintObjects
    // in the notification center.
    const SLAPrintObject* get_print_object_by_model_object_id(ObjectID object_id) const {
        auto it = std::find_if(m_objects.begin(), m_objects.end(),
            [object_id](const SLAPrintObject* obj) { return obj->model_object()->id() == object_id; });
        return (it == m_objects.end()) ? nullptr : *it;
    }
    const SLAPrintObject* get_object(ObjectID object_id) const {
        auto it = std::find_if(m_objects.begin(), m_objects.end(),
            [object_id](const SLAPrintObject *obj) { return obj->id() == object_id; });
        return (it == m_objects.end()) ? nullptr : *it;
    }

    const SLAPrintConfig&       print_config() const { return m_print_config; }
    const SLAPrinterConfig&     printer_config() const { return m_printer_config; }
    const SLAMaterialConfig&    material_config() const { return m_material_config; }
    const SLAPrintObjectConfig& default_object_config() const { return m_default_object_config; }

    // Extracted value from the configuration objects
    Vec3d                       relative_correction() const;

    // Return sla tansformation for a given model_object
    Transform3d sla_trafo(const ModelObject &model_object) const;

	std::string                 output_filename(const std::string &filename_base = std::string()) const override;

    const SLAPrintStatistics&   print_statistics() const { return m_print_statistics; }

    StringObjectException validate(StringObjectException *                 warning           = nullptr,
                                   Polygons *                              collison_polygons = nullptr,
                                   std::vector<std::pair<Polygon, float>> *height_polygons   = nullptr) const override;

    // An aggregation of SliceRecord-s from all the print objects for each
    // occupied layer. Slice record levels dont have to match exactly.
    // They are unified if the level difference is within +/- SCALED_EPSILON
    class PrintLayer {
        coord_t m_level;

        // The collection of slice records for the current level.
        std::vector<std::reference_wrapper<const SliceRecord>> m_slices;

        ExPolygons m_transformed_slices;

        template<class Container> void transformed_slices(Container&& c)
        {
            m_transformed_slices = std::forward<Container>(c);
        }

        friend class SLAPrint::Steps;

    public:

        explicit PrintLayer(coord_t lvl) : m_level(lvl) {}

        // for being sorted in their container (see m_printer_input)
        bool operator<(const PrintLayer& other) const {
            return m_level < other.m_level;
        }

        void add(const SliceRecord& sr) { m_slices.emplace_back(sr); }

        coord_t level() const { return m_level; }

        auto slices() const -> const decltype (m_slices)& { return m_slices; }

        const ExPolygons & transformed_slices() const {
            return m_transformed_slices;
        }
    };

    // The aggregated and leveled print records from various objects.
    // TODO: use this structure for the preview in the future.
    const std::vector<PrintLayer>& print_layers() const { return m_printer_input; }

    void set_printer(SLAArchive *archiver);

private:

    // Implement same logic as in SLAPrintObject
    bool invalidate_step(SLAPrintStep st);

    // Invalidate steps based on a set of parameters changed.
    bool invalidate_state_by_config_options(const std::vector<t_config_option_key> &opt_keys, bool &invalidate_all_model_objects);

    SLAPrintConfig                  m_print_config;
    SLAPrinterConfig                m_printer_config;
    SLAMaterialConfig               m_material_config;
    SLAPrintObjectConfig            m_default_object_config;

    PrintObjects                    m_objects;
    std::vector<bool>               m_stepmask;

    // Ready-made data for rasterization.
    std::vector<PrintLayer>         m_printer_input;

    // The archive object which collects the raster images after slicing
    SLAArchive                     *m_printer = nullptr;

    // Estimated print time, material consumed.
    SLAPrintStatistics              m_print_statistics;

    class StatusReporter
    {
        double m_st = 0;

    public:
        void operator()(SLAPrint &         p,
                        double             st,
                        const std::string &msg,
                        unsigned           flags = SlicingStatus::DEFAULT,
                        const std::string &logmsg = "");

        double status() const { return m_st; }
    } m_report_status;

	friend SLAPrintObject;
};

// Helper functions:

bool is_zero_elevation(const SLAPrintObjectConfig &c);

sla::SupportTreeConfig make_support_cfg(const SLAPrintObjectConfig& c);

sla::PadConfig::EmbedObject builtin_pad_cfg(const SLAPrintObjectConfig& c);

sla::PadConfig make_pad_cfg(const SLAPrintObjectConfig& c);

bool validate_pad(const indexed_triangle_set &pad, const sla::PadConfig &pcfg);


} // namespace Slic3r

#endif /* slic3r_SLAPrint_hpp_ */
