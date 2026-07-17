#ifndef slic3r_GCodeProcessor_hpp_
#define slic3r_GCodeProcessor_hpp_

#include "libslic3r/GCodeReader.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/CustomGCode.hpp"

#include <cstdint>
#include <array>
#include <vector>
#include <mutex>
#include <string>
#include <string_view>
#include <optional>

namespace Slic3r {

class Print;

// 切片警告枚举字符串
#define NOZZLE_HRC_CHECKER                                          "the_actual_nozzle_hrc_smaller_than_the_required_nozzle_hrc"
#define BED_TEMP_TOO_HIGH_THAN_FILAMENT                             "bed_temperature_too_high_than_filament"
#define NOT_SUPPORT_TRADITIONAL_TIMELAPSE                           "not_support_traditional_timelapse"
#define NOT_GENERATE_TIMELAPSE                                      "not_generate_timelapse"
#define LONG_RETRACTION_WHEN_CUT                                    "activate_long_retraction_when_cut"

    enum class EMoveType : unsigned char
    {
        Noop,
        Retract,
        Unretract,
        Seam,
        Tool_change,
        Color_change,
        Pause_Print,
        Custom_GCode,
        Travel,
        Wipe,
        Extrude,
        Count
    };

    struct PrintEstimatedStatistics
    {
        enum class ETimeMode : unsigned char
        {
            Normal,
            Stealth,
            Count
        };

        struct Mode
        {
            float time;
            float prepare_time;
            std::vector<std::pair<CustomGCode::Type, std::pair<float, float>>> custom_gcode_times;
            std::vector<std::pair<EMoveType, float>> moves_times;
            std::vector<std::pair<ExtrusionRole, float>> roles_times;
            std::vector<float> layers_times;

            void reset() {
                time = 0.0f;
                prepare_time = 0.0f;
                custom_gcode_times.clear();
                custom_gcode_times.shrink_to_fit();
                moves_times.clear();
                moves_times.shrink_to_fit();
                roles_times.clear();
                roles_times.shrink_to_fit();
                layers_times.clear();
                layers_times.shrink_to_fit();
            }
        };

        std::vector<double>                                 volumes_per_color_change;
        std::map<size_t, double>                            model_volumes_per_extruder;
        std::map<size_t, double>                            wipe_tower_volumes_per_extruder;
        std::map<size_t, double>                            support_volumes_per_extruder;
        std::map<size_t, double>                            total_volumes_per_extruder;
        //BBS: 每种丝材的冲洗量
        std::map<size_t, double>                            flush_per_filament;
        std::map<ExtrusionRole, std::pair<double, double>>  used_filaments_per_role;

        std::array<Mode, static_cast<size_t>(ETimeMode::Count)> modes;
        unsigned int                                        total_filamentchanges;

        PrintEstimatedStatistics() { reset(); }

        void reset() {
            for (auto m : modes) {
                m.reset();
            }
            volumes_per_color_change.clear();
            volumes_per_color_change.shrink_to_fit();
            wipe_tower_volumes_per_extruder.clear();
            model_volumes_per_extruder.clear();
            support_volumes_per_extruder.clear();
            total_volumes_per_extruder.clear();
            flush_per_filament.clear();
            used_filaments_per_role.clear();
            total_filamentchanges = 0;
        }
    };

    struct ConflictResult
    {
        std::string        _objName1;
        std::string        _objName2;
        double             _height;
        const void *_obj1; // nullptr表示擦拭塔
        const void *_obj2;
        int                layer = -1;
        ConflictResult(const std::string &objName1, const std::string &objName2, double height, const void *obj1, const void *obj2)
            : _objName1(objName1), _objName2(objName2), _height(height), _obj1(obj1), _obj2(obj2)
        {}
        ConflictResult() = default;
    };

    struct BedMatchResult
    {
        bool match;
        std::string bed_type_name;
        int extruder_id;
        BedMatchResult():match(true),bed_type_name(""),extruder_id(-1) {}
        BedMatchResult(bool _match,const std::string& _bed_type_name="",int _extruder_id=-1)
            :match(_match),bed_type_name(_bed_type_name),extruder_id(_extruder_id)
        {}
    };

    using ConflictResultOpt = std::optional<ConflictResult>;

    struct GCodeProcessorResult
    {
        ConflictResultOpt conflict_result;
        BedMatchResult  bed_match_result;

        struct SettingsIds
        {
            std::string print;
            std::vector<std::string> filament;
            std::string printer;

