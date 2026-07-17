#ifndef slic3r_Emboss_hpp_
#define slic3r_Emboss_hpp_

#include <vector>
#include <set>
#include <optional>
#include <memory>
#include <admesh/stl.h> // indexed_triangle_set
#include "Polygon.hpp"
#include "ExPolygon.hpp"
#include "EmbossShape.hpp" // ExPolygonsWithIds
#include "BoundingBox.hpp"
#include "TextConfiguration.hpp"

namespace Slic3r {

/// <summary>
/// 仅包含静态函数的类，提供将雕刻或凸起文本或多边形添加到模型表面的能力
/// </summary>
namespace Emboss
{
    static const float UNION_DELTA = 50.0f; // [近似纳秒，取决于体积比例]
    static const unsigned UNION_MAX_ITERATIN = 10; // [次数]

    /// <summary>
    /// 收集操作系统中注册的字体
    /// </summary>
    /// <returns>操作系统中注册的TTF字体文件（完整路径）及名称</returns>
    EmbossStyles get_font_list();
#ifdef _WIN32
    EmbossStyles get_font_list_by_register();
    EmbossStyles get_font_list_by_enumeration();
    EmbossStyles get_font_list_by_folder();
#endif

    /// <summary>
    /// 依赖于操作系统的函数，根据字体名称描述符获取字体位置
    /// </summary>
    /// <param name="font_face_name">字体的唯一标识符</param>
    /// <returns>找到字体时的文件路径</returns>
    std::optional<std::wstring> get_font_path(const std::wstring &font_face_name);

    // 单个字母的描述
    struct Glyph
    {
        // 注意：形状按 SHAPE_SCALE 缩放
        // 以便能够在不使用浮点数的情况下存储点
        ExPolygons shape;

        // 值以字体点为单位
        int advance_width=0, left_side_bearing=0;
    };
    // 按unicode缓存的字形
    using Glyphs = std::map<int, Glyph>;
        
    /// <summary>
    /// 从文件中保留字体的信息
    /// （存储文件数据本身）
    /// + 从缓冲区读取的缓存数据
    /// </summary>
    struct FontFile
    {
        // 从字体文件加载的数据
        // 必须为imgui光栅化存储数据大小
        // 为避免在堆上存储数据并防止不必要的复制
        // 数据存储在unique_ptr中
        std::unique_ptr<std::vector<unsigned char>> data;

        struct Info
        {
            // 垂直位置为"scale*(ascent - descent + lineGap)"
            int ascent, descent, linegap;

            // 用于将字体单位转换为像素
            int unit_per_em;
        };
        // 数据中每个字体的信息
        std::vector<Info> infos;

        FontFile(std::unique_ptr<std::vector<unsigned char>> data,
                 std::vector<Info>                         &&infos)
            : data(std::move(data)), infos(std::move(infos))
        {
            assert(this->data != nullptr);
            assert(!this->data->empty());
        }

        bool operator==(const FontFile &other) const {
            if (data->size() != other.data->size())
                return false;
            //if(*data != *other.data) return false;
            for (size_t i = 0; i < infos.size(); i++) 
                if (infos[i].ascent != other.infos[i].ascent ||
                    infos[i].descent == other.infos[i].descent ||
                    infos[i].linegap == other.infos[i].linegap)
                    return false;
            return true;
        }
    };

    /// <summary>
    /// 为字形形状添加缓存
    /// </summary>
    struct FontFileWithCache
    {
        // 指向字体文件数据的指针
        std::shared_ptr<const FontFile> font_file;

        // 字形形状的缓存
        // 重要：仅可在 plater 作业线程中访问 !!!
        // 主线程仅通过设置为另一个 shared_ptr 来清除缓存
        std::shared_ptr<Emboss::Glyphs> cache;

