#ifndef slic3r_Format_AMF_hpp_
#define slic3r_Format_AMF_hpp_

namespace Slic3r {

class Model;
class DynamicPrintConfig;

// 将amf文件的内容加载到给定的模型和配置中。
extern bool load_amf(const char* path, DynamicPrintConfig* config, ConfigSubstitutionContext* config_substitutions, Model* model, bool* use_inches);

//BBS: 移除amf导出
// 将给定的模型和配置数据保存到amf文件中。
// 在导出过程中，如果网格未修复或没有共享顶点，模型可能会被修改。
//extern bool store_amf(const char* path, Model* model, const DynamicPrintConfig* config, bool fullpath_sources);

} // namespace Slic3r

#endif /* slic3r_Format_AMF_hpp_ */