            void reset() {
                print.clear();
                filament.clear();
                printer.clear();
            }
        };

        struct MoveVertex
        {
            unsigned int gcode_id{ 0 };
            EMoveType type{ EMoveType::Noop };
            ExtrusionRole extrusion_role{ erNone };
            unsigned char extruder_id{ 0 };
            unsigned char cp_color_id{ 0 };
            Vec3f position{ Vec3f::Zero() }; // mm
            float delta_extruder{ 0.0f }; // mm
            float feedrate{ 0.0f }; // mm/s
            float width{ 0.0f }; // mm
            float height{ 0.0f }; // mm
            float mm3_per_mm{ 0.0f };
            float travel_dist{ 0.0f }; // mm
            float fan_speed{ 0.0f }; // 百分比
            float temperature{ 0.0f }; // 摄氏度
            float time{ 0.0f }; // s
            float layer_duration{ 0.0f }; // s (最终确定前的层id)


            //BBS: 圆弧移动相关数据
            EMovePathType move_path_type{ EMovePathType::Noop_move };
            Vec3f arc_center_position{ Vec3f::Zero() };      // mm
            std::vector<Vec3f> interpolation_points;     // 用于绘图的圆弧插值点

            float volumetric_rate() const { return feedrate * mm3_per_mm; }
            //BBS: 支持圆弧移动的新函数
            bool is_arc_move_with_interpolation_points() const {
                return (move_path_type == EMovePathType::Arc_move_ccw || move_path_type == EMovePathType::Arc_move_cw) && interpolation_points.size();
            }
            bool is_arc_move() const {
                return move_path_type == EMovePathType::Arc_move_ccw || move_path_type == EMovePathType::Arc_move_cw;
            }
        };

        struct SliceWarning {
            int         level;                  // 0: 普通提示, 1: 警告; 2: 错误
            std::string msg;                    // 枚举字符串
            std::string error_code;             // 用于studio的错误码
            std::vector<std::string> params;    // 额外消息信息
        };

        std::string filename;
        unsigned int id;
        std::vector<MoveVertex> moves;
        // 在TimeProcessor::post_process()最终确定G-code后，最终G-code this->filename的行结束位置。
        std::vector<size_t> lines_ends;
        Pointfs printable_area;
        //BBS: 添加床排除区域
        Pointfs bed_exclude_area;
        //BBS: 添加外部刀路
        bool toolpath_outside;
        //BBS: 添加对象标签启用
        bool label_object_enabled;
        //BBS: 更换丝材时的额外回抽，实验功能
        bool long_retraction_when_cut {0};
        int timelapse_warning_code {0};
        bool support_traditional_timelapse{true};
        float printable_height;
        SettingsIds settings_ids;
        size_t extruders_count;
        bool backtrace_enabled;
        std::vector<std::string> extruder_colors;
        std::vector<float> filament_diameters;
        std::vector<int>   required_nozzle_HRC;
        std::vector<float> filament_densities;
        std::vector<float> filament_costs;
        std::vector<int> filament_vitrification_temperature;
        PrintEstimatedStatistics print_statistics;
        std::vector<CustomGCode::Item> custom_gcode_per_print_z;
        std::vector<std::pair<float, std::pair<size_t, size_t>>> spiral_vase_layers;
        //BBS
        std::vector<SliceWarning> warnings;
        int nozzle_hrc;
        NozzleType nozzle_type;
        BedType bed_type = BedType::btCount;
#if ENABLE_GCODE_VIEWER_STATISTICS
        int64_t time{ 0 };
#endif // ENABLE_GCODE_VIEWER_STATISTICS
        void reset();

        //BBS: 添加互斥锁以保护gcode结果
        mutable std::mutex result_mutex;
        GCodeProcessorResult& operator=(const GCodeProcessorResult &other)
        {
            filename = other.filename;
            id = other.id;
            moves = other.moves;
            lines_ends = other.lines_ends;
            printable_area = other.printable_area;
            bed_exclude_area = other.bed_exclude_area;
            toolpath_outside = other.toolpath_outside;
            label_object_enabled = other.label_object_enabled;
            long_retraction_when_cut = other.long_retraction_when_cut;
            timelapse_warning_code = other.timelapse_warning_code;
            printable_height = other.printable_height;
            settings_ids = other.settings_ids;
            extruders_count = other.extruders_count;
            extruder_colors = other.extruder_colors;
            filament_diameters = other.filament_diameters;
            filament_densities = other.filament_densities;
            filament_costs = other.filament_costs;
            print_statistics = other.print_statistics;
            custom_gcode_per_print_z = other.custom_gcode_per_print_z;
            spiral_vase_layers = other.spiral_vase_layers;
            warnings = other.warnings;
            bed_type = other.bed_type;
            bed_match_result = other.bed_match_result;
#if ENABLE_GCODE_VIEWER_STATISTICS
            time = other.time;
#endif
            return *this;
        }
        void  lock() const { result_mutex.lock(); }
        void  unlock() const { result_mutex.unlock(); }
    };