        FontFileWithCache() : font_file(nullptr), cache(nullptr) {}
        explicit FontFileWithCache(std::unique_ptr<FontFile> font_file)
            : font_file(std::move(font_file))
            , cache(std::make_shared<Emboss::Glyphs>())
        {}
        bool has_value() const { return font_file != nullptr && cache != nullptr; }
    };

    /// <summary>
    /// 将字体文件加载到缓冲区
    /// </summary>
    /// <param name="file_path">.ttf 或 .ttc 字体文件的位置</param>
    /// <returns>加载后的字体对象。</returns>
    std::unique_ptr<FontFile> create_font_file(const char *file_path);
    // data = 原始文件数据
    std::unique_ptr<FontFile> create_font_file(std::unique_ptr<std::vector<unsigned char>> data);
#ifdef _WIN32
    // 修复未知指针 HFONT 替换为 "void *"
    void * can_load(void* hfont);
    std::unique_ptr<FontFile> create_font_file(void * hfont);
#endif // _WIN32

    /// <summary>
    /// 将字母转换为多边形
    /// </summary>
    /// <param name="font">定义字体</param>
    /// <param name="font_index">字体在集合中的索引</param>
    /// <param name="letter">由unicode码点定义的单个字符</param>
    /// <param name="flatness">字母轮廓曲线转换为线段时的精度</param>
    /// <returns>内部多边形顺时针（外部逆时针）</returns>
    std::optional<Glyph> letter2glyph(const FontFile &font, unsigned int font_index, int letter, float flatness);

    /// <summary>
    /// 将文本转换为多边形
    /// </summary>
    /// <param name="font">定义字体 + 缓存，可扩展</param>
    /// <param name="text">要转换的字符</param>
    /// <param name="font_prop">用户定义的字体属性</param>
    /// <param name="was_canceled">中断处理的方式</param>
    /// <returns>内部多边形顺时针（外部逆时针）</returns>
    HealedExPolygons  text2shapes (FontFileWithCache &font, const char *text,         const FontProp &font_prop, const std::function<bool()> &was_canceled = []() {return false;});
    ExPolygonsWithIds text2vshapes(FontFileWithCache &font, const std::wstring& text, const FontProp &font_prop, const std::function<bool()>& was_canceled = []() {return false;});

    const unsigned ENTER_UNICODE = static_cast<unsigned>('\n');
    /// 字符 '\n' 的和
    unsigned get_count_lines(const std::wstring &ws);
    unsigned get_count_lines(const std::string &text);
    unsigned get_count_lines(const ExPolygonsWithIds &shape);

    /// <summary>
    /// 修复多边形中的重复点和自相交。
    /// 同时尝试减少点的数量并移除无用的多边形部分
    /// </summary>
    /// <param name="is_non_zero">填充类型 ClipperLib::pftNonZero 用于重叠，否则使用其他</param>
    /// <param name="max_iteration">参见 heal_expolygon()::max_iteration</param>
    /// <returns>修复后的形状及完全修复的标志</returns>
    HealedExPolygons heal_polygons(const Polygons &shape, bool is_non_zero = true, unsigned max_iteration = 10);

    /// <summary>
    /// 注意：在调用此函数之前先调用 Slic3r::union_ex
    ///
    /// 修复 expolygons 中的问题：
    ///  - 自相交
    ///  - 重复点
    ///  - 靠近线段的点
    /// </summary>
    /// <param name="shape">输入/输出 要修复的形状</param>
    /// <param name="max_iteration">修复可能会产生另一个问题，
    /// 修复后会再次检查，直到形状良好或达到最大迭代次数</param>
    /// <returns>形状良好时返回True，否则返回False</returns>
    bool heal_expolygons(ExPolygons &shape, unsigned max_iteration = 10);

