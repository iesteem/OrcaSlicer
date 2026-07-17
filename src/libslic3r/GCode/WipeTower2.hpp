// Orca: 适用于所有非BBL打印机的WipeTower2，支持所有MMU设备和工具更换器

#ifndef WipeTower2_
#define WipeTower2_

#include <cmath>
#include <string>
#include <sstream>
#include <utility>
#include <algorithm>

#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "WipeTower.hpp"
namespace Slic3r
{

class WipeTowerWriter2;
class PrintRegionConfig;

class WipeTower2
{
public:
    static const std::string never_skip_tag() { return "_GCODE_WIPE_TOWER_NEVER_SKIP_TAG"; }
    static std::pair<double, double> get_wipe_tower_cone_base(double width, double height, double depth, double angle_deg);
    static std::vector<std::vector<float>> extract_wipe_volumes(const PrintConfig& config);


    // 从WipeTower2和WipeTowerWriter2的当前状态构造ToolChangeResult。
    // WipeTowerWriter2被移动！
    WipeTower::ToolChangeResult construct_tcr(WipeTowerWriter2& writer,
                                   bool priming,
                                   size_t old_tool,
                                   bool is_finish) const;

    // x            -- x坐标，mm（左下角）
    // y            -- y坐标，mm（左下角）
    // width        -- 擦拭塔宽度，mm（默认为60mm - 保持原样）
    // wipe_area    -- 一次工具更换可用空间，mm
    WipeTower2(const PrintConfig& config,
              const PrintRegionConfig& default_region_config,
              int plate_idx, Vec3d plate_origin,
              const std::vector<std::vector<float>>& wiping_matrix,
              size_t initial_tool);

    // 设置挤出机属性。
    void set_extruder(size_t idx, const PrintConfig& config);

    // 追加到包含未来擦拭塔信息的内部结构m_plan中
    // 在开始构建之前使用。条目必须按z顺序添加。
    void plan_toolchange(float z_par, float layer_height_par, unsigned int old_tool, unsigned int new_tool, float wipe_volume = 0.f);
    void plan_local_z_toolchange(float z_par, float layer_height_par, unsigned int old_tool, unsigned int new_tool, float wipe_volume = 0.f);
    void plan_local_z_reserve(float z_par, float layer_height_par, size_t reserve_slot_count, float wipe_volume = 0.f);

    // 遍历已准备的m_plan，生成ToolChangeResult并将它们追加到"result"中
    void generate(std::vector<std::vector<WipeTower::ToolChangeResult>> &result,
                  std::vector<std::vector<WipeTower::ToolChangeResult>> &local_z_result);

    float get_depth() const { return m_wipe_tower_depth; }
    std::vector<std::pair<float, float>> get_z_and_depth_pairs() const;
    std::vector<std::vector<WipeTower::box_coordinates>> get_local_z_reserve_boxes() const;
    float get_brim_width() const { return m_wipe_tower_brim_width_real; }
    float get_wipe_tower_height() const { return m_wipe_tower_height; }





    // 切换到下一层。
    void set_layer(
        // 此层的打印高度。
        float print_z,
        // 层高，用于计算挤出率。
        float layer_height,
        // 此层或以下层的最大工具更换次数。
        size_t max_tool_changes,
        // 这是打印的第一层吗？在这种情况下先打印brim。（已废弃）
        bool /*is_first_layer*/,
        // 这是废料塔的最后一层吗？
        bool is_last_layer)
    {
        m_z_pos                 = print_z;
        m_layer_height          = layer_height;
        m_depth_traversed  = 0.f;
        m_current_layer_finished = false;


        // 推进m_layer_info迭代器，确保正确
        while (!m_plan.empty() && m_layer_info->z < print_z - WT_EPSILON && m_layer_info+1 != m_plan.end())
            ++m_layer_info;

        //m_current_shape = (! this->is_first_layer() && m_current_shape == SHAPE_NORMAL) ? SHAPE_REVERSED : SHAPE_NORMAL;
        m_current_shape = SHAPE_NORMAL;
        if (this->is_first_layer()) {
            m_num_layer_changes = 0;
            m_num_tool_changes  = 0;
        } else
            ++ m_num_layer_changes;

        // 根据所需线宽、喷嘴直径、丝材直径和层高计算挤出流量：
        m_extrusion_flow = extrusion_flow(layer_height);
    }

    // 返回擦拭塔位置。
    const Vec2f&      position() const { return m_wipe_tower_pos; }
    // 返回擦拭塔宽度。
    float             width()    const { return m_wipe_tower_width; }
    // 擦拭塔已完成，不应有更多工具更换或擦拭塔打印。
    bool              finished() const { return m_max_color_changes == 0; }