    class GCodeProcessor
    {
        static const std::vector<std::string> Reserved_Tags;
        static const std::vector<std::string> Reserved_Tags_compatible;
        static const std::string Flush_Start_Tag;
        static const std::string Flush_End_Tag;
        static const std::string External_Purge_Tag;
    public:
        enum class ETags : unsigned char
        {
            Role,
            Wipe_Start,
            Wipe_End,
            Height,
            Width,
            Layer_Change,
            Color_Change,
            Pause_Print,
            Custom_Code,
            First_Line_M73_Placeholder,
            Last_Line_M73_Placeholder,
            Estimated_Printing_Time_Placeholder,
            Total_Layer_Number_Placeholder,
            Manual_Tool_Change,
            During_Print_Exhaust_Fan,
            Wipe_Tower_Start,
            Wipe_Tower_End,
            PA_Change,
        };

        static const std::string& reserved_tag(ETags tag) { return s_IsBBLPrinter ? Reserved_Tags[static_cast<unsigned char>(tag)] : Reserved_Tags_compatible[static_cast<unsigned char>(tag)]; }
        // 检查给定gcode中的保留标签，找到第一个时返回true（其值返回到found_tag中）
        static bool contains_reserved_tag(const std::string& gcode, std::string& found_tag);
        // 检查给定gcode中的保留标签，找到任何标签时返回true
        // （前max_count个找到的标签返回到found_tag中）
        static bool contains_reserved_tags(const std::string& gcode, unsigned int max_count, std::vector<std::string>& found_tag);

        static int get_gcode_last_filament(const std::string &gcode_str);
        static bool get_last_z_from_gcode(const std::string& gcode_str, double& z);

        static const float Wipe_Width;
        static const float Wipe_Height;

        static bool s_IsBBLPrinter;

#if ENABLE_GCODE_VIEWER_DATA_CHECKING
        static const std::string Mm3_Per_Mm_Tag;
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING

    private:
        using AxisCoords = std::array<double, 4>;
        using ExtruderColors = std::vector<unsigned char>;
        using ExtruderTemps = std::vector<float>;

        enum class EUnits : unsigned char
        {
            Millimeters,
            Inches
        };

        enum class EPositioningType : unsigned char
        {
            Absolute,
            Relative
        };

        struct CachedPosition
        {
            AxisCoords position; // mm
            float feedrate; // mm/s

            void reset();
        };

        struct CpColor
        {
            unsigned char counter;
            unsigned char current;

            void reset();
        };

    public:
        struct FeedrateProfile
        {
            float entry{ 0.0f }; // mm/s
            float cruise{ 0.0f }; // mm/s
            float exit{ 0.0f }; // mm/s
        };

        struct Trapezoid
        {
            float accelerate_until{ 0.0f }; // mm
            float decelerate_after{ 0.0f }; // mm
            float cruise_feedrate{ 0.0f }; // mm/sec

            float acceleration_time(float entry_feedrate, float acceleration) const;
            float cruise_time() const;
            float deceleration_time(float distance, float acceleration) const;
            float cruise_distance() const;
        };

        struct TimeBlock
        {
            struct Flags
            {
                bool recalculate{ false };
                bool nominal_length{ false };
                bool prepare_stage{ false };
            };

            EMoveType move_type{ EMoveType::Noop };
            ExtrusionRole role{ erNone };
            unsigned int g1_line_id{ 0 };
            unsigned int remaining_internal_g1_lines{ 0 };
            unsigned int layer_id{ 0 };
            float distance{ 0.0f }; // mm
            float acceleration{ 0.0f }; // mm/s^2
            float max_entry_speed{ 0.0f }; // mm/s
            float safe_feedrate{ 0.0f }; // mm/s
            Flags flags;
            FeedrateProfile feedrate_profile;
            Trapezoid trapezoid;