    /// <summary>
    /// 在靠近点的位置就地分割线段
    /// （由于精度问题可能导致自相交）
    /// 移除相同的相邻点
    /// 注意：可能是修复形状的一部分
    /// </summary>
    /// <param name="expolygons">要编辑的 Expolygon</param>
    /// <param name="distance">（epsilon）从点到分割线的欧几里得距离</param>
    /// <returns>当进行了某些分割时返回True，否则返回false</returns>
    bool divide_segments_for_close_point(ExPolygons &expolygons, double distance);

    /// <summary>
    /// 使用字体属性中的数据修改变换
    /// </summary>
    /// <param name="angle">相对于Y轴的Z旋转角度</param>
    /// <param name="distance">作为表面距离的Z移动</param>
    /// <param name="transformation">输入/输出 要由属性修改的变换</param>
    void apply_transformation(const std::optional<float> &angle, const std::optional<float> &distance, Transform3d &transformation);

    /// <summary>
    /// 从字体文件的命名表中读取信息
    /// 搜索斜体（或倾斜）、粗体斜体（或粗体倾斜）
    /// </summary>
    /// <param name="font">字体选择器</param>
    /// <param name="font_index">字体在集合中的索引</param>
    /// <returns>当字体描述包含斜体/倾斜时返回True，否则返回False</returns>
    bool is_italic(const FontFile &font, unsigned int font_index);

    /// <summary>
    /// 从字符串中创建唯一字符集，仅过滤出字体中包含的字符
    /// </summary>
    /// <param name="text">字形的源向量</param>
    /// <param name="font">字体描述符</param>
    /// <param name="font_index">定义字体在集合中的位置</param>
    /// <param name="exist_unknown">当文本包含字体中未知的字形时返回True</param>
    /// <returns>文本中包含在字体中的唯一字符集</returns>
    std::string create_range_text(const std::string &text, const FontFile &font, unsigned int font_index, bool* exist_unknown = nullptr);

    /// <summary>
    /// 计算字形形状从形状点转换为毫米的比例
    /// </summary>
    /// <param name="fp">字体属性</param>
    /// <param name="ff">字体数据</param>
    /// <returns>转换为毫米</returns>
    double get_text_shape_scale(const FontProp &fp, const FontFile &ff);

    /// <summary>
    /// 根据 prop 中定义的集合获取字体信息的 getter
    /// </summary>
    /// <param name="font">包含文件中所有字体（集合）的信息</param>
    /// <param name="prop">集合的索引</param>
    /// <returns>上升、下降、行间距</returns>
    const FontFile::Info &get_font_info(const FontFile &font, const FontProp &prop);

    /// <summary>
    /// 从字体文件和属性中读取带有间距的行高
    /// </summary>
    /// <param name="font">集合的信息</param>
    /// <param name="prop">集合索引 + 附加行间距</param>
    /// <returns>带有间距的行高，以缩放后的字体点为单位（与 ExPolygons 相同）</returns>
    int get_line_height(const FontFile &font, const FontProp &prop);

    /// <summary>
    /// 计算垂直对齐
    /// </summary>
    /// <param name="align">顶部 | 居中 | 底部</param>
    /// <param name="count_lines">行数</param>
    /// <returns>返回以毫米为单位的Y对齐偏移量</returns>
    double get_align_y_offset_in_mm(FontProp::VerticalAlign align, unsigned count_lines, const FontFile &ff, const FontProp &fp);

    /// <summary>
    /// 投影空间点
    /// </summary>
    class IProject3d
    {
    public:
        virtual ~IProject3d() = default;
        /// <summary>
        /// 根据投影方向移动点
        /// 例如：正交投影将沿方向移动点
        /// 例如：球面投影需要使用投影中心
        /// </summary>
        /// <param name="point">空间点坐标</param>
        /// <returns>投影后的空间点</returns>
        virtual Vec3d project(const Vec3d &point) const = 0;
    };

    /// <summary>
    /// 将2D点投影到空间
    /// 可以是平面、球面、柱面等
    /// </summary>
    class IProjection : public IProject3d
    {
    public:
        virtual ~IProjection() = default;