    // 返回在打印床前缘初始化喷嘴的gcode。
    std::vector<WipeTower::ToolChangeResult> prime(
        // 第一层的print_z。
        float                       first_layer_height,
        // 挤出机索引，按初始顺序排列。最后一个挤出机稍后将打印擦拭塔brim、打印brim和对象。
        const std::vector<unsigned int> &tools,
        // 如果为true，最后一个初始区域将与其他初始区域相同，其余擦拭将在擦拭塔内执行。
        // 如果为false，最后一个初始区域将足够大以充分擦拭最后一个挤出机。
        bool                        last_wipe_inside_wipe_tower);

    // 返回工具更换和最终打印头位置的gcode。
    // 在第一层，首先在未来的擦拭塔周围挤出brim。
    WipeTower::ToolChangeResult tool_change(size_t new_tool);
    WipeTower::ToolChangeResult local_z_tool_change(size_t new_tool, const WipeTower::box_coordinates& cleaning_box, float wipe_volume);
    void set_current_tool(size_t tool) { m_current_tool = tool; }

    // 用稀疏填充填充未填充的空间。
    // 仅在layer_finished()为false时调用此方法。
    WipeTower::ToolChangeResult finish_layer();

    // 当前层是否完成？
    bool              layer_finished() const {
        return m_current_layer_finished;
    }

    std::vector<float> get_used_filament() const { return m_used_filament_length; }
    std::vector<std::pair<float, std::vector<float>>> get_used_filament_until_layer() const { return m_used_filament_length_until_layer; }
    int get_number_of_toolchanges() const { return m_num_tool_changes; }

    struct FilamentParameters {
        std::string         material = "PLA";
        bool                is_soluble = false;
        int                 temperature = 0;
        int                 first_layer_temperature = 0;
        float               loading_speed = 0.f;
        float               loading_speed_start = 0.f;
        float               unloading_speed = 0.f;
        float               unloading_speed_start = 0.f;
        float               delay = 0.f ;

        float               filament_stamping_loading_speed = 0.f;
        float               filament_stamping_distance = 0.f;

        int                 cooling_moves = 0;
        float               cooling_initial_speed = 0.f;
        float               cooling_final_speed = 0.f;
        float               ramming_line_width_multiplicator = 1.f;
        float               ramming_step_multiplicator = 1.f;
        float               max_e_speed = std::numeric_limits<float>::max();
        std::vector<float>  ramming_speed;
        float               nozzle_diameter;
        float               filament_area;
        bool                multitool_ramming;
        float               multitool_ramming_time = 0.f;
        float               multitool_ramming_volume = 0.f;
        float               filament_minimal_purge_on_wipe_tower = 0.f;
        float               retract_length;
        float               retract_speed;
        float               flat_iron_area;
    };

    const std::map<float, Polylines>& get_outer_wall() const { return m_outer_wall; }

private:
    struct WipeTowerInfo;

    enum wipe_shape // 填充方向
    {
        SHAPE_NORMAL = 1,
        SHAPE_REVERSED = -1
    };

    const float Width_To_Nozzle_Ratio = 1.25f; // 所需线宽（椭圆形）为喷嘴直径的倍数 - 实际上可能不需要调整
    const float WT_EPSILON            = 1e-3f;
    float filament_area() const {
        return m_filpar[0].filament_area; // 此时假定所有挤出机具有相同的丝材直径
    }