            // 计算此块的梯形
            void calculate_trapezoid();

            float time() const;
        };


    private:
        struct TimeMachine
        {
            struct State
            {
                float feedrate; // mm/s
                float safe_feedrate; // mm/s
                //BBS: X-Y-Z-E轴的进给率。但当移动为G2和G3时，X-Y将
                //相同，表示X-Y平面中的进给率。
                AxisCoords axis_feedrate; // mm/s
                AxisCoords abs_axis_feedrate; // mm/s

                //BBS: x-y-z空间中进入速度和退出速度的单位向量。
                //对于直线移动，它们相同。对于圆弧移动，它们不同。
                Vec3f enter_direction;
                Vec3f exit_direction;

                void reset();
            };

            struct CustomGCodeTime
            {
                bool needed;
                float cache;
                std::vector<std::pair<CustomGCode::Type, float>> times;

                void reset();
            };

            struct G1LinesCacheItem
            {
                unsigned int id;
                unsigned int remaining_internal_g1_lines{ 0 };
                float elapsed_time;
            };

            bool enabled;
            float acceleration; // mm/s^2
            // 固件将限制到的加速度硬限制。
            float max_acceleration; // mm/s^2
            float retract_acceleration; // mm/s^2
            // 固件将限制到的回抽加速度硬限制。
            float max_retract_acceleration; // mm/s^2
            float travel_acceleration; // mm/s^2
            // 固件将限制到的移动加速度硬限制。
            float max_travel_acceleration; // mm/s^2
            float extrude_factor_override_percentage;
            float time; // s
            struct StopTime
            {
                unsigned int g1_line_id;
                float elapsed_time;
            };
            std::vector<StopTime> stop_times;
            std::string line_m73_main_mask;
            std::string line_m73_stop_mask;
            State curr;
            State prev;
            CustomGCodeTime gcode_time;
            std::vector<TimeBlock> blocks;
            std::vector<G1LinesCacheItem> g1_times_cache;
            std::array<float, static_cast<size_t>(EMoveType::Count)> moves_time;
            std::array<float, static_cast<size_t>(ExtrusionRole::erCount)> roles_time;
            std::vector<float> layers_time;
            //BBS: 打印模型之前的准备阶段时间，包括开始gcode时间，大多与开始gcode时间相同
            float prepare_time;

            void reset();

            // 模拟固件st_synchronize()调用
            void simulate_st_synchronize(float additional_time = 0.0f);
            void calculate_time(size_t keep_last_n_blocks = 0, float additional_time = 0.0f);
        };

        struct TimeProcessor
        {
            struct Planner
            {
                // 固件规划器队列的大小。旧的8位Marlins通常只管理16个梯形块。
                // 让我们保守一点，为具有更多内存的新主板规划。
                static constexpr size_t queue_size = 64;
                // 每次添加新块时，固件重新计算最后planner_queue_size个梯形块。
                // 我们不精确模拟固件，而是当足够数量的块累积后计算一个块序列。
                static constexpr size_t refresh_threshold = queue_size * 4;
            };

            // extruder_id当前用于正确计算丝材加载/卸载时间到总打印时间中。
            // 这目前仅真正由MK3 MMU2使用：
            // extruder_unloaded = true表示尚未加载丝材，所有丝材都停放在MK3 MMU2单元中。
            bool extruder_unloaded;
            // 允许跳过GCode::print_machine_envelope()为非常规时间估计模式生成的行M201/M203/M204/M205
            bool machine_envelope_processing_enabled;
            MachineEnvelopeConfig machine_limits;
            // 丝材交换序列的额外加载/卸载时间。
            float filament_load_times;
            float filament_unload_times;
            //Orca: 工具更换时间
            float machine_tool_change_time;

            std::array<TimeMachine, static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Count)> machines;

            void reset();
        };

        struct UsedFilaments  // 每次颜色更改的丝材
        {
            double color_change_cache;
            std::vector<double> volumes_per_color_change;

            double model_extrude_cache;
            std::map<size_t, double> model_volumes_per_extruder;

            double wipe_tower_cache;
            std::map<size_t, double>wipe_tower_volumes_per_extruder;

            double support_volume_cache;
            std::map<size_t, double>support_volumes_per_extruder;

            //BBS: 每种丝材的冲洗量
            std::map<size_t, double> flush_per_filament;

            double total_volume_cache;
            std::map<size_t, double>total_volumes_per_extruder;