        /// <summary>
        /// 将2D点转换为3D点
        /// </summary>
        /// <param name="p">2D坐标</param>
        /// <returns>
        /// first - 前向空间点
        /// second - 后向空间点
        /// </returns>
        virtual std::pair<Vec3d, Vec3d> create_front_back(const Point &p) const = 0;

        /// <summary>
        /// 反向投影
        /// </summary>
        /// <param name="p">要投影的点</param>
        /// <param name="depth">[可选] 2D投影点的深度。注意数值在2D比例下</param>
        /// <returns>可能时返回反向投影的点</returns>
        virtual std::optional<Vec2d> unproject(const Vec3d &p, double * depth = nullptr) const = 0;
    };

    /// <summary>
    /// 为文本创建三角形模型
    /// </summary>
    /// <param name="shape2d">文本或图像</param>
    /// <param name="projection">定义从2D到3D的变换（方向、位置、比例等）</param>
    /// <returns>投影到空间的形状</returns>
    indexed_triangle_set polygons2model(const ExPolygons &shape2d, const IProjection& projection);
    
    /// <summary>
    /// 根据浮雕方向建议浮雕文本所需的上方向向量
    /// </summary>
    /// <param name="normal">世界中浮雕方向的归一化向量</param>
    /// <param name="up_limit">与 normal.z 比较以建议上方向</param>
    /// <returns>所需的上方向向量</returns>
    Vec3d suggest_up(const Vec3d normal, double up_limit = 0.9);

    /// <summary>
    /// 通过变换计算建议上向量与实际上向量之间的角度
    /// </summary>
    /// <param name="tr">世界中浮雕体积的变换</param>
    /// <param name="up_limit">与 normal.z 比较以建议上方向</param>
    /// <returns>建议上向量的旋转[弧度]在范围[-Pi, Pi]内，当旋转不为零时</returns>
    std::optional<float> calc_up(const Transform3d &tr, double up_limit = 0.9);

    /// <summary>
    /// 为浮雕文本对象创建变换以放置在表面点上
    /// </summary>
    /// <param name="position">表面点的位置</param>
    /// <param name="normal">表面点的法线</param>
    /// <param name="up_limit">与 normal.z 比较以建议上方向</param>
    /// <returns>到表面点的变换</returns>
    Transform3d create_transformation_onto_surface(
        const Vec3d &position, const Vec3d &normal, double up_limit = 0.9);

    class ProjectZ : public IProjection
    {
    public:
        explicit ProjectZ(double depth) : m_depth(depth) {}
        // 继承自 IProject
        std::pair<Vec3d, Vec3d> create_front_back(const Point &p) const override;
        Vec3d project(const Vec3d &point) const override;
        std::optional<Vec2d> unproject(const Vec3d &p, double * depth = nullptr) const override;
        double m_depth;
    };

    class ProjectScale : public IProjection
    {
        std::unique_ptr<IProjection> core;
        double m_scale;
    public:
        ProjectScale(std::unique_ptr<IProjection> core, double scale)
            : core(std::move(core)), m_scale(scale)
        {}

        // 继承自 IProject
        std::pair<Vec3d, Vec3d> create_front_back(const Point &p) const override
        {
            auto res = core->create_front_back(p);
            return std::make_pair(res.first * m_scale, res.second * m_scale);
        }
        Vec3d project(const Vec3d &point) const override{
            return core->project(point);
        }
        std::optional<Vec2d> unproject(const Vec3d &p, double *depth = nullptr) const override {
            auto res = core->unproject(p / m_scale, depth);
            if (depth != nullptr) *depth *= m_scale;
            return res;
        }
    };

    class ProjectTransform : public IProjection
    {
        std::unique_ptr<IProjection> m_core;
        Transform3d m_tr;
        Transform3d m_tr_inv;
        double z_scale;
    public:
        ProjectTransform(std::unique_ptr<IProjection> core, const Transform3d &tr) : m_core(std::move(core)), m_tr(tr)
        {
            m_tr_inv = m_tr.inverse();
            z_scale  = (m_tr.linear() * Vec3d::UnitZ()).norm();
        }