    bool   m_change_pressure         = true;
    float  m_change_pressure_value   = 0.0;
    float  m_ramming_width_ratio     = 2.0;
    bool   m_semm               = true; // 是否使用单挤出机多材料打印机？
    bool   m_enable_filament_ramming = true;
    bool   m_is_mk4mmu3         = false;
    std::string m_printer_model;    // 打印机型号名称（例如"Snapmaker U1"）
    Vec2f  m_wipe_tower_pos;            // 擦拭塔的左前角，mm。
    float  m_wipe_tower_width;          // 擦拭塔的宽度。
    float  m_wipe_tower_depth   = 0.f;  // 擦拭塔的深度
    float  m_wipe_tower_height  = 0.f;
    float  m_wipe_tower_cone_angle = 0.f;
    float  m_wipe_tower_brim_width      = 0.f;  // 配置文件中的brim宽度（mm）
    float  m_wipe_tower_brim_width_real = 0.f;  // 生成后的brim宽度（mm）
    bool   m_prime_tower_brim_chamfer          = true;   // 启用/禁用brim倒角
    float  m_prime_tower_brim_chamfer_max_width = 4.f;   // 最大倒角宽度（mm）
    float  m_wipe_tower_rotation_angle = 0.f; // 擦拭塔旋转角度（度），相对于x轴
    float  m_internal_rotation  = 0.f;
    float  m_y_shift            = 0.f;  // 传递给写入器的y偏移
    float  m_z_pos              = 0.f;  // 当前Z位置。
    float  m_layer_height       = 0.f;  // 当前层高。
    size_t m_max_color_changes  = 0;    // 每层的最大颜色更改次数。
    int    m_old_temperature    = -1;   // 跟踪上次设置的温度（以便在不需要时不发出命令）
    float  m_travel_speed       = 0.f;
    float  m_infill_speed       = 0.f;
    float  m_wipe_tower_max_purge_speed   = 90.f;
    float  m_perimeter_speed    = 0.f;
    float  m_first_layer_speed  = 0.f;
    size_t m_first_layer_idx    = size_t(-1);

    int m_wall_type;
    bool   m_used_fillet                  = true;
    bool   m_use_gap_wall                 = true;
    float  m_rib_width                    = 10;
    float  m_extra_rib_length             = 0;
    std::vector<std::vector<Vec2f>> m_wall_skip_points;
    float  m_rib_length                   = 0;

    bool   m_enable_arc_fitting           = false;
    std::map<float, Polylines> m_outer_wall; // 用于擦拭塔外壁和brim

    // G-code生成器参数。
    float           m_cooling_tube_retraction   = 0.f;
    float           m_cooling_tube_length       = 0.f;
    float           m_parking_pos_retraction    = 0.f;
    float           m_extra_loading_move        = 0.f;
    float           m_bridging                  = 0.f;
    bool            m_no_sparse_layers          = false;
    bool            m_set_extruder_trimpot      = false;
    bool            m_adhesion                  = true;
    GCodeFlavor     m_gcode_flavor;

    // 热床属性
    enum {
        RectangularBed,
        CircularBed,
        CustomBed
    } m_bed_shape;
    float m_bed_width; // 热床边界框的宽度
    Vec2f m_bed_bottom_left; // 左下角坐标（用于矩形热床）

    float m_perimeter_width = 0.4f * Width_To_Nozzle_Ratio; // 挤出线的宽度，也是100%填充的周长间距。
    float m_extrusion_flow = 0.038f; //0.029f;// 挤出流量来自m_perimeter_width、层高和丝材直径。

    // 挤出机特定参数。
    std::vector<FilamentParameters> m_filpar;

    // 擦拭塔生成器的状态。
    unsigned int m_num_layer_changes = 0; // 用于输出统计的层更改计数器。
    unsigned int m_num_tool_changes  = 0; // 用于输出统计的工具更改更改计数器。

    // 填充方向（正Y、负Y）随每层交替。
    wipe_shape      m_current_shape = SHAPE_NORMAL;
    size_t  m_current_tool  = 0;
    const std::vector<std::vector<float>> wipe_volumes;

    float           m_depth_traversed = 0.f; // 擦拭塔上的当前y位置。
    bool            m_current_layer_finished = false;
    bool            m_left_to_right   = true;
    float           m_extra_flow      = 1.f;
    float           m_extra_spacing_wipe    = 1.f;
    float           m_extra_spacing_ramming = 1.f;
    float           m_local_z_wipe_tower_purge_lines = 3.f;

    bool is_first_layer() const { return size_t(m_layer_info - m_plan.begin()) == m_first_layer_idx; }

    // 计算给定层高产生所需线宽所需的挤出流量
    float extrusion_flow(float layer_height = -1.f) const   // 负layer_height - 返回当前m_extrusion_flow
    {
        if ( layer_height < 0 )
            return m_extrusion_flow;
        return layer_height * ( m_perimeter_width - layer_height * (1.f-float(M_PI)/4.f)) / filament_area();
    }


    // 计算所有层的深度并向下传播
    void plan_tower();

    // 遍历m_plan，计算边界和finish_layer挤出并从上次擦拭中减去
    void save_on_last_wipe();