            double role_cache;
            std::map<ExtrusionRole, std::pair<double, double>> filaments_per_role;

            void reset();

            void increase_support_caches(double extruded_volume);
            void increase_model_caches(double extruded_volume);
            void increase_wipe_tower_caches(double extruded_volume);

            void process_color_change_cache();
            void process_model_cache(GCodeProcessor* processor);
            void process_wipe_tower_cache(GCodeProcessor* processor);
            void process_support_cache(GCodeProcessor* processor);
            void process_total_volume_cache(GCodeProcessor* processor);

            void update_flush_per_filament(size_t extrude_id, float flush_length);
            void process_role_cache(GCodeProcessor* processor);
            void process_caches(GCodeProcessor* processor);

            friend class GCodeProcessor;
        };

    public:
        class SeamsDetector
        {
            bool m_active{ false };
            std::optional<Vec3f> m_first_vertex;

        public:
            void activate(bool active) {
                if (m_active != active) {
                    m_active = active;
                    if (m_active)
                        m_first_vertex.reset();
                }
            }

            std::optional<Vec3f> get_first_vertex() const { return m_first_vertex; }
            void set_first_vertex(const Vec3f& vertex) { m_first_vertex = vertex; }

            bool is_active() const { return m_active; }
            bool has_first_vertex() const { return m_first_vertex.has_value(); }
        };

        // 用于修复颜色更改、暂停打印和自定义gcode标记的z的辅助类
        class OptionsZCorrector
        {
            GCodeProcessorResult& m_result;
            std::optional<size_t> m_move_id;
            std::optional<size_t> m_custom_gcode_per_print_z_id;

        public:
            explicit OptionsZCorrector(GCodeProcessorResult& result) : m_result(result) {
            }

            void set() {
                m_move_id = m_result.moves.size() - 1;
                m_custom_gcode_per_print_z_id = m_result.custom_gcode_per_print_z.size() - 1;
            }

            void update(float height) {
                if (!m_move_id.has_value() || !m_custom_gcode_per_print_z_id.has_value())
                    return;

                const Vec3f position = m_result.moves.back().position;

                GCodeProcessorResult::MoveVertex& move = m_result.moves.emplace_back(m_result.moves[*m_move_id]);
                move.position = position;
                move.height = height;
                m_result.moves.erase(m_result.moves.begin() + *m_move_id);
                m_result.custom_gcode_per_print_z[*m_custom_gcode_per_print_z_id].print_z = position.z();
                reset();
            }

            void reset() {
                m_move_id.reset();
                m_custom_gcode_per_print_z_id.reset();
            }
        };

#if ENABLE_GCODE_VIEWER_DATA_CHECKING
        struct DataChecker
        {
            struct Error
            {
                float value;
                float tag_value;
                ExtrusionRole role;
            };

            std::string type;
            float threshold{ 0.01f };
            float last_tag_value{ 0.0f };
            unsigned int count{ 0 };
            std::vector<Error> errors;

            DataChecker(const std::string& type, float threshold)
                : type(type), threshold(threshold)
            {}

            void update(float value, ExtrusionRole role) {
                if (role != erCustom) {
                    ++count;
                    if (last_tag_value != 0.0f) {
                        if (std::abs(value - last_tag_value) / last_tag_value > threshold)
                            errors.push_back({ value, last_tag_value, role });
                    }
                }
            }

            void reset() { last_tag_value = 0.0f; errors.clear(); count = 0; }

            std::pair<float, float> get_min() const {
                float delta_min = FLT_MAX;
                float perc_min = 0.0f;
                for (const Error& e : errors) {
                    if (delta_min > e.value - e.tag_value) {
                        delta_min = e.value - e.tag_value;
                        perc_min = 100.0f * delta_min / e.tag_value;
                    }
                }
                return { delta_min, perc_min };
            }

            std::pair<float, float> get_max() const {
                float delta_max = -FLT_MAX;
                float perc_max = 0.0f;
                for (const Error& e : errors) {
                    if (delta_max < e.value - e.tag_value) {
                        delta_max = e.value - e.tag_value;
                        perc_max = 100.0f * delta_max / e.tag_value;
                    }
                }
                return { delta_max, perc_max };
            }

            void output() const {
                if (!errors.empty()) {
                    std::cout << type << ":\n";
                    std::cout << "Errors: " << errors.size() << " (" << 100.0f * float(errors.size()) / float(count) << "%)\n";
                    auto [min, perc_min] = get_min();
                    auto [max, perc_max] = get_max();
                    std::cout << "min: " << min << "(" << perc_min << "%) - max: " << max << "(" << perc_max << "%)\n";
                }
            }
        };
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING

    private:
        GCodeReader m_parser;
        EUnits m_units;
        EPositioningType m_global_positioning_type;
        EPositioningType m_e_local_positioning_type;
        std::vector<Vec3f> m_extruder_offsets;
        GCodeFlavor m_flavor;
        float       m_nozzle_volume;
        AxisCoords m_start_position; // mm
        AxisCoords m_end_position; // mm
        AxisCoords m_origin; // mm
        CachedPosition m_cached_position;
        bool m_wiping;
        bool m_flushing;
        bool m_wipe_tower;
        float m_remaining_volume;
        bool m_manual_filament_change;

        //BBS: 生成的gcode的x, y偏移
        double          m_x_offset{ 0 };
        double          m_y_offset{ 0 };
        //BBS: 圆弧移动相关数据
        EMovePathType m_move_path_type{ EMovePathType::Noop_move };
        Vec3f m_arc_center{ Vec3f::Zero() };    // mm
        std::vector<Vec3f> m_interpolation_points;

        unsigned int m_line_id;
        unsigned int m_last_line_id;
        float m_feedrate; // mm/s
        float m_width; // mm
        float m_height; // mm
        float m_forced_width; // mm
        float m_forced_height; // mm
        float m_mm3_per_mm;
        float m_travel_dist; // mm
        float m_fan_speed; // 百分比
        float m_z_offset; // mm
        ExtrusionRole m_extrusion_role;
        unsigned char m_extruder_id;
        unsigned char m_last_extruder_id;
        ExtruderColors m_extruder_colors;
        ExtruderTemps m_extruder_temps;
        ExtruderTemps m_extruder_temps_config;
        ExtruderTemps m_extruder_temps_first_layer_config;
        bool  m_is_XL_printer = false;
        int m_highest_bed_temp;
        float m_extruded_last_z;
        float m_first_layer_height; // mm
        float m_zero_layer_height; // mm
        bool m_processing_start_custom_gcode;
        unsigned int m_g1_line_id;
        unsigned int m_layer_id;
        CpColor m_cp_color;
        SeamsDetector m_seams_detector;
        OptionsZCorrector m_options_z_corrector;
        size_t m_last_default_color_id;
        bool m_detect_layer_based_on_tag {false};
        int m_seams_count;
        bool m_single_extruder_multi_material;
        float m_preheat_time;
        int m_delta_temperature;
        int m_preheat_steps;
        bool m_disable_m73;
#if ENABLE_GCODE_VIEWER_STATISTICS
        std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;
#endif // ENABLE_GCODE_VIEWER_STATISTICS

        enum class EProducer
        {
            Unknown,
            Snapmaker_Orca,
            Slic3rPE,
            Slic3r,
            SuperSlicer,
            Cura,
            Simplify3D,
            CraftWare,
            ideaMaker,
            KissSlicer
        };

        static const std::vector<std::pair<GCodeProcessor::EProducer, std::string>> Producers;
        EProducer m_producer;

        DynamicConfig m_current_config;

        TimeProcessor m_time_processor;
        UsedFilaments m_used_filaments;

        Print* m_print{ nullptr };

        GCodeProcessorResult m_result;
        static unsigned int s_result_id;

#if ENABLE_GCODE_VIEWER_DATA_CHECKING
        DataChecker m_mm3_per_mm_compare{ "mm3_per_mm", 0.01f };
        DataChecker m_height_compare{ "height", 0.01f };
        DataChecker m_width_compare{ "width", 0.01f };
#endif // ENABLE_GCODE_VIEWER_DATA_CHECKING

    public:
        GCodeProcessor();

        void apply_config(const PrintConfig& config);
        void set_print(Print* print) { m_print = print; }
        void enable_stealth_time_estimator(bool enabled);
        bool is_stealth_time_estimator_enabled() const {
            return m_time_processor.machines[static_cast<size_t>(PrintEstimatedStatistics::ETimeMode::Stealth)].enabled;
        }
        void enable_machine_envelope_processing(bool enabled) { m_time_processor.machine_envelope_processing_enabled = enabled; }
        void reset();

        const GCodeProcessorResult& get_result() const { return m_result; }
        GCodeProcessorResult& result() { return m_result; }
        GCodeProcessorResult&& extract_result() { return std::move(m_result); }
        DynamicConfig&              current_dynamic_config() { return m_current_config; }


