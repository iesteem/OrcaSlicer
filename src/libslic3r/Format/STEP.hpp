#ifndef slic3r_Format_STEP_hpp_
#define slic3r_Format_STEP_hpp_
#include "XCAFDoc_DocumentTool.hxx"
#include "XCAFApp_Application.hxx"
#include "XCAFDoc_ShapeTool.hxx"
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include <Message_ProgressIndicator.hxx>
#include <atomic>

namespace fs = boost::filesystem;

namespace Slic3r {

class TriangleMesh;
class ModelObject;

// 加载step的阶段
const int LOAD_STEP_STAGE_READ_FILE          = 0;
const int LOAD_STEP_STAGE_GET_SOLID          = 1;
const int LOAD_STEP_STAGE_GET_MESH           = 2;
const int LOAD_STEP_STAGE_NUM                = 3;
const int LOAD_STEP_STAGE_UNIT_NUM           = 5;

typedef std::function<void(int load_stage, int current, int total, bool& cancel)> ImportStepProgressFn;
typedef std::function<void(bool isUtf8)> StepIsUtf8Fn;

struct NamedSolid
{
    NamedSolid(const TopoDS_Shape& s,
               const std::string& n) : solid{ s }, name{ n } {
    }
    const TopoDS_Shape solid;
    const std::string  name;
    int tri_face_cout = 0;
};

//BBS: 将step文件加载到提供的模型中。
extern bool load_step(const char *path, Model *model,
                      bool& is_cancel,
                      double linear_defletion = 0.003,
                      double angle_defletion = 0.5,
                      bool isSplitCompound = false,
                      ImportStepProgressFn proFn = nullptr,
                      StepIsUtf8Fn isUtf8Fn = nullptr,
                      long& mesh_face_num = *(new long(-1)));

//BBS: 用于检测step的name字段使用哪种编码类型
// 如果是UTF8编码，文件无需处理，直接返回原始路径。
// 如果是GBK编码，则转换为UTF8并生成新的临时step文件。
// 如果是其他编码类型，无法处理，则视为UTF8。在这种情况下，名称将是乱码字符。
// 通过预处理，至少可以在name字段为GBK编码时避免乱码字符。
class StepPreProcessor {
    enum class EncodedType : unsigned char
    {
        UTF8,
        GBK,
        OTHER
    };

public:
    bool preprocess(const char* path, std::string &output_path);
    static bool isUtf8File(const char* path);
    static bool isUtf8(const std::string str);
private:
    static bool isGBK(const std::string str);
    static int preNum(const unsigned char byte);
    //BBS: 大多数step文件的默认编码是UTF8
    EncodedType m_encode_type = EncodedType::UTF8;
};

class StepProgressIncdicator : public Message_ProgressIndicator
{
public:
    StepProgressIncdicator(std::atomic<bool>& stop_flag) : should_stop(stop_flag){}

    Standard_Boolean UserBreak() override { return should_stop.load(); }

    void Show(const Message_ProgressScope&, const Standard_Boolean) override {
        std::cout << "Progress: " << GetPosition() << "%" << std::endl;
    }
private:
    std::atomic<bool>& should_stop;
};

class Step
{
public:
    Step(fs::path path, ImportStepProgressFn stepFn = nullptr, StepIsUtf8Fn isUtf8Fn = nullptr);
    Step(std::string path, ImportStepProgressFn stepFn = nullptr, StepIsUtf8Fn isUtf8Fn = nullptr);
    bool load();
    unsigned int get_triangle_num(double linear_defletion, double angle_defletion);
    unsigned int get_triangle_num_tbb(double linear_defletion, double angle_defletion);
    void clean_mesh_data();

    std::atomic<bool> m_stop_mesh;
private:
    std::string m_path;
    ImportStepProgressFn m_stepFn;
    StepIsUtf8Fn m_utf8Fn;
    Handle(XCAFApp_Application) m_app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) m_doc;
    Handle(XCAFDoc_ShapeTool) m_shape_tool;
    std::vector<NamedSolid> m_name_solids;
};

}; // namespace Slic3r

#endif /* slic3r_Format_STEP_hpp_ */