    // 存储给定层的工具更改信息
    struct WipeTowerInfo{
        struct ToolChange {
            size_t old_tool;
            size_t new_tool;
            float required_depth;
            float ramming_depth;
            float first_wipe_line;
            float wipe_volume;
            float wipe_volume_total;
            ToolChange(size_t old, size_t newtool, float depth=0.f, float ramming_depth=0.f, float fwl=0.f, float wv=0.f)
            : old_tool{old}, new_tool{newtool}, required_depth{depth}, ramming_depth{ramming_depth}, first_wipe_line{fwl}, wipe_volume{wv}, wipe_volume_total{wv} {}
        };
        float z;        // 层的z位置
        float height;   // 层高
        float depth;    // 基于所有上层计算的层深度
        float normal_toolchanges_depth() const { float sum = 0.f; for (const auto &a : tool_changes) sum += a.required_depth; return sum; }
        float local_z_toolchanges_depth() const { float sum = 0.f; for (const auto &a : local_z_tool_changes) sum += a.required_depth; return sum; }
        float toolchanges_depth() const { return normal_toolchanges_depth() + local_z_toolchanges_depth(); }
        float local_z_reserve_slot_depth { 0.f };
        size_t local_z_reserve_slot_count { 0 };
        float local_z_reserve_depth() const { return local_z_reserve_slot_depth * float(local_z_reserve_slot_count); }
        float planned_depth() const { return toolchanges_depth() + local_z_reserve_depth(); }

        std::vector<ToolChange> tool_changes;
        std::vector<ToolChange> local_z_tool_changes;

        WipeTowerInfo(float z_par, float layer_height_par)
            : z{z_par}, height{layer_height_par}, depth{0} {}
    };

    std::vector<WipeTowerInfo> m_plan;   // 存储关于未来擦拭塔的所有层和工具更改的信息（由plan_toolchange(...)填充）
    std::vector<WipeTowerInfo>::iterator m_layer_info = m_plan.end();
    const WipeTowerInfo::ToolChange     *m_active_tool_change = nullptr;

    // 这累加所有挤出层的高度，不计算使用"no_sparse_layers"时随后将被移除的层。
    float m_current_height = 0.f;

    // 存储每个挤出机的已使用丝材长度信息：
    std::vector<float> m_used_filament_length;
    std::vector<std::pair<float, std::vector<float>>> m_used_filament_length_until_layer;

    // 返回第一个切换到非可溶性挤出机的工具更改索引
    // 如果没有这样的工具更改则返回-1。
    int first_toolchange_to_nonsoluble(
            const std::vector<WipeTowerInfo::ToolChange>& tool_changes) const;
    bool layer_has_soluble_toolchange(const WipeTowerInfo &layer) const;
    float cumulative_toolchange_depth_before(const WipeTowerInfo::ToolChange *tool_change) const;
    WipeTower::ToolChangeResult emit_planned_tool_change(const WipeTowerInfo::ToolChange *tool_change);

    void toolchange_Unload(
        WipeTowerWriter2 &writer,
        const WipeTower::box_coordinates  &cleaning_box,
        const std::string&   current_material,
        const int            old_temperature,
        const int            new_temperature);

    void toolchange_Change(WipeTowerWriter2 &writer, const size_t new_tool,
        const std::string& new_material);

    void toolchange_Load(
        WipeTowerWriter2 &writer,
        const WipeTower::box_coordinates  &cleaning_box);

    void toolchange_Wipe(
        WipeTowerWriter2 &writer,
        const WipeTower::box_coordinates  &cleaning_box,
        float wipe_volume);

    Polygon generate_support_rib_wall(WipeTowerWriter2&                 writer,
                                      const WipeTower::box_coordinates& wt_box,
                                      double                 feedrate,
                                      bool                   first_layer,
                                      bool                   rib_wall,
                                      bool                   extrude_perimeter,
                                      const std::vector<Vec2f>&         skip_points);

    void get_all_wall_skip_points();
    // 为特定层检索预先计算的间隙点。如果layer_id超出范围，则返回空。
    std::vector<Vec2f> get_wall_skip_points(size_t layer_id);
    // 预测toolchange_Unload ramming后的喷嘴X，匹配其xl/xr和do_ramming逻辑。
    // old_tool: 正在卸载的丝材的挤出机索引
    float predict_ramming_end_x(int old_tool, float layer_height) const;

    Polygon generate_support_cone_wall(
        WipeTowerWriter2& writer,
        const WipeTower::box_coordinates& wt_box,
        double feedrate,
        bool infill_cone,
        float spacing,
        const std::vector<Vec2f>& skip_points = {});

    Polygon generate_rib_polygon(const WipeTower::box_coordinates& wt_box);

    WipeTowerInfo::ToolChange set_toolchange(int old_tool, int new_tool, float layer_height, float wipe_volume);
};




} // namespace Slic3r

#endif // slic3r_GCode_WipeTower_hpp_
