#ifndef slic3r_TextConfiguration_hpp_
#define slic3r_TextConfiguration_hpp_

#include <vector>
#include <string>
#include <optional>
#include <cereal/cereal.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/archives/binary.hpp>
#include "Point.hpp" // Transform3d

namespace Slic3r {

/// <summary>
/// 用户可修改的文本样式属性
/// 注意: OnEdit修复序列化: EmbossStylesSerializable, TextConfigurationSerialization
/// </summary>
struct FontProp
{
    // 定义字母之间的额外间距，负值表示字母更紧密
    // 未设置时值为零且不存储
    std::optional<int> char_gap; // [in font point]

    // 定义行之间的额外间距，负值表示行更紧密
    // 未设置时值为零且不存储
    std::optional<int> line_gap; // [in font point]

    // 正值表示更宽的字符形状
    // 负值表示更细的字符形状
    // 未设置时值为零且不存储
    std::optional<float> boldness; // [in mm]

    // 正值表示字符斜体（顺时针）
    // 负值表示逆时针倾斜（非斜体）
    // 未设置时值为零且不存储
    std::optional<float> skew; // [ration x:y]

    // True Type字体集合的参数
    // 选择集合中的字体索引
    std::optional<unsigned int> collection_number;

    // 区分每个字形的投影
    bool per_glyph;

    // 注意: 序列化到3mf的方式强制零必须为默认值
    enum class HorizontalAlign { left = 0, center, right };
    enum class VerticalAlign { top = 0, center, bottom };
    using Align = std::pair<HorizontalAlign, VerticalAlign>;
    // 更改文本的枢轴
    // 未设置时，使用居中且不存储
    Align align = Align(HorizontalAlign::center, VerticalAlign::center);

    //////
    // 重复数据到wxFontDescriptor
    // 用于存储/加载.3mf文件
    //////

    // 文本行高度（字母）
    // 与wxFont::PointSize重复
    float size_in_mm; // [in mm]

    // 关于字体的附加数据，以便在未安装相同字体时能够找到替代字体
    std::optional<std::string> family;
    std::optional<std::string> face_name;
    std::optional<std::string> style;
    std::optional<std::string> weight;

    /// <summary>
    /// 唯一构造函数，带有限制值
    /// </summary>
    /// <param name="line_height">文本的Y尺寸 [单位mm]</param>
    /// <param name="depth">文本的Z尺寸 [单位mm]</param>
    FontProp(float line_height = 10.f) : size_in_mm(line_height), per_glyph(false)
    {}

    bool operator==(const FontProp& other) const {
        return 
            char_gap == other.char_gap && 
            line_gap == other.line_gap &&
            per_glyph == other.per_glyph &&
            align == other.align &&
            is_approx(size_in_mm, other.size_in_mm) && 
            is_approx(boldness, other.boldness) &&
            is_approx(skew, other.skew);
    }

    // undo / redo stack recovery
    template<class Archive> void save(Archive &ar) const
    {
        ar(size_in_mm, per_glyph, align.first, align.second);
        cereal::save(ar, char_gap);
        cereal::save(ar, line_gap);
        cereal::save(ar, boldness);
        cereal::save(ar, skew);
        cereal::save(ar, collection_number);
    }
    template<class Archive> void load(Archive &ar)
    {
        ar(size_in_mm, per_glyph, align.first, align.second);
        cereal::load(ar, char_gap);
        cereal::load(ar, line_gap);
        cereal::load(ar, boldness);
        cereal::load(ar, skew);
        cereal::load(ar, collection_number);
    }
};

/// <summary>
/// 浮雕文本的样式
/// (Path + Type) 必须定义如何在不同的操作系统上打开字体
/// 注意: OnEdit修复序列化: EmbossStylesSerializable, TextConfigurationSerialization
/// </summary>
struct EmbossStyle
{
    // 人类可读的样式名称，显示在GUI中
    std::string name;

    // 定义如何打开字体
    // 含义取决于类型
    std::string path;

    enum class Type;
    // 定义存储在路径中的内容
    Type type { Type::undefined };

    // 用户对字体样式的修改
    FontProp prop;

    // 当名称为空时，字体项是从.3mf文件加载的
    // 并且可能无法重现
    // 定义存储在路径中的数据
    // 当wx更改存储方式时，添加新的描述符Type
    enum class Type { 
        undefined = 0,

        // wx字体描述符是平台相关的
        // path是由wxWidgets生成的字体描述符
        wx_win_font_descr, // on Windows 
        wx_lin_font_descr, // on Linux
        wx_mac_font_descr, // on Max OS

        // 计算机上的TrueType字体文件位置
        // 为隐私考虑：只有文件名存储到.3mf中
        file_path
    };

    bool operator==(const EmbossStyle &other) const
    {
        return 
            type == other.type &&
            prop == other.prop &&
            name == other.name &&
            path == other.path
            ;
    }

    // undo / redo stack recovery
    template<class Archive> void serialize(Archive &ar){ ar(name, path, type, prop); }
};

// 向量中的浮雕样式名称是唯一的
// 它不是映射，因为项目有自己的顺序（选择内的视图）
// 它通过EmbossStylesSerializable存储到AppConfig中
using EmbossStyles = std::vector<EmbossStyle>;

/// <summary>
/// 定义如何创建'文本体积'
/// 通过TextConfigurationSerialization存储到.3mf
/// 它是ModelVolume可选数据的一部分
/// </summary>
struct TextConfiguration
{
    // 浮雕文本的样式
    EmbossStyle style;

    // 浮雕文本的值
    std::string text = "None";

    // undo / redo stack recovery
    template<class Archive> void serialize(Archive &ar) { ar(style, text); }
};    

} // namespace Slic3r

#endif // slic3r_TextConfiguration_hpp_