        GCodeReader& parser() { return m_parser; }
        // 将G-code加载到独立的G-code查看器中。
        // 通过print->throw_if_canceled()（由调用者作为回调发送）抛出CanceledException。
        void process_file(const std::string& filename, std::function<void()> cancel_callback = nullptr);

        // 流式接口，用于以流水线方式处理PrusaSlicer刚刚生成的G-codes。
        void initialize(const std::string& filename);
        void process_buffer(const std::string& buffer);
        void finalize(bool post_process);

        float get_time(PrintEstimatedStatistics::ETimeMode mode) const;
        float get_prepare_time(PrintEstimatedStatistics::ETimeMode mode) const;
        std::string get_time_dhm(PrintEstimatedStatistics::ETimeMode mode) const;
        std::vector<std::pair<CustomGCode::Type, std::pair<float, float>>> get_custom_gcode_times(PrintEstimatedStatistics::ETimeMode mode, bool include_remaining) const;

        std::vector<std::pair<EMoveType, float>> get_moves_time(PrintEstimatedStatistics::ETimeMode mode) const;
        std::vector<std::pair<ExtrusionRole, float>> get_roles_time(PrintEstimatedStatistics::ETimeMode mode) const;
        std::vector<float> get_layers_time(PrintEstimatedStatistics::ETimeMode mode) const;

        //BBS: 为gcode写入器设置偏移
        void set_xy_offset(double x, double y) { m_x_offset = x; m_y_offset = y; }

        // Orca: 如果为true，仅在出现ETags::Layer_Change时更改新层
        // 否则当在挤出过程中抬升z时，将添加新层
        void detect_layer_based_on_tag(bool enabled) { m_detect_layer_based_on_tag = enabled; }

    private:
        void apply_config(const DynamicPrintConfig& config);
        void apply_config_simplify3d(const std::string& filename);
        void apply_config_superslicer(const std::string& filename);
        void process_gcode_line(const GCodeReader::GCodeLine& line, bool producers_enabled);

        // 处理嵌入到注释中的标签
        void process_tags(const std::string_view comment, bool producers_enabled);
        bool process_producers_tags(const std::string_view comment);
        bool process_bambuslicer_tags(const std::string_view comment);
        bool process_cura_tags(const std::string_view comment);
        bool process_simplify3d_tags(const std::string_view comment);
        bool process_craftware_tags(const std::string_view comment);
        bool process_ideamaker_tags(const std::string_view comment);
        bool process_kissslicer_tags(const std::string_view comment);

        bool detect_producer(const std::string_view comment);

        // 移动
        void process_G0(const GCodeReader::GCodeLine& line);
        void process_G1(const GCodeReader::GCodeLine& line, const std::optional<unsigned int>& remaining_internal_g1_lines = std::nullopt);
        void process_G2_G3(const GCodeReader::GCodeLine& line);

        // BBS: 处理延迟命令
        void process_G4(const GCodeReader::GCodeLine& line);

        // 回抽
        void process_G10(const GCodeReader::GCodeLine& line);

        // 取消回抽
        void process_G11(const GCodeReader::GCodeLine& line);

        // 设置单位为英寸
        void process_G20(const GCodeReader::GCodeLine& line);

        // 设置单位为毫米
        void process_G21(const GCodeReader::GCodeLine& line);

        // 固件控制回抽
        void process_G22(const GCodeReader::GCodeLine& line);

        // 固件控制取消回抽
        void process_G23(const GCodeReader::GCodeLine& line);

        // 移动到原点
        void process_G28(const GCodeReader::GCodeLine& line);

        // BBS
        void process_G29(const GCodeReader::GCodeLine& line);

        // 设置为绝对定位
        void process_G90(const GCodeReader::GCodeLine& line);

        // 设置为相对定位
        void process_G91(const GCodeReader::GCodeLine& line);

        // 设置位置
        void process_G92(const GCodeReader::GCodeLine& line);

        // 睡眠或有条件停止
        void process_M1(const GCodeReader::GCodeLine& line);

        // 设置挤出机为绝对模式
        void process_M82(const GCodeReader::GCodeLine& line);

        // 设置挤出机为相对模式
        void process_M83(const GCodeReader::GCodeLine& line);

        // 设置挤出机温度
        void process_M104(const GCodeReader::GCodeLine& line);

