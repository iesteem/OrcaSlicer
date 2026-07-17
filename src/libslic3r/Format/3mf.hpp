#ifndef slic3r_Format_3mf_hpp_
#define slic3r_Format_3mf_hpp_
#include <expat.h>

namespace Slic3r {
// PrusaFileParser 用于检查3mf文件是否来自Prusa
class PrusaFileParser
{
public:
    PrusaFileParser() {}
    ~PrusaFileParser() {}

    bool check_3mf_from_prusa(const std::string filename);
    void _start_element_handler(const char *name, const char **attributes);
    void _characters_handler(const XML_Char *s, int len);

private:
    const char *get_attribute_value_charptr(const char **attributes, unsigned int attributes_size, const char *attribute_key);
    std::string get_attribute_value_string(const char **attributes, unsigned int attributes_size, const char *attribute_key);

    static void XMLCALL start_element_handler(void *userData, const char *name, const char **attributes);
    static void XMLCALL characters_handler(void *userData, const XML_Char *s, int len);
private:
    bool       m_from_prusa         = false;
    bool       m_is_application_key = false;
    XML_Parser m_parser;
};

    /* 保存SLA点的格式在过去有过变化。此枚举保存当前使用的最新版本。
     * 历史上使用的Slic3r_PE_sla_support_points.txt版本示例：

     *  版本0 : object_id=1|-12.055421 -2.658771 10.000000
                    object_id=2|-14.051745 -3.570338 5.000000
        // 无头部，只有点的x,y,z位置)

     * 版本1 :  ThreeMF_support_points_version=1
                    object_id=1|-12.055421 -2.658771 10.000000 0.4 0.0
                    object_id=2|-14.051745 -3.570338 5.000000 0.6 1.0
        // 引入了带版本号的头部；x,y,z,head_size,is_new_island)
    */

    enum {
        support_points_format_version = 1
    };
    
    enum {
        drain_holes_format_version = 1
    };

    class Model;
    struct ConfigSubstitutionContext;
    class DynamicPrintConfig;
    struct ThumbnailData;

    // 将3mf文件的内容加载到给定的模型和预设包中。
    extern bool load_3mf(const char* path, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions, Model* model, bool check_version);

    // 将给定的模型和包含在给定Print中的配置数据保存到3mf文件中。
    // 在导出过程中，如果网格未修复或没有共享顶点，模型可能会被修改。
    extern bool store_3mf(const char* path, Model* model, const DynamicPrintConfig* config, bool fullpath_sources, const ThumbnailData* thumbnail_data = nullptr, bool zip64 = true);

} // namespace Slic3r

#endif /* slic3r_Format_3mf_hpp_ */