        // 继承自 IProject
        std::pair<Vec3d, Vec3d> create_front_back(const Point &p) const override
        {
            auto [front, back] = m_core->create_front_back(p);
            return std::make_pair(m_tr * front, m_tr * back);
        }
        Vec3d project(const Vec3d &point) const override{
            return m_core->project(point);
        }
        std::optional<Vec2d> unproject(const Vec3d &p, double *depth = nullptr) const override {
            auto res = m_core->unproject(m_tr_inv * p, depth);
            if (depth != nullptr)
                *depth *= z_scale;
            return res;
        }
    };

    class OrthoProject3d : public Emboss::IProject3d
    {
        // 正交投影的浮雕大小和方向
        Vec3d m_direction;
    public:
        OrthoProject3d(Vec3d direction) : m_direction(direction) {}
        Vec3d project(const Vec3d &point) const override{ return point + m_direction;}
    };

    class OrthoProject: public Emboss::IProjection {
        Transform3d m_matrix;
        // 正交投影的浮雕大小和方向
        Vec3d       m_direction;
        Transform3d m_matrix_inv;
    public:
        OrthoProject(Transform3d matrix, Vec3d direction)
            : m_matrix(matrix), m_direction(direction), m_matrix_inv(matrix.inverse())
        {}
        // 继承自 IProject
        std::pair<Vec3d, Vec3d> create_front_back(const Point &p) const override;
        Vec3d project(const Vec3d &point) const override;
        std::optional<Vec2d> unproject(const Vec3d &p, double * depth = nullptr) const override;
    };

    /// <summary>
    /// 定义用于绘制字母的多边形
    /// </summary>
    struct TextLine
    {
        // 对象的切片
        Polygon polygon;

        // 多边形上最接近零点的点
        PolygonPoint start;

        // 文本行在体积中的偏移量（毫米）
        float y;
    };
    using TextLines = std::vector<TextLine>;

    /// <summary>
    /// 通过边界框中心对切片多边形进行采样
    /// 切片起始点具有 shape_center_x 坐标
    /// </summary>
    /// <param name="slice">多边形和起始点 [Slic3r 缩放毫米]</param>
    /// <param name="bbs">一行上字母的边界框 [以字体比例表示]</param>
    /// <param name="scale">bbs 的比例（相乘后 bb 以毫米为单位）</param>
    /// <returns>按边界框采样的多边形</returns>
    PolygonPoints sample_slice(const TextLine &slice, const BoundingBoxes &bbs, double scale);

    /// <summary>
    /// 计算多边形点的角度
    /// </summary>
    /// <param name="distance">在点处找到法线的距离</param>
    /// <param name="polygon_point">选择多边形上的点</param>
    /// <param name="polygon">多边形知道点的邻居</param>
    /// <returns>多边形点处法线的角度(atan2)</returns>
    double calculate_angle(int32_t distance, PolygonPoint polygon_point, const Polygon &polygon);
    std::vector<double> calculate_angles(int32_t distance, const PolygonPoints& polygon_points, const Polygon &polygon);

} // namespace Emboss

///////////////////////
// 移动到 ExPolygonsWithIds 工具函数
void translate(ExPolygonsWithIds &e, const Point &p);
BoundingBox get_extents(const ExPolygonsWithIds &e);
void center(ExPolygonsWithIds &e);
// delta .. 合并前的安全偏移（用作布尔闭合）
// 注意：移除相邻曲线之间不可打印的间隙（由曲线线性化产生）
ExPolygons union_with_delta(EmbossShape &shape, float delta, unsigned max_heal_iteration);
} // namespace Slic3r
#endif // slic3r_Emboss_hpp_