        // 设置风扇速度
        void process_M106(const GCodeReader::GCodeLine& line);

        // 禁用风扇
        void process_M107(const GCodeReader::GCodeLine& line);

        // 设置工具（Sailfish）
        void process_M108(const GCodeReader::GCodeLine& line);

        // 设置挤出机温度并等待
        void process_M109(const GCodeReader::GCodeLine& line);

        // 恢复存储的主页偏移
        void process_M132(const GCodeReader::GCodeLine& line);

        // 设置工具（MakerWare）
        void process_M135(const GCodeReader::GCodeLine& line);

        //BBS: 设置热床温度
        void process_M140(const GCodeReader::GCodeLine& line);

        //BBS: 等待热床温度
        void process_M190(const GCodeReader::GCodeLine& line);

        //BBS: 等待腔室温度
        void process_M191(const GCodeReader::GCodeLine& line);

        // 设置最大打印加速度
        void process_M201(const GCodeReader::GCodeLine& line);

        // 设置最大进给率
        void process_M203(const GCodeReader::GCodeLine& line);

        // 设置默认加速度
        void process_M204(const GCodeReader::GCodeLine& line);

        // 高级设置
        void process_M205(const GCodeReader::GCodeLine& line);

        // Klipper SET_VELOCITY_LIMIT
        void process_SET_VELOCITY_LIMIT(const GCodeReader::GCodeLine& line);

        // 设置挤出因子覆盖百分比
        void process_M221(const GCodeReader::GCodeLine& line);

        // BBS: 处理延迟命令。M400仅由BBL定义
        void process_M400(const GCodeReader::GCodeLine& line);

        // Repetier: 存储x, y和z位置
        void process_M401(const GCodeReader::GCodeLine& line);

        // Repetier: 转到存储的位置
        void process_M402(const GCodeReader::GCodeLine& line);

        // 设置允许的瞬时速度变化
        void process_M566(const GCodeReader::GCodeLine& line);

        // 在打印结束时将当前丝材卸载到MK3 MMU2单元中。
        void process_M702(const GCodeReader::GCodeLine& line);

        // 处理T行（选择工具）
        void process_T(const GCodeReader::GCodeLine& line);
        void process_T(const std::string_view command);

        // 对具有给定文件名的文件进行后处理以：
        // 1) 添加剩余时间行M73并相应地更新移动的gcode id
        // 2) 更新已使用的丝材数据
        void run_post_process();

        //BBS: 不同的path_type仅用于圆弧移动
        void store_move_vertex(EMoveType type, EMovePathType path_type = EMovePathType::Noop_move);

        void set_extrusion_role(ExtrusionRole role);

        float minimum_feedrate(PrintEstimatedStatistics::ETimeMode mode, float feedrate) const;
        float minimum_travel_feedrate(PrintEstimatedStatistics::ETimeMode mode, float feedrate) const;
        float get_axis_max_feedrate(PrintEstimatedStatistics::ETimeMode mode, Axis axis) const;
        float get_axis_max_acceleration(PrintEstimatedStatistics::ETimeMode mode, Axis axis) const;
        float get_axis_max_jerk(PrintEstimatedStatistics::ETimeMode mode, Axis axis) const;
        Vec3f get_xyz_max_jerk(PrintEstimatedStatistics::ETimeMode mode) const;
        float get_retract_acceleration(PrintEstimatedStatistics::ETimeMode mode) const;
        void  set_retract_acceleration(PrintEstimatedStatistics::ETimeMode mode, float value);
    float get_acceleration(PrintEstimatedStatistics::ETimeMode mode) const;
        void  set_acceleration(PrintEstimatedStatistics::ETimeMode mode, float value);
        float get_travel_acceleration(PrintEstimatedStatistics::ETimeMode mode) const;
        void  set_travel_acceleration(PrintEstimatedStatistics::ETimeMode mode, float value);
        float get_filament_load_time(size_t extruder_id);
        float get_filament_unload_time(size_t extruder_id);
        int   get_filament_vitrification_temperature(size_t extrude_id);
        void process_custom_gcode_time(CustomGCode::Type code);
        void process_filaments(CustomGCode::Type code);

        // 模拟固件st_synchronize()调用
        void simulate_st_synchronize(float additional_time = 0.0f);

        void update_estimated_times_stats();
        //BBS:
        void update_slice_warnings();
   };

} /* namespace Slic3r */

#endif /* slic3r_GCodeProcessor_hpp_ */
