#include <string>

namespace Slic3r {
    /**
     * 使用ModelIO将支持的模型类型转换为临时STL文件，
     * 然后可以由现有的STL加载器使用
     * @param input_file 要加载的文件
     * @return 临时文件的路径，如果转换失败则返回空字符串
     */
    std::string make_temp_stl_with_modelio(const std::string &input_file);

    /**
     * 删除文件的便捷函数。
     * 不需要返回值，因为成功与否并非必需
     * @param temp_file 要删除的文件路径
     */
    void delete_temp_file(const std::string &temp_file);
}

