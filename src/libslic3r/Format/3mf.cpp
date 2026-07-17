#include "../libslic3r.h"
#include "../Exception.hpp"
#include "../Model.hpp"
#include "../Utils.hpp"
#include "../LocalesUtils.hpp"
#include "../GCode.hpp"
#include "../Geometry.hpp"
#include "../GCode/ThumbnailData.hpp"
#include "../Semver.hpp"
#include "../Time.hpp"

#include "../I18N.hpp"

#include "3mf.hpp"

#include <limits>
#include <stdexcept>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/qi_int.hpp>
#include <boost/log/trivial.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
namespace pt = boost::property_tree;

#include <expat.h>
#include <Eigen/Dense>
#include "miniz_extension.hpp"

#include <fast_float/fast_float.h>

// 比sprintf("%.9g")略快，但karma浮点数格式化器存在问题，
// https://github.com/boostorg/spirit/pull/586
// 导出的字符串比保证无损往返所需的长度少一位。
// 此处代码保留以备boost改进时使用。
#define EXPORT_3MF_USE_SPIRIT_KARMA_FP 0

// 版本号
// 0 : 旧版slic3r或其他应用程序保存的.3mf文件。其中没有版本定义。
// 1 : 引入3mf版本控制。保存到3mf文件中的数据没有其他变化。
// 2 : 在Metadata/Slic3r_PE_model.config文件中添加了体积矩阵和源数据，加载时将网格转换回其坐标系。
// 警告 !! -> 版本号已回滚到1
//              下一次更改应使用3
const unsigned int VERSION_3MF = 1;
// 允许同时加载版本2文件。
const unsigned int VERSION_3MF_COMPATIBLE = 2;
const char* SLIC3RPE_3MF_VERSION = "slic3rpe:Version3mf"; // 保存到.model文件中的元数据名称定义

// 绘制工具数据版本号
// 0 : 旧版PrusaSlicer保存的3MF文件或未使用绘制工具。其中没有版本定义。
// 1 : 引入绘制工具数据版本控制。绘制工具数据没有其他变化。
const unsigned int FDM_SUPPORTS_PAINTING_VERSION = 1;
const unsigned int SEAM_PAINTING_VERSION         = 1;
const unsigned int MM_PAINTING_VERSION           = 1;

const std::string SLIC3RPE_FDM_SUPPORTS_PAINTING_VERSION = "slic3rpe:FdmSupportsPaintingVersion";
const std::string SLIC3RPE_SEAM_PAINTING_VERSION         = "slic3rpe:SeamPaintingVersion";
const std::string SLIC3RPE_MM_PAINTING_VERSION           = "slic3rpe:MmPaintingVersion";

const std::string MODEL_FOLDER = "3D/";
const std::string MODEL_EXTENSION = ".model";
const std::string MODEL_FILE = "3D/3dmodel.model"; // << 这是唯一能与CURA配合使用的字符串格式
const std::string CONTENT_TYPES_FILE = "[Content_Types].xml";
const std::string RELATIONSHIPS_FILE = "_rels/.rels";
const std::string THUMBNAIL_FILE = "Metadata/thumbnail.png";
const std::string PRINT_CONFIG_FILE = "Metadata/Slic3r_PE.config";
const std::string MODEL_CONFIG_FILE = "Metadata/Slic3r_PE_model.config";
const std::string LAYER_HEIGHTS_PROFILE_FILE = "Metadata/Slic3r_PE_layer_heights_profile.txt";
const std::string LAYER_CONFIG_RANGES_FILE = "Metadata/Prusa_Slicer_layer_config_ranges.xml";
const std::string SLA_SUPPORT_POINTS_FILE = "Metadata/Slic3r_PE_sla_support_points.txt";
const std::string SLA_DRAIN_HOLES_FILE = "Metadata/Slic3r_PE_sla_drain_holes.txt";
const std::string CUSTOM_GCODE_PER_PRINT_Z_FILE = "Metadata/Prusa_Slicer_custom_gcode_per_print_z.xml";

static constexpr const char* MODEL_TAG = "model";
static constexpr const char* RESOURCES_TAG = "resources";
static constexpr const char* OBJECT_TAG = "object";
static constexpr const char* MESH_TAG = "mesh";
static constexpr const char* VERTICES_TAG = "vertices";
static constexpr const char* VERTEX_TAG = "vertex";
static constexpr const char* TRIANGLES_TAG = "triangles";
static constexpr const char* TRIANGLE_TAG = "triangle";
static constexpr const char* COMPONENTS_TAG = "components";
static constexpr const char* COMPONENT_TAG = "component";
static constexpr const char* BUILD_TAG = "build";
static constexpr const char* ITEM_TAG = "item";
static constexpr const char* METADATA_TAG = "metadata";

static constexpr const char* CONFIG_TAG = "config";
static constexpr const char* VOLUME_TAG = "volume";

static constexpr const char* UNIT_ATTR = "unit";
static constexpr const char* NAME_ATTR = "name";
static constexpr const char* TYPE_ATTR = "type";
static constexpr const char* ID_ATTR = "id";
static constexpr const char* X_ATTR = "x";
static constexpr const char* Y_ATTR = "y";
static constexpr const char* Z_ATTR = "z";
static constexpr const char* V1_ATTR = "v1";
static constexpr const char* V2_ATTR = "v2";
static constexpr const char* V3_ATTR = "v3";
static constexpr const char* OBJECTID_ATTR = "objectid";
static constexpr const char* TRANSFORM_ATTR = "transform";
static constexpr const char* PRINTABLE_ATTR = "printable";
static constexpr const char* INSTANCESCOUNT_ATTR = "instances_count";
static constexpr const char* CUSTOM_SUPPORTS_ATTR = "slic3rpe:custom_supports";
static constexpr const char* CUSTOM_SEAM_ATTR = "slic3rpe:custom_seam";
static constexpr const char* MMU_SEGMENTATION_ATTR = "slic3rpe:mmu_segmentation";
static constexpr const char* FUZZY_SKIN_ATTR = "slic3rpe:fuzzy_skin";

static constexpr const char* KEY_ATTR = "key";
static constexpr const char* VALUE_ATTR = "value";
static constexpr const char* FIRST_TRIANGLE_ID_ATTR = "firstid";
static constexpr const char* LAST_TRIANGLE_ID_ATTR = "lastid";

static constexpr const char* OBJECT_TYPE = "object";
static constexpr const char* VOLUME_TYPE = "volume";

static constexpr const char* NAME_KEY = "name";
static constexpr const char* MODIFIER_KEY = "modifier";
static constexpr const char* VOLUME_TYPE_KEY = "volume_type";
static constexpr const char* MATRIX_KEY = "matrix";
static constexpr const char* SOURCE_FILE_KEY = "source_file";
static constexpr const char* SOURCE_OBJECT_ID_KEY = "source_object_id";
static constexpr const char* SOURCE_VOLUME_ID_KEY = "source_volume_id";
static constexpr const char* SOURCE_OFFSET_X_KEY = "source_offset_x";
static constexpr const char* SOURCE_OFFSET_Y_KEY = "source_offset_y";
static constexpr const char* SOURCE_OFFSET_Z_KEY = "source_offset_z";
static constexpr const char* SOURCE_IN_INCHES    = "source_in_inches";
static constexpr const char* SOURCE_IN_METERS    = "source_in_meters";

static constexpr const char* MESH_STAT_EDGES_FIXED          = "edges_fixed";
static constexpr const char* MESH_STAT_DEGENERATED_FACETS   = "degenerate_facets";
static constexpr const char* MESH_STAT_FACETS_REMOVED       = "facets_removed";
static constexpr const char* MESH_STAT_FACETS_RESERVED      = "facets_reversed";
static constexpr const char* MESH_STAT_BACKWARDS_EDGES      = "backwards_edges";


const unsigned int VALID_OBJECT_TYPES_COUNT = 1;
const char* VALID_OBJECT_TYPES[] =
{
    "model"
};

const char* INVALID_OBJECT_TYPES[] =
{
    "solidsupport",
    "support",
    "surface",
    "other"
};

class version_error : public Slic3r::FileIOError
{
public:
    version_error(const std::string& what_arg) : Slic3r::FileIOError(what_arg) {}
    version_error(const char* what_arg) : Slic3r::FileIOError(what_arg) {}
};

const char* get_attribute_value_charptr(const char** attributes, unsigned int attributes_size, const char* attribute_key)
{
    if ((attributes == nullptr) || (attributes_size == 0) || (attributes_size % 2 != 0) || (attribute_key == nullptr))
        return nullptr;

    for (unsigned int a = 0; a < attributes_size; a += 2) {
        if (::strcmp(attributes[a], attribute_key) == 0)
            return attributes[a + 1];
    }

    return nullptr;
}

std::string get_attribute_value_string(const char** attributes, unsigned int attributes_size, const char* attribute_key)
{
    const char* text = get_attribute_value_charptr(attributes, attributes_size, attribute_key);
    return (text != nullptr) ? text : "";
}

float get_attribute_value_float(const char** attributes, unsigned int attributes_size, const char* attribute_key)
{
    float value = 0.0f;
    if (const char *text = get_attribute_value_charptr(attributes, attributes_size, attribute_key); text != nullptr)
        fast_float::from_chars(text, text + strlen(text), value);
    return value;
}

int get_attribute_value_int(const char** attributes, unsigned int attributes_size, const char* attribute_key)
{
    int value = 0;
    if (const char *text = get_attribute_value_charptr(attributes, attributes_size, attribute_key); text != nullptr)
        boost::spirit::qi::parse(text, text + strlen(text), boost::spirit::qi::int_, value);
    return value;
}

bool get_attribute_value_bool(const char** attributes, unsigned int attributes_size, const char* attribute_key)
{
    const char* text = get_attribute_value_charptr(attributes, attributes_size, attribute_key);
    return (text != nullptr) ? (bool)::atoi(text) : true;
}

Slic3r::Transform3d get_transform_from_3mf_specs_string(const std::string& mat_str)
{
    // 查看：https://3mf.io/3d-manufacturing-format/ 或 https://github.com/3MFConsortium/spec_core/blob/master/3MF%20Core%20Specification.md
    // 了解根据规范矩阵如何存储在3mf中
    Slic3r::Transform3d ret = Slic3r::Transform3d::Identity();

    if (mat_str.empty())
        // 空字符串表示默认单位矩阵
        return ret;

    std::vector<std::string> mat_elements_str;
    boost::split(mat_elements_str, mat_str, boost::is_any_of(" "), boost::token_compress_on);

    unsigned int size = (unsigned int)mat_elements_str.size();
    if (size != 12)
        // 无效数据，返回单位矩阵
        return ret;

    unsigned int i = 0;
    // 矩阵在3mf文件中存储为4x3格式
    // 需要对其进行转置
    for (unsigned int c = 0; c < 4; ++c) {
        for (unsigned int r = 0; r < 3; ++r) {
            ret(r, c) = ::atof(mat_elements_str[i++].c_str());
        }
    }
    return ret;
}

float get_unit_factor(const std::string& unit)
{
    const char* text = unit.c_str();

    if (::strcmp(text, "micron") == 0)
        return 0.001f;
    else if (::strcmp(text, "centimeter") == 0)
        return 10.0f;
    else if (::strcmp(text, "inch") == 0)
        return 25.4f;
    else if (::strcmp(text, "foot") == 0)
        return 304.8f;
    else if (::strcmp(text, "meter") == 0)
        return 1000.0f;
    else
        // 默认为"毫米"（参见规范）
        return 1.0f;
}

bool is_valid_object_type(const std::string& type)
{
    // 如果类型为空，默认为"model"（参见规范）
    if (type.empty())
        return true;

    for (unsigned int i = 0; i < VALID_OBJECT_TYPES_COUNT; ++i) {
        if (::strcmp(type.c_str(), VALID_OBJECT_TYPES[i]) == 0)
            return true;
    }

    return false;
}

namespace Slic3r {

//! 用于标记本地化使用的字符串的宏，
//! 返回相同的字符串
#define L(s) (s)
#define _(s) Slic3r::I18N::translate(s)
void XMLCALL PrusaFileParser::start_element_handler(void *userData, const char *name, const char **attributes)
{
    PrusaFileParser *prusa_parser = (PrusaFileParser *) userData;
    if (prusa_parser != nullptr) { prusa_parser->_start_element_handler(name, attributes); }
}

void XMLCALL PrusaFileParser::characters_handler(void *userData, const XML_Char *s, int len)
{
    PrusaFileParser *prusa_parser = (PrusaFileParser *) userData;
    if (prusa_parser != nullptr) { prusa_parser->_characters_handler(s, len); }
}

bool PrusaFileParser::check_3mf_from_prusa(const std::string filename)
{
    mz_zip_archive archive;
    mz_zip_zero_struct(&archive);
    if (!open_zip_reader(&archive, filename)) {
        // throw Slic3r::RuntimeError("Loading 3mf file failed.");
        return false;
    }

    const std::string sub_relationship_file = "3D/_rels/3dmodel.model.rels";
    int               sub_index             = mz_zip_reader_locate_file(&archive, sub_relationship_file.c_str(), nullptr, 0);
    if (sub_index == -1) {
        const std::string model_file       = "3D/3dmodel.model";
        int               model_file_index = mz_zip_reader_locate_file(&archive, model_file.c_str(), nullptr, 0);
        if (model_file_index != -1) {
            int depth = 0;
            m_parser  = XML_ParserCreate(nullptr);
            XML_SetUserData(m_parser, (void *) this);
            XML_SetElementHandler(m_parser, start_element_handler, nullptr);
            XML_SetCharacterDataHandler(m_parser, characters_handler);

            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(&archive, model_file_index, &stat)) goto EXIT;

            void *parser_buffer = XML_GetBuffer(m_parser, (int) stat.m_uncomp_size);
            if (parser_buffer == nullptr) goto EXIT;

            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, parser_buffer, (size_t) stat.m_uncomp_size, 0);
            if (res == 0) goto EXIT;

            XML_ParseBuffer(m_parser, (int) stat.m_uncomp_size, 1);
        }
    }

EXIT:
    close_zip_reader(&archive);
    return m_from_prusa;
}

void PrusaFileParser::_characters_handler(const XML_Char *s, int len)
{
    if (m_is_application_key) {
        std::string str(s, len);
        if (!str.empty() && str.find("PrusaSlicer") != std::string::npos) m_from_prusa = true;
    }
}

void PrusaFileParser::_start_element_handler(const char *name, const char **attributes)
{
    if (::strcmp(name, "metadata") == 0) {
        unsigned int num_attributes = (unsigned int) XML_GetSpecifiedAttributeCount(m_parser);

        std::string str_name = get_attribute_value_string(attributes, num_attributes, "name");
        if (!str_name.empty() && str_name.find("Application") != std::string::npos) { m_is_application_key = true; }
    }
}

const char *PrusaFileParser::get_attribute_value_charptr(const char **attributes, unsigned int attributes_size, const char *attribute_key)
{
    if ((attributes == nullptr) || (attributes_size == 0) || (attributes_size % 2 != 0) || (attribute_key == nullptr)) return nullptr;

    for (unsigned int a = 0; a < attributes_size; a += 2) {
        if (::strcmp(attributes[a], attribute_key) == 0) return attributes[a + 1];
    }

    return nullptr;
}

std::string PrusaFileParser::get_attribute_value_string(const char **attributes, unsigned int attributes_size, const char *attribute_key)
{
    const char *text = get_attribute_value_charptr(attributes, attributes_size, attribute_key);
    return (text != nullptr) ? text : "";
}

ModelVolumeType type_from_string(const std::string &s)
{
    // 遗留支持
    if (s == "1") return ModelVolumeType::PARAMETER_MODIFIER;
    // 新类型（支持支撑强化和阻挡）
    if (s == "ModelPart") return ModelVolumeType::MODEL_PART;
    if (s == "NegativeVolume") return ModelVolumeType::NEGATIVE_VOLUME;
    if (s == "ParameterModifier") return ModelVolumeType::PARAMETER_MODIFIER;
    if (s == "SupportEnforcer") return ModelVolumeType::SUPPORT_ENFORCER;
    if (s == "SupportBlocker") return ModelVolumeType::SUPPORT_BLOCKER;
    // 如果收到无效类型字符串，返回默认值。
    return ModelVolumeType::MODEL_PART;
}

    // 带有错误消息管理的基类
    class _3MF_Base
    {
        std::vector<std::string> m_errors;

    protected:
        void add_error(const std::string& error) { m_errors.push_back(error); }
        void clear_errors() { m_errors.clear(); }

    public:
        void log_errors()
        {
            for (const std::string& error : m_errors)
                BOOST_LOG_TRIVIAL(error) << error;
        }
    };

    class _3MF_Importer : public _3MF_Base
    {
        struct Component
        {
            int object_id;
            Transform3d transform;

            explicit Component(int object_id)
                : object_id(object_id)
                , transform(Transform3d::Identity())
            {
            }

            Component(int object_id, const Transform3d& transform)
                : object_id(object_id)
                , transform(transform)
            {
            }
        };

        typedef std::vector<Component> ComponentsList;

        struct Geometry
        {
            std::vector<Vec3f> vertices;
            std::vector<Vec3i32> triangles;
            std::vector<std::string> custom_supports;
            std::vector<std::string> custom_seam;
            std::vector<std::string> mmu_segmentation;
            std::vector<std::string> fuzzy_skin;

            bool empty() { return vertices.empty() || triangles.empty(); }

            void reset() {
                vertices.clear();
                triangles.clear();
                custom_supports.clear();
                custom_seam.clear();
                mmu_segmentation.clear();
                fuzzy_skin.clear();
            }
        };

        struct CurrentObject
        {
            // 3MF文件中对象的ID，从1开始。
            int id;
            // ModelObject在其对应Model中的索引，从0开始。
            int model_object_idx;
            Geometry geometry;
            ModelObject* object;
            ComponentsList components;

            CurrentObject() { reset(); }

            void reset() {
                id = -1;
                model_object_idx = -1;
                geometry.reset();
                object = nullptr;
                components.clear();
            }
        };

        struct CurrentConfig
        {
            int object_id;
            int volume_id;
        };

        struct Instance
        {
            ModelInstance* instance;
            Transform3d transform;

            Instance(ModelInstance* instance, const Transform3d& transform)
                : instance(instance)
                , transform(transform)
            {
            }
        };

        struct Metadata
        {
            std::string key;
            std::string value;

            Metadata(const std::string& key, const std::string& value)
                : key(key)
                , value(value)
            {
            }
        };

        typedef std::vector<Metadata> MetadataList;

        struct ObjectMetadata
        {
            struct VolumeMetadata
            {
                unsigned int first_triangle_id;
                unsigned int last_triangle_id;
                MetadataList metadata;
                RepairedMeshErrors mesh_stats;

                VolumeMetadata(unsigned int first_triangle_id, unsigned int last_triangle_id)
                    : first_triangle_id(first_triangle_id)
                    , last_triangle_id(last_triangle_id)
                {
                }
            };

            typedef std::vector<VolumeMetadata> VolumeMetadataList;

            MetadataList metadata;
            VolumeMetadataList volumes;
        };

        // 从基于1的3MF对象ID映射到m_model->objects中基于0的ModelObject索引。
        typedef std::map<int, int> IdToModelObjectMap;
        typedef std::map<int, ComponentsList> IdToAliasesMap;
        typedef std::vector<Instance> InstancesList;
        typedef std::map<int, ObjectMetadata> IdToMetadataMap;
        typedef std::map<int, Geometry> IdToGeometryMap;
        typedef std::map<int, std::vector<coordf_t>> IdToLayerHeightsProfileMap;
        typedef std::map<int, t_layer_config_ranges> IdToLayerConfigRangesMap;
        typedef std::map<int, std::vector<sla::SupportPoint>> IdToSlaSupportPointsMap;
        typedef std::map<int, std::vector<sla::DrainHole>> IdToSlaDrainHolesMap;

        // 3mf文件的版本
        unsigned int m_version;
        bool m_check_version;

        // 生成此3MF的PrusaSlicer的语义版本。
        boost::optional<Semver> m_prusaslicer_generator_version;
        unsigned int m_fdm_supports_painting_version = 0;
        unsigned int m_seam_painting_version         = 0;
        unsigned int m_mm_painting_version           = 0;

        XML_Parser m_xml_parser;
        // 由解析器应用程序端返回的错误代码。在这种情况下，expat在从XML_Parse()函数返回后可能无法可靠地传递错误状态，
        // 因此我们将错误状态保留在这里。
        bool m_parse_error { false };
        std::string m_parse_error_message;
        Model* m_model;
        float m_unit_factor;
        CurrentObject m_curr_object;
        IdToModelObjectMap m_objects;
        IdToAliasesMap m_objects_aliases;
        InstancesList m_instances;
        IdToGeometryMap m_geometries;
        CurrentConfig m_curr_config;
        IdToMetadataMap m_objects_metadata;
        IdToLayerHeightsProfileMap m_layer_heights_profiles;
        IdToLayerConfigRangesMap m_layer_config_ranges;
        IdToSlaSupportPointsMap m_sla_support_points;
        IdToSlaDrainHolesMap    m_sla_drain_holes;
        std::string m_curr_metadata_name;
        std::string m_curr_characters;
        std::string m_name;

    public:
        _3MF_Importer();
        ~_3MF_Importer();

        bool load_model_from_file(const std::string& filename, Model& model, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions, bool check_version);
        unsigned int version() const { return m_version; }

    private:
        void _destroy_xml_parser();
        void _stop_xml_parser(const std::string& msg = std::string());

        bool        parse_error()         const { return m_parse_error; }
        const char* parse_error_message() const {
            return m_parse_error ?
                // 错误由用户代码发出，而不是expat解析器。
                (m_parse_error_message.empty() ? "Invalid 3MF format" : m_parse_error_message.c_str()) :
                // 错误由expat解析器发出。
                XML_ErrorString(XML_GetErrorCode(m_xml_parser));
        }

        bool _load_model_from_file(const std::string& filename, Model& model, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions);
        bool _extract_model_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat);
        void _extract_layer_heights_profile_config_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat);
        void _extract_layer_config_ranges_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat, ConfigSubstitutionContext& config_substitutions);
        void _extract_sla_support_points_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat);
        void _extract_sla_drain_holes_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat);

        void _extract_custom_gcode_per_print_z_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat);

        void _extract_print_config_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat, DynamicPrintConfig& config, ConfigSubstitutionContext& subs_context, const std::string& archive_filename);
        bool _extract_model_config_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat, Model& model);

        // 解析.model文件的处理器
        void _handle_start_model_xml_element(const char* name, const char** attributes);
        void _handle_end_model_xml_element(const char* name);
        void _handle_model_xml_characters(const XML_Char* s, int len);

        // 解析MODEL_CONFIG_FILE文件的处理器
        void _handle_start_config_xml_element(const char* name, const char** attributes);
        void _handle_end_config_xml_element(const char* name);

        bool _handle_start_model(const char** attributes, unsigned int num_attributes);
        bool _handle_end_model();

        bool _handle_start_resources(const char** attributes, unsigned int num_attributes);
        bool _handle_end_resources();

        bool _handle_start_object(const char** attributes, unsigned int num_attributes);
        bool _handle_end_object();

        bool _handle_start_mesh(const char** attributes, unsigned int num_attributes);
        bool _handle_end_mesh();

        bool _handle_start_vertices(const char** attributes, unsigned int num_attributes);
        bool _handle_end_vertices();

        bool _handle_start_vertex(const char** attributes, unsigned int num_attributes);
        bool _handle_end_vertex();

        bool _handle_start_triangles(const char** attributes, unsigned int num_attributes);
        bool _handle_end_triangles();

        bool _handle_start_triangle(const char** attributes, unsigned int num_attributes);
        bool _handle_end_triangle();

        bool _handle_start_components(const char** attributes, unsigned int num_attributes);
        bool _handle_end_components();

        bool _handle_start_component(const char** attributes, unsigned int num_attributes);
        bool _handle_end_component();

        bool _handle_start_build(const char** attributes, unsigned int num_attributes);
        bool _handle_end_build();

        bool _handle_start_item(const char** attributes, unsigned int num_attributes);
        bool _handle_end_item();

        bool _handle_start_metadata(const char** attributes, unsigned int num_attributes);
        bool _handle_end_metadata();

        bool _create_object_instance(int object_id, const Transform3d& transform, const bool printable, unsigned int recur_counter);

        void _apply_transform(ModelInstance& instance, const Transform3d& transform);

        bool _handle_start_config(const char** attributes, unsigned int num_attributes);
        bool _handle_end_config();

        bool _handle_start_config_object(const char** attributes, unsigned int num_attributes);
        bool _handle_end_config_object();

        bool _handle_start_config_volume(const char** attributes, unsigned int num_attributes);
        bool _handle_start_config_volume_mesh(const char** attributes, unsigned int num_attributes);
        bool _handle_end_config_volume();
        bool _handle_end_config_volume_mesh();

        bool _handle_start_config_metadata(const char** attributes, unsigned int num_attributes);
        bool _handle_end_config_metadata();

        bool _generate_volumes(ModelObject& object, const Geometry& geometry, const ObjectMetadata::VolumeMetadataList& volumes, ConfigSubstitutionContext& config_substitutions);

        // 解析.model文件的回调函数
        static void XMLCALL _handle_start_model_xml_element(void* userData, const char* name, const char** attributes);
        static void XMLCALL _handle_end_model_xml_element(void* userData, const char* name);
        static void XMLCALL _handle_model_xml_characters(void* userData, const XML_Char* s, int len);

        // 解析MODEL_CONFIG_FILE文件的回调函数
        static void XMLCALL _handle_start_config_xml_element(void* userData, const char* name, const char** attributes);
        static void XMLCALL _handle_end_config_xml_element(void* userData, const char* name);
    };

    _3MF_Importer::_3MF_Importer()
        : m_version(0)
        , m_check_version(false)
        , m_xml_parser(nullptr)
        , m_model(nullptr)
        , m_unit_factor(1.0f)
        , m_curr_metadata_name("")
        , m_curr_characters("")
        , m_name("")
    {
    }

    _3MF_Importer::~_3MF_Importer()
    {
        _destroy_xml_parser();
    }

    bool _3MF_Importer::load_model_from_file(const std::string& filename, Model& model, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions, bool check_version)
    {
        m_version = 0;
        m_fdm_supports_painting_version = 0;
        m_seam_painting_version = 0;
        m_mm_painting_version = 0;
        m_check_version = check_version;
        m_model = &model;
        m_unit_factor = 1.0f;
        m_curr_object.reset();
        m_objects.clear();
        m_objects_aliases.clear();
        m_instances.clear();
        m_geometries.clear();
        m_curr_config.object_id = -1;
        m_curr_config.volume_id = -1;
        m_objects_metadata.clear();
        m_layer_heights_profiles.clear();
        m_layer_config_ranges.clear();
        m_sla_support_points.clear();
        m_curr_metadata_name.clear();
        m_curr_characters.clear();
        clear_errors();

        return _load_model_from_file(filename, model, config, config_substitutions);
    }

    void _3MF_Importer::_destroy_xml_parser()
    {
        if (m_xml_parser != nullptr) {
            XML_ParserFree(m_xml_parser);
            m_xml_parser = nullptr;
        }
    }

    void _3MF_Importer::_stop_xml_parser(const std::string &msg)
    {
        assert(! m_parse_error);
        assert(m_parse_error_message.empty());
        assert(m_xml_parser != nullptr);
        m_parse_error = true;
        m_parse_error_message = msg;
        XML_StopParser(m_xml_parser, false);
    }

    bool _3MF_Importer::_load_model_from_file(const std::string& filename, Model& model, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions)
    {
        mz_zip_archive archive;
        mz_zip_zero_struct(&archive);

        if (!open_zip_reader(&archive, filename)) {
            add_error("Unable to open the file");
            return false;
        }

        mz_uint num_entries = mz_zip_reader_get_num_files(&archive);

        mz_zip_archive_file_stat stat;

        m_name = boost::filesystem::path(filename).stem().string();

        // 我们首先循环条目，仅从存档中读取.model文件，以便从中提取版本
        for (mz_uint i = 0; i < num_entries; ++i) {
            if (mz_zip_reader_file_stat(&archive, i, &stat)) {
                std::string name(stat.m_filename);
                std::replace(name.begin(), name.end(), '\\', '/');

                if (boost::algorithm::istarts_with(name, MODEL_FOLDER) && boost::algorithm::iends_with(name, MODEL_EXTENSION)) {
                    try
                    {
                        // 有效模型名称 -> 提取模型
                        if (!_extract_model_from_archive(archive, stat)) {
                            close_zip_reader(&archive);
                            add_error("Archive does not contain a valid model");
                            return false;
                        }
                    }
                    catch (const std::exception& e)
                    {
                        // 确保zip存档已关闭并重新抛出异常
                        close_zip_reader(&archive);
                        throw Slic3r::FileIOError(e.what());
                    }
                }
            }
        }

        // 然后再次循环条目以读取存储在存档中的其他文件
        for (mz_uint i = 0; i < num_entries; ++i) {
            if (mz_zip_reader_file_stat(&archive, i, &stat)) {
                std::string name(stat.m_filename);
                std::replace(name.begin(), name.end(), '\\', '/');

                /*
                if (boost::algorithm::iequals(name, LAYER_HEIGHTS_PROFILE_FILE)) {
                    // extract slic3r layer heights profile file
                    _extract_layer_heights_profile_config_from_archive(archive, stat);
                }
                else if (boost::algorithm::iequals(name, LAYER_CONFIG_RANGES_FILE)) {
                    // extract slic3r layer config ranges file
                    _extract_layer_config_ranges_from_archive(archive, stat, config_substitutions);
                }
                else if (boost::algorithm::iequals(name, SLA_SUPPORT_POINTS_FILE)) {
                    // extract sla support points file
                    _extract_sla_support_points_from_archive(archive, stat);
                }
                else if (boost::algorithm::iequals(name, SLA_DRAIN_HOLES_FILE)) {
                    // extract sla support points file
                    _extract_sla_drain_holes_from_archive(archive, stat);
                }
                else if (boost::algorithm::iequals(name, PRINT_CONFIG_FILE)) {
                    // extract slic3r print config file
                    _extract_print_config_from_archive(archive, stat, config, config_substitutions, filename);
                }
                else if (boost::algorithm::iequals(name, CUSTOM_GCODE_PER_PRINT_Z_FILE)) {
                    // extract slic3r layer config ranges file
                    _extract_custom_gcode_per_print_z_from_archive(archive, stat);
                }
                */
                // 仅读取Prusa 3mf的模型配置
                if (boost::algorithm::iequals(name, MODEL_CONFIG_FILE)) {
                    // extract slic3r model config file
                    if (!_extract_model_config_from_archive(archive, stat, model)) {
                        close_zip_reader(&archive);
                        add_error("Archive does not contain a valid model config");
                        return false;
                    }
                }
            }
        }

        close_zip_reader(&archive);

        if (m_version == 0) {
            // 如果3mf不是由PrusaSlicer生成且有多个实例，
            // 则将对象拆分为与实例数量相同的对象
            size_t curr_models_count = m_model->objects.size();
            size_t i = 0;
            while (i < curr_models_count) {
                ModelObject* model_object = m_model->objects[i];
                if (model_object->instances.size() > 1) {
                    // select the geometry associated with the original model object
                    const Geometry* geometry = nullptr;
                    for (const IdToModelObjectMap::value_type& object : m_objects) {
                        if (object.second == int(i)) {
                            IdToGeometryMap::const_iterator obj_geometry = m_geometries.find(object.first);
                            if (obj_geometry == m_geometries.end()) {
                                add_error("Unable to find object geometry");
                                return false;
                            }
                            geometry = &obj_geometry->second;
                            break;
                        }
                    }

                    if (geometry == nullptr) {
                        add_error("Unable to find object geometry");
                        return false;
                    }

                    // use the geometry to create the volumes in the new model objects
                    ObjectMetadata::VolumeMetadataList volumes(1, { 0, (unsigned int)geometry->triangles.size() - 1 });

                    // for each instance after the 1st, create a new model object containing only that instance
                    // and copy into it the geometry
                    while (model_object->instances.size() > 1) {
                        ModelObject* new_model_object = m_model->add_object(*model_object);
                        new_model_object->clear_instances();
                        new_model_object->add_instance(*model_object->instances.back());
                        model_object->delete_last_instance();
                        if (!_generate_volumes(*new_model_object, *geometry, volumes, config_substitutions))
                            return false;
                    }
                }
                ++i;
            }
        }

        for (const IdToModelObjectMap::value_type& object : m_objects) {
            if (object.second >= int(m_model->objects.size())) {
                add_error("Unable to find object");
                return false;
            }
            ModelObject* model_object = m_model->objects[object.second];
            IdToGeometryMap::const_iterator obj_geometry = m_geometries.find(object.first);
            if (obj_geometry == m_geometries.end()) {
                add_error("Unable to find object geometry");
                return false;
            }

            // m_layer_heights_profiles are indexed by a 1 based model object index.
            IdToLayerHeightsProfileMap::iterator obj_layer_heights_profile = m_layer_heights_profiles.find(object.second + 1);
            if (obj_layer_heights_profile != m_layer_heights_profiles.end())
                model_object->layer_height_profile.set(std::move(obj_layer_heights_profile->second));

            // m_layer_config_ranges are indexed by a 1 based model object index.
            IdToLayerConfigRangesMap::iterator obj_layer_config_ranges = m_layer_config_ranges.find(object.second + 1);
            if (obj_layer_config_ranges != m_layer_config_ranges.end())
                model_object->layer_config_ranges = std::move(obj_layer_config_ranges->second);

            // m_sla_support_points are indexed by a 1 based model object index.
            IdToSlaSupportPointsMap::iterator obj_sla_support_points = m_sla_support_points.find(object.second + 1);
            if (obj_sla_support_points != m_sla_support_points.end() && !obj_sla_support_points->second.empty()) {
                model_object->sla_support_points = std::move(obj_sla_support_points->second);
                model_object->sla_points_status = sla::PointsStatus::UserModified;
            }

            IdToSlaDrainHolesMap::iterator obj_drain_holes = m_sla_drain_holes.find(object.second + 1);
            if (obj_drain_holes != m_sla_drain_holes.end() && !obj_drain_holes->second.empty()) {
                model_object->sla_drain_holes = std::move(obj_drain_holes->second);
            }

            ObjectMetadata::VolumeMetadataList volumes;
            ObjectMetadata::VolumeMetadataList* volumes_ptr = nullptr;

            IdToMetadataMap::iterator obj_metadata = m_objects_metadata.find(object.first);
            if (obj_metadata != m_objects_metadata.end()) {
                // 找到了配置数据，此模型是使用slic3r pe保存的

                // 应用对象的名称和配置数据
                for (const Metadata& metadata : obj_metadata->second.metadata) {
                    if (metadata.key == "name")
                        model_object->name = metadata.value;
                    else
                        model_object->config.set_deserialize(metadata.key, metadata.value, config_substitutions);
                }

                // select object's detected volumes
                volumes_ptr = &obj_metadata->second.volumes;
            }
            else {
                // config data not found, this model was not saved using slic3r pe

                // add the entire geometry as the single volume to generate
                volumes.emplace_back(0, (int)obj_geometry->second.triangles.size() - 1);

                // select as volumes
                volumes_ptr = &volumes;
            }

            if (!_generate_volumes(*model_object, obj_geometry->second, *volumes_ptr, config_substitutions))
                return false;
        }

        int object_idx = 0;
        for (ModelObject* o : model.objects) {
            int volume_idx = 0;
            for (ModelVolume* v : o->volumes) {
                if (v->source.input_file.empty() && v->type() == ModelVolumeType::MODEL_PART) {
                    v->source.input_file = filename;
                    if (v->source.volume_idx == -1)
                        v->source.volume_idx = volume_idx;
                    if (v->source.object_idx == -1)
                        v->source.object_idx = object_idx;
                }
                ++volume_idx;
            }
            ++object_idx;
        }

        //BBS: copy object isteadof instance
        int object_size = model.objects.size();
        for (int obj_index = 0; obj_index < object_size; obj_index ++) {
            ModelObject* object = model.objects[obj_index];
            while (object->instances.size() > 1) {
                ModelObject* new_model_object = model.add_object(*object);
                new_model_object->clear_instances();
                new_model_object->add_instance(*object->instances.back());
                object->delete_last_instance();
            }
        }

//        // fixes the min z of the model if negative
//        model.adjust_min_z();

        return true;
    }

    bool _3MF_Importer::_extract_model_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat)
    {
        if (stat.m_uncomp_size == 0) {
            add_error("Found invalid size");
            return false;
        }

        _destroy_xml_parser();

        m_xml_parser = XML_ParserCreate(nullptr);
        if (m_xml_parser == nullptr) {
            add_error("Unable to create parser");
            return false;
        }

        XML_SetUserData(m_xml_parser, (void*)this);
        XML_SetElementHandler(m_xml_parser, _3MF_Importer::_handle_start_model_xml_element, _3MF_Importer::_handle_end_model_xml_element);
        XML_SetCharacterDataHandler(m_xml_parser, _3MF_Importer::_handle_model_xml_characters);

        struct CallbackData
        {
            XML_Parser& parser;
            _3MF_Importer& importer;
            const mz_zip_archive_file_stat& stat;

            CallbackData(XML_Parser& parser, _3MF_Importer& importer, const mz_zip_archive_file_stat& stat) : parser(parser), importer(importer), stat(stat) {}
        };

        CallbackData data(m_xml_parser, *this, stat);

        mz_bool res = 0;

        try
        {
            res = mz_zip_reader_extract_file_to_callback(&archive, stat.m_filename, [](void* pOpaque, mz_uint64 file_ofs, const void* pBuf, size_t n)->size_t {
                CallbackData* data = (CallbackData*)pOpaque;
                if (!XML_Parse(data->parser, (const char*)pBuf, (int)n, (file_ofs + n == data->stat.m_uncomp_size) ? 1 : 0) || data->importer.parse_error()) {
                    char error_buf[1024];
                    ::sprintf(error_buf, "Error (%s) while parsing '%s' at line %d", data->importer.parse_error_message(), data->stat.m_filename, (int)XML_GetCurrentLineNumber(data->parser));
                    throw Slic3r::FileIOError(error_buf);
                }

                return n;
                }, &data, 0);
        }
        catch (const version_error& e)
        {
            // rethrow the exception
            throw Slic3r::FileIOError(e.what());
        }
        catch (std::exception& e)
        {
            add_error(e.what());
            return false;
        }

        if (res == 0) {
            add_error("Error while extracting model data from zip archive");
            return false;
        }

        return true;
    }

    void _3MF_Importer::_extract_print_config_from_archive(
        mz_zip_archive& archive, const mz_zip_archive_file_stat& stat,
        DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions,
        const std::string& archive_filename)
    {
        if (stat.m_uncomp_size > 0) {
            std::string buffer((size_t)stat.m_uncomp_size, 0);
            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
            if (res == 0) {
                add_error("Error while reading config data to buffer");
                return;
            }
            //FIXME Loading a "will be one day a legacy format" of configuration in a form of a G-code comment.
            // 每行配置都以分号（G-code注释）为前缀，这很难看。

            // 用load_from_ini_string_commented替换旧函数会导致
            // 解析PrusaSlicer 2.0.0之前的3MF时出现问题（INI中可能有重复条目）。
            // 参见 https://github.com/prusa3d/PrusaSlicer/issues/7155。我们暂时恢复原样。
            //config_substitutions.substitutions = config.load_from_ini_string_commented(std::move(buffer), config_substitutions.rule);
            ConfigBase::load_from_gcode_string_legacy(config, buffer.data(), config_substitutions);
        }
    }

    void _3MF_Importer::_extract_layer_heights_profile_config_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat)
    {
        if (stat.m_uncomp_size > 0) {
            std::string buffer((size_t)stat.m_uncomp_size, 0);
            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
            if (res == 0) {
                add_error("Error while reading layer heights profile data to buffer");
                return;
            }

            if (buffer.back() == '\n')
                buffer.pop_back();

            std::vector<std::string> objects;
            boost::split(objects, buffer, boost::is_any_of("\n"), boost::token_compress_off);

            for (const std::string& object : objects)             {
                std::vector<std::string> object_data;
                boost::split(object_data, object, boost::is_any_of("|"), boost::token_compress_off);
                if (object_data.size() != 2) {
                    add_error("Error while reading object data");
                    continue;
                }

                std::vector<std::string> object_data_id;
                boost::split(object_data_id, object_data[0], boost::is_any_of("="), boost::token_compress_off);
                if (object_data_id.size() != 2) {
                    add_error("Error while reading object id");
                    continue;
                }

                int object_id = std::atoi(object_data_id[1].c_str());
                if (object_id == 0) {
                    add_error("Found invalid object id");
                    continue;
                }

                IdToLayerHeightsProfileMap::iterator object_item = m_layer_heights_profiles.find(object_id);
                if (object_item != m_layer_heights_profiles.end()) {
                    add_error("Found duplicated layer heights profile");
                    continue;
                }

                std::vector<std::string> object_data_profile;
                boost::split(object_data_profile, object_data[1], boost::is_any_of(";"), boost::token_compress_off);
                if (object_data_profile.size() <= 4 || object_data_profile.size() % 2 != 0) {
                    add_error("Found invalid layer heights profile");
                    continue;
                }

                std::vector<coordf_t> profile;
                profile.reserve(object_data_profile.size());

                for (const std::string& value : object_data_profile) {
                    profile.push_back((coordf_t)std::atof(value.c_str()));
                }

                m_layer_heights_profiles.insert({ object_id, profile });
            }
        }
    }

    void _3MF_Importer::_extract_layer_config_ranges_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat, ConfigSubstitutionContext& config_substitutions)
    {
        if (stat.m_uncomp_size > 0) {
            std::string buffer((size_t)stat.m_uncomp_size, 0);
            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
            if (res == 0) {
                add_error("Error while reading layer config ranges data to buffer");
                return;
            }

            std::istringstream iss(buffer); // wrap returned xml to istringstream
            pt::ptree objects_tree;
            pt::read_xml(iss, objects_tree);

            for (const auto& object : objects_tree.get_child("objects")) {
                pt::ptree object_tree = object.second;
                int obj_idx = object_tree.get<int>("<xmlattr>.id", -1);
                if (obj_idx <= 0) {
                    add_error("Found invalid object id");
                    continue;
                }

                IdToLayerConfigRangesMap::iterator object_item = m_layer_config_ranges.find(obj_idx);
                if (object_item != m_layer_config_ranges.end()) {
                    add_error("Found duplicated layer config range");
                    continue;
                }

                t_layer_config_ranges config_ranges;

                for (const auto& range : object_tree) {
                    if (range.first != "range")
                        continue;
                    pt::ptree range_tree = range.second;
                    double min_z = range_tree.get<double>("<xmlattr>.min_z");
                    double max_z = range_tree.get<double>("<xmlattr>.max_z");

                    // 获取Z范围信息
                    DynamicPrintConfig config;

                    for (const auto& option : range_tree) {
                        if (option.first != "option")
                            continue;
                        std::string opt_key = option.second.get<std::string>("<xmlattr>.opt_key");
                        std::string value = option.second.data();
                        config.set_deserialize(opt_key, value, config_substitutions);
                    }

                    config_ranges[{ min_z, max_z }].assign_config(std::move(config));
                }

                if (!config_ranges.empty())
                    m_layer_config_ranges.insert({ obj_idx, std::move(config_ranges) });
            }
        }
    }

    void _3MF_Importer::_extract_sla_support_points_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat)
    {
        if (stat.m_uncomp_size > 0) {
            std::string buffer((size_t)stat.m_uncomp_size, 0);
            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
            if (res == 0) {
                add_error("Error while reading sla support points data to buffer");
                return;
            }

            if (buffer.back() == '\n')
                buffer.pop_back();

            std::vector<std::string> objects;
            boost::split(objects, buffer, boost::is_any_of("\n"), boost::token_compress_off);

            // 格式版本信息 - 参见3mf.hpp
            int version = 0;
            std::string key("support_points_format_version=");
            if (!objects.empty() && objects[0].find(key) != std::string::npos) {
                objects[0].erase(objects[0].begin(), objects[0].begin() + long(key.size())); // removes the string
                version = std::stoi(objects[0]);
                objects.erase(objects.begin()); // pop the header
            }

            for (const std::string& object : objects) {
                std::vector<std::string> object_data;
                boost::split(object_data, object, boost::is_any_of("|"), boost::token_compress_off);

                if (object_data.size() != 2) {
                    add_error("Error while reading object data");
                    continue;
                }

                std::vector<std::string> object_data_id;
                boost::split(object_data_id, object_data[0], boost::is_any_of("="), boost::token_compress_off);
                if (object_data_id.size() != 2) {
                    add_error("Error while reading object id");
                    continue;
                }

                int object_id = std::atoi(object_data_id[1].c_str());
                if (object_id == 0) {
                    add_error("Found invalid object id");
                    continue;
                }

                IdToSlaSupportPointsMap::iterator object_item = m_sla_support_points.find(object_id);
                if (object_item != m_sla_support_points.end()) {
                    add_error("Found duplicated SLA support points");
                    continue;
                }

                std::vector<std::string> object_data_points;
                boost::split(object_data_points, object_data[1], boost::is_any_of(" "), boost::token_compress_off);

                std::vector<sla::SupportPoint> sla_support_points;

                if (version == 0) {
                    for (unsigned int i=0; i<object_data_points.size(); i+=3)
                    sla_support_points.emplace_back(float(std::atof(object_data_points[i+0].c_str())),
                                                    float(std::atof(object_data_points[i+1].c_str())),
													float(std::atof(object_data_points[i+2].c_str())),
                                                    0.4f,
                                                    false);
                }
                if (version == 1) {
                    for (unsigned int i=0; i<object_data_points.size(); i+=5)
                    sla_support_points.emplace_back(float(std::atof(object_data_points[i+0].c_str())),
                                                    float(std::atof(object_data_points[i+1].c_str())),
                                                    float(std::atof(object_data_points[i+2].c_str())),
                                                    float(std::atof(object_data_points[i+3].c_str())),
													//FIXME storing boolean as 0 / 1 and importing it as float.
                                                    std::abs(std::atof(object_data_points[i+4].c_str()) - 1.) < EPSILON);
                }

                if (!sla_support_points.empty())
                    m_sla_support_points.insert({ object_id, sla_support_points });
            }
        }
    }

    void _3MF_Importer::_extract_sla_drain_holes_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat)
    {
        if (stat.m_uncomp_size > 0) {
            std::string buffer(size_t(stat.m_uncomp_size), 0);
            mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
            if (res == 0) {
                add_error("Error while reading sla support points data to buffer");
                return;
            }

            if (buffer.back() == '\n')
                buffer.pop_back();

            std::vector<std::string> objects;
            boost::split(objects, buffer, boost::is_any_of("\n"), boost::token_compress_off);

            // 格式版本信息 - 参见3mf.hpp
            int version = 0;
            std::string key("drain_holes_format_version=");
            if (!objects.empty() && objects[0].find(key) != std::string::npos) {
                objects[0].erase(objects[0].begin(), objects[0].begin() + long(key.size())); // removes the string
                version = std::stoi(objects[0]);
                objects.erase(objects.begin()); // pop the header
            }

            for (const std::string& object : objects) {
                std::vector<std::string> object_data;
                boost::split(object_data, object, boost::is_any_of("|"), boost::token_compress_off);

                if (object_data.size() != 2) {
                    add_error("Error while reading object data");
                    continue;
                }

                std::vector<std::string> object_data_id;
                boost::split(object_data_id, object_data[0], boost::is_any_of("="), boost::token_compress_off);
                if (object_data_id.size() != 2) {
                    add_error("Error while reading object id");
                    continue;
                }

                int object_id = std::atoi(object_data_id[1].c_str());
                if (object_id == 0) {
                    add_error("Found invalid object id");
                    continue;
                }

                IdToSlaDrainHolesMap::iterator object_item = m_sla_drain_holes.find(object_id);
                if (object_item != m_sla_drain_holes.end()) {
                    add_error("Found duplicated SLA drain holes");
                    continue;
                }

                std::vector<std::string> object_data_points;
                boost::split(object_data_points, object_data[1], boost::is_any_of(" "), boost::token_compress_off);

                sla::DrainHoles sla_drain_holes;

                if (version == 1) {
                    for (unsigned int i=0; i<object_data_points.size(); i+=8)
                        sla_drain_holes.emplace_back(Vec3f{float(std::atof(object_data_points[i+0].c_str())),
                                                      float(std::atof(object_data_points[i+1].c_str())),
                                                      float(std::atof(object_data_points[i+2].c_str()))},
                                                     Vec3f{float(std::atof(object_data_points[i+3].c_str())),
                                                      float(std::atof(object_data_points[i+4].c_str())),
                                                      float(std::atof(object_data_points[i+5].c_str()))},
                                                      float(std::atof(object_data_points[i+6].c_str())),
                                                      float(std::atof(object_data_points[i+7].c_str())));
                }

                // 孔被保存在网格上方且更深（确实是个坏主意）。
                // 为了兼容性而保留。
                // 将孔放置在网格上并使其更浅以进行补偿。
                // 偏移量为网格上方1毫米。
                for (sla::DrainHole& hole : sla_drain_holes) {
                    hole.pos += hole.normal.normalized();
                    hole.height -= 1.f;
                }

                if (!sla_drain_holes.empty())
                    m_sla_drain_holes.insert({ object_id, sla_drain_holes });
            }
        }
    }

    bool _3MF_Importer::_extract_model_config_from_archive(mz_zip_archive& archive, const mz_zip_archive_file_stat& stat, Model& model)
    {
        if (stat.m_uncomp_size == 0) {
            add_error("Found invalid size");
            return false;
        }

        _destroy_xml_parser();

        m_xml_parser = XML_ParserCreate(nullptr);
        if (m_xml_parser == nullptr) {
            add_error("Unable to create parser");
            return false;
        }

        XML_SetUserData(m_xml_parser, (void*)this);
        XML_SetElementHandler(m_xml_parser, _3MF_Importer::_handle_start_config_xml_element, _3MF_Importer::_handle_end_config_xml_element);

        void* parser_buffer = XML_GetBuffer(m_xml_parser, (int)stat.m_uncomp_size);
        if (parser_buffer == nullptr) {
            add_error("Unable to create buffer");
            return false;
        }

        mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, parser_buffer, (size_t)stat.m_uncomp_size, 0);
        if (res == 0) {
            add_error("Error while reading config data to buffer");
            return false;
        }

        if (!XML_ParseBuffer(m_xml_parser, (int)stat.m_uncomp_size, 1)) {
            char error_buf[1024];
            ::sprintf(error_buf, "Error (%s) while parsing xml file at line %d", XML_ErrorString(XML_GetErrorCode(m_xml_parser)), (int)XML_GetCurrentLineNumber(m_xml_parser));
            add_error(error_buf);
            return false;
        }

        return true;
    }

    void _3MF_Importer::_extract_custom_gcode_per_print_z_from_archive(::mz_zip_archive &archive, const mz_zip_archive_file_stat &stat)
    {
        //if (stat.m_uncomp_size > 0) {
        //    std::string buffer((size_t)stat.m_uncomp_size, 0);
        //    mz_bool res = mz_zip_reader_extract_file_to_mem(&archive, stat.m_filename, (void*)buffer.data(), (size_t)stat.m_uncomp_size, 0);
        //    if (res == 0) {
        //        add_error("Error while reading custom Gcodes per height data to buffer");
        //        return;
        //    }

        //    std::istringstream iss(buffer); // wrap returned xml to istringstream
        //    pt::ptree main_tree;
        //    pt::read_xml(iss, main_tree);

        //    if (main_tree.front().first != "custom_gcodes_per_print_z")
        //        return;
        //    pt::ptree code_tree = main_tree.front().second;

        //    m_model->custom_gcode_per_print_z.gcodes.clear();

        //    for (const auto& code : code_tree) {
        //        if (code.first == "mode") {
        //            pt::ptree tree = code.second;
        //            std::string mode = tree.get<std::string>("<xmlattr>.value");
        //            m_model->custom_gcode_per_print_z.mode = mode == CustomGCode::SingleExtruderMode ? CustomGCode::Mode::SingleExtruder :
        //                                                     mode == CustomGCode::MultiAsSingleMode  ? CustomGCode::Mode::MultiAsSingle  :
        //                                                     CustomGCode::Mode::MultiExtruder;
        //        }
        //        if (code.first != "code")
        //            continue;

        //        pt::ptree tree = code.second;
        //        double print_z          = tree.get<double>      ("<xmlattr>.print_z" );
        //        int extruder            = tree.get<int>         ("<xmlattr>.extruder");
        //        std::string color       = tree.get<std::string> ("<xmlattr>.color"   );

        //        CustomGCode::Type   type;
        //        std::string         extra;
        //        pt::ptree attr_tree = tree.find("<xmlattr>")->second;
        //        if (attr_tree.find("type") == attr_tree.not_found()) {
        //            // It means that data was saved in old version (2.2.0 and older) of PrusaSlicer
        //            // read old data ...
        //            std::string gcode       = tree.get<std::string> ("<xmlattr>.gcode");
        //            // ... and interpret them to the new data
        //            type  = gcode == "M600"           ? CustomGCode::ColorChange :
        //                    gcode == "M601"           ? CustomGCode::PausePrint  :
        //                    gcode == "tool_change"    ? CustomGCode::ToolChange  :   CustomGCode::Custom;
        //            extra = type == CustomGCode::PausePrint ? color :
        //                    type == CustomGCode::Custom     ? gcode : "";
        //        }
        //        else {
        //            type  = static_cast<CustomGCode::Type>(tree.get<int>("<xmlattr>.type"));
        //            extra = tree.get<std::string>("<xmlattr>.extra");
        //        }
        //        m_model->custom_gcode_per_print_z.gcodes.push_back(CustomGCode::Item{print_z, type, extruder, color, extra}) ;
        //    }
        //}
    }

    void _3MF_Importer::_handle_start_model_xml_element(const char* name, const char** attributes)
    {
        if (m_xml_parser == nullptr)
            return;

        bool res = true;
        unsigned int num_attributes = (unsigned int)XML_GetSpecifiedAttributeCount(m_xml_parser);

        if (::strcmp(MODEL_TAG, name) == 0)
            res = _handle_start_model(attributes, num_attributes);
        else if (::strcmp(RESOURCES_TAG, name) == 0)
            res = _handle_start_resources(attributes, num_attributes);
        else if (::strcmp(OBJECT_TAG, name) == 0)
            res = _handle_start_object(attributes, num_attributes);
        else if (::strcmp(MESH_TAG, name) == 0)
            res = _handle_start_mesh(attributes, num_attributes);
        else if (::strcmp(VERTICES_TAG, name) == 0)
            res = _handle_start_vertices(attributes, num_attributes);
        else if (::strcmp(VERTEX_TAG, name) == 0)
            res = _handle_start_vertex(attributes, num_attributes);
        else if (::strcmp(TRIANGLES_TAG, name) == 0)
            res = _handle_start_triangles(attributes, num_attributes);
        else if (::strcmp(TRIANGLE_TAG, name) == 0)
            res = _handle_start_triangle(attributes, num_attributes);
        else if (::strcmp(COMPONENTS_TAG, name) == 0)
            res = _handle_start_components(attributes, num_attributes);
        else if (::strcmp(COMPONENT_TAG, name) == 0)
            res = _handle_start_component(attributes, num_attributes);
        else if (::strcmp(BUILD_TAG, name) == 0)
            res = _handle_start_build(attributes, num_attributes);
        else if (::strcmp(ITEM_TAG, name) == 0)
            res = _handle_start_item(attributes, num_attributes);
        else if (::strcmp(METADATA_TAG, name) == 0)
            res = _handle_start_metadata(attributes, num_attributes);

        if (!res)
            _stop_xml_parser();
    }

    void _3MF_Importer::_handle_end_model_xml_element(const char* name)
    {
        if (m_xml_parser == nullptr)
            return;

        bool res = true;

        if (::strcmp(MODEL_TAG, name) == 0)
            res = _handle_end_model();
        else if (::strcmp(RESOURCES_TAG, name) == 0)
            res = _handle_end_resources();
        else if (::strcmp(OBJECT_TAG, name) == 0)
            res = _handle_end_object();
        else if (::strcmp(MESH_TAG, name) == 0)
            res = _handle_end_mesh();
        else if (::strcmp(VERTICES_TAG, name) == 0)
            res = _handle_end_vertices();
        else if (::strcmp(VERTEX_TAG, name) == 0)
            res = _handle_end_vertex();
        else if (::strcmp(TRIANGLES_TAG, name) == 0)
            res = _handle_end_triangles();
        else if (::strcmp(TRIANGLE_TAG, name) == 0)
            res = _handle_end_triangle();
        else if (::strcmp(COMPONENTS_TAG, name) == 0)
            res = _handle_end_components();
        else if (::strcmp(COMPONENT_TAG, name) == 0)
            res = _handle_end_component();
        else if (::strcmp(BUILD_TAG, name) == 0)
            res = _handle_end_build();
        else if (::strcmp(ITEM_TAG, name) == 0)
            res = _handle_end_item();
        else if (::strcmp(METADATA_TAG, name) == 0)
            res = _handle_end_metadata();

        if (!res)
            _stop_xml_parser();
    }

    void _3MF_Importer::_handle_model_xml_characters(const XML_Char* s, int len)
    {
        m_curr_characters.append(s, len);
    }

    void _3MF_Importer::_handle_start_config_xml_element(const char* name, const char** attributes)
    {
        if (m_xml_parser == nullptr)
            return;

        bool res = true;
        unsigned int num_attributes = (unsigned int)XML_GetSpecifiedAttributeCount(m_xml_parser);

        if (::strcmp(CONFIG_TAG, name) == 0)
            res = _handle_start_config(attributes, num_attributes);
        else if (::strcmp(OBJECT_TAG, name) == 0)
            res = _handle_start_config_object(attributes, num_attributes);
        else if (::strcmp(VOLUME_TAG, name) == 0)
            res = _handle_start_config_volume(attributes, num_attributes);
        else if (::strcmp(MESH_TAG, name) == 0)
            res = _handle_start_config_volume_mesh(attributes, num_attributes);
        else if (::strcmp(METADATA_TAG, name) == 0)
            res = _handle_start_config_metadata(attributes, num_attributes);

        if (!res)
            _stop_xml_parser();
    }

    void _3MF_Importer::_handle_end_config_xml_element(const char* name)
    {
        if (m_xml_parser == nullptr)
            return;

        bool res = true;

        if (::strcmp(CONFIG_TAG, name) == 0)
            res = _handle_end_config();
        else if (::strcmp(OBJECT_TAG, name) == 0)
            res = _handle_end_config_object();
        else if (::strcmp(VOLUME_TAG, name) == 0)
            res = _handle_end_config_volume();
        else if (::strcmp(MESH_TAG, name) == 0)
            res = _handle_end_config_volume_mesh();
        else if (::strcmp(METADATA_TAG, name) == 0)
            res = _handle_end_config_metadata();

        if (!res)
            _stop_xml_parser();
    }

    bool _3MF_Importer::_handle_start_model(const char** attributes, unsigned int num_attributes)
    {
        m_unit_factor = get_unit_factor(get_attribute_value_string(attributes, num_attributes, UNIT_ATTR));
        return true;
    }

    bool _3MF_Importer::_handle_end_model()
    {
        // deletes all non-built or non-instanced objects
        for (const IdToModelObjectMap::value_type& object : m_objects) {
            if (object.second >= int(m_model->objects.size())) {
                add_error("Unable to find object");
                return false;
            }
            ModelObject *model_object = m_model->objects[object.second];
            if (model_object != nullptr && model_object->instances.size() == 0)
                m_model->delete_object(model_object);
        }

        if (m_version == 0) {
            // if the 3mf was not produced by PrusaSlicer and there is only one object,
            // set the object name to match the filename
            if (m_model->objects.size() == 1)
                m_model->objects.front()->name = m_name;
        }

        // applies instances' matrices
        for (Instance& instance : m_instances) {
            if (instance.instance != nullptr && instance.instance->get_object() != nullptr)
                // apply the transform to the instance
                _apply_transform(*instance.instance, instance.transform);
        }

        return true;
    }

    bool _3MF_Importer::_handle_start_resources(const char** attributes, unsigned int num_attributes)
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_end_resources()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_object(const char** attributes, unsigned int num_attributes)
    {
        // reset current data
        m_curr_object.reset();

        if (is_valid_object_type(get_attribute_value_string(attributes, num_attributes, TYPE_ATTR))) {
            // create new object (it may be removed later if no instances are generated from it)
            m_curr_object.model_object_idx = (int)m_model->objects.size();
            m_curr_object.object = m_model->add_object();
            if (m_curr_object.object == nullptr) {
                add_error("Unable to create object");
                return false;
            }

            // set object data
            m_curr_object.object->name = get_attribute_value_string(attributes, num_attributes, NAME_ATTR);
            if (m_curr_object.object->name.empty())
                m_curr_object.object->name = m_name + "_" + std::to_string(m_model->objects.size());

            m_curr_object.id = get_attribute_value_int(attributes, num_attributes, ID_ATTR);
        }

        return true;
    }

    bool _3MF_Importer::_handle_end_object()
    {
        if (m_curr_object.object != nullptr) {
            if (m_curr_object.geometry.empty()) {
                // no geometry defined
                // remove the object from the model
                m_model->delete_object(m_curr_object.object);

                if (m_curr_object.components.empty()) {
                    // no components defined -> invalid object, delete it
                    IdToModelObjectMap::iterator object_item = m_objects.find(m_curr_object.id);
                    if (object_item != m_objects.end())
                        m_objects.erase(object_item);

                    IdToAliasesMap::iterator alias_item = m_objects_aliases.find(m_curr_object.id);
                    if (alias_item != m_objects_aliases.end())
                        m_objects_aliases.erase(alias_item);
                }
                else
                    // adds components to aliases
                    m_objects_aliases.insert({ m_curr_object.id, m_curr_object.components });
            }
            else {
                // geometry defined, store it for later use
                m_geometries.insert({ m_curr_object.id, std::move(m_curr_object.geometry) });

                // stores the object for later use
                if (m_objects.find(m_curr_object.id) == m_objects.end()) {
                    m_objects.insert({ m_curr_object.id, m_curr_object.model_object_idx });
                    m_objects_aliases.insert({ m_curr_object.id, { 1, Component(m_curr_object.id) } }); // aliases itself
                }
                else {
                    add_error("Found object with duplicate id");
                    return false;
                }
            }
        }

        return true;
    }

    bool _3MF_Importer::_handle_start_mesh(const char** attributes, unsigned int num_attributes)
    {
        // reset current geometry
        m_curr_object.geometry.reset();
        return true;
    }

    bool _3MF_Importer::_handle_end_mesh()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_vertices(const char** attributes, unsigned int num_attributes)
    {
        // reset current vertices
        m_curr_object.geometry.vertices.clear();
        return true;
    }

    bool _3MF_Importer::_handle_end_vertices()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_vertex(const char** attributes, unsigned int num_attributes)
    {
        // appends the vertex coordinates
        // missing values are set equal to ZERO
        m_curr_object.geometry.vertices.emplace_back(
            m_unit_factor * get_attribute_value_float(attributes, num_attributes, X_ATTR),
            m_unit_factor * get_attribute_value_float(attributes, num_attributes, Y_ATTR),
            m_unit_factor * get_attribute_value_float(attributes, num_attributes, Z_ATTR));
        return true;
    }

    bool _3MF_Importer::_handle_end_vertex()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_triangles(const char** attributes, unsigned int num_attributes)
    {
        // reset current triangles
        m_curr_object.geometry.triangles.clear();
        return true;
    }

    bool _3MF_Importer::_handle_end_triangles()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_triangle(const char** attributes, unsigned int num_attributes)
    {
        // we are ignoring the following attributes:
        // p1
        // p2
        // p3
        // pid
        // see specifications

        // appends the triangle's vertices indices
        // missing values are set equal to ZERO
        m_curr_object.geometry.triangles.emplace_back(
            get_attribute_value_int(attributes, num_attributes, V1_ATTR),
            get_attribute_value_int(attributes, num_attributes, V2_ATTR),
            get_attribute_value_int(attributes, num_attributes, V3_ATTR));

        m_curr_object.geometry.custom_supports.push_back(get_attribute_value_string(attributes, num_attributes, CUSTOM_SUPPORTS_ATTR));
        m_curr_object.geometry.custom_seam.push_back(get_attribute_value_string(attributes, num_attributes, CUSTOM_SEAM_ATTR));
        m_curr_object.geometry.fuzzy_skin.push_back(get_attribute_value_string(attributes, num_attributes, FUZZY_SKIN_ATTR));
        m_curr_object.geometry.mmu_segmentation.push_back(get_attribute_value_string(attributes, num_attributes, MMU_SEGMENTATION_ATTR));
        return true;
    }

    bool _3MF_Importer::_handle_end_triangle()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_components(const char** attributes, unsigned int num_attributes)
    {
        // reset current components
        m_curr_object.components.clear();
        return true;
    }

    bool _3MF_Importer::_handle_end_components()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_component(const char** attributes, unsigned int num_attributes)
    {
        int object_id = get_attribute_value_int(attributes, num_attributes, OBJECTID_ATTR);
        Transform3d transform = get_transform_from_3mf_specs_string(get_attribute_value_string(attributes, num_attributes, TRANSFORM_ATTR));

        IdToModelObjectMap::iterator object_item = m_objects.find(object_id);
        if (object_item == m_objects.end()) {
            IdToAliasesMap::iterator alias_item = m_objects_aliases.find(object_id);
            if (alias_item == m_objects_aliases.end()) {
                add_error("Found component with invalid object id");
                return false;
            }
        }

        m_curr_object.components.emplace_back(object_id, transform);

        return true;
    }

    bool _3MF_Importer::_handle_end_component()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_build(const char** attributes, unsigned int num_attributes)
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_end_build()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_item(const char** attributes, unsigned int num_attributes)
    {
        // we are ignoring the following attributes
        // thumbnail
        // partnumber
        // pid
        // pindex
        // see specifications

        int object_id = get_attribute_value_int(attributes, num_attributes, OBJECTID_ATTR);
        Transform3d transform = get_transform_from_3mf_specs_string(get_attribute_value_string(attributes, num_attributes, TRANSFORM_ATTR));
        int printable = get_attribute_value_bool(attributes, num_attributes, PRINTABLE_ATTR);

        return _create_object_instance(object_id, transform, printable, 1);
    }

    bool _3MF_Importer::_handle_end_item()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_metadata(const char** attributes, unsigned int num_attributes)
    {
        m_curr_characters.clear();

        std::string name = get_attribute_value_string(attributes, num_attributes, NAME_ATTR);
        if (!name.empty())
            m_curr_metadata_name = name;

        return true;
    }

    inline static void check_painting_version(unsigned int loaded_version, unsigned int highest_supported_version, const std::string &error_msg)
    {
        if (loaded_version > highest_supported_version)
            throw version_error(error_msg);
    }

    bool _3MF_Importer::_handle_end_metadata()
    {
        if (m_curr_metadata_name == SLIC3RPE_3MF_VERSION) {
            m_version = (unsigned int)atoi(m_curr_characters.c_str());
            if (m_check_version && (m_version > VERSION_3MF_COMPATIBLE)) {
                // std::string msg = _(L("The selected 3mf file has been saved with a newer version of " + std::string(SLIC3R_APP_NAME) + " and is not compatible."));
                // throw version_error(msg.c_str());
                const std::string msg = (boost::format(_(L("The selected 3mf file has been saved with a newer version of %1% and is not compatible."))) % std::string(SLIC3R_APP_NAME)).str();
                throw version_error(msg);
            }
        } else if (m_curr_metadata_name == "Application") {
            // 3MF的生成器应用程序。
            // SLIC3R_APP_KEY - SLIC3R_VERSION
            if (boost::starts_with(m_curr_characters, "PrusaSlicer-"))
                m_prusaslicer_generator_version = Semver::parse(m_curr_characters.substr(12));
        } else if (m_curr_metadata_name == SLIC3RPE_FDM_SUPPORTS_PAINTING_VERSION) {
            m_fdm_supports_painting_version = (unsigned int) atoi(m_curr_characters.c_str());
            check_painting_version(m_fdm_supports_painting_version, FDM_SUPPORTS_PAINTING_VERSION,
                _(L("The selected 3MF contains FDM supports painted object using a newer version of PrusaSlicer and is not compatible.")));
        } else if (m_curr_metadata_name == SLIC3RPE_SEAM_PAINTING_VERSION) {
            m_seam_painting_version = (unsigned int) atoi(m_curr_characters.c_str());
            check_painting_version(m_seam_painting_version, SEAM_PAINTING_VERSION,
                _(L("The selected 3MF contains seam painted object using a newer version of PrusaSlicer and is not compatible.")));
        } else if (m_curr_metadata_name == SLIC3RPE_MM_PAINTING_VERSION) {
            m_mm_painting_version = (unsigned int) atoi(m_curr_characters.c_str());
            check_painting_version(m_mm_painting_version, MM_PAINTING_VERSION,
                _(L("The selected 3MF contains multi-material painted object using a newer version of PrusaSlicer and is not compatible.")));
        }

        return true;
    }

    bool _3MF_Importer::_create_object_instance(int object_id, const Transform3d& transform, const bool printable, unsigned int recur_counter)
    {
        static const unsigned int MAX_RECURSIONS = 10;

        // escape from circular aliasing
        if (recur_counter > MAX_RECURSIONS) {
            add_error("Too many recursions");
            return false;
        }

        IdToAliasesMap::iterator it = m_objects_aliases.find(object_id);
        if (it == m_objects_aliases.end()) {
            add_error("Found item with invalid object id");
            return false;
        }

        if (it->second.size() == 1 && it->second[0].object_id == object_id) {
            // aliasing to itself

            IdToModelObjectMap::iterator object_item = m_objects.find(object_id);
            if (object_item == m_objects.end() || object_item->second == -1) {
                add_error("Found invalid object");
                return false;
            }
            else {
                ModelInstance* instance = m_model->objects[object_item->second]->add_instance();
                if (instance == nullptr) {
                    add_error("Unable to add object instance");
                    return false;
                }
                instance->printable = printable;

                m_instances.emplace_back(instance, transform);
            }
        }
        else {
            // recursively process nested components
            for (const Component& component : it->second) {
                if (!_create_object_instance(component.object_id, transform * component.transform, printable, recur_counter + 1))
                    return false;
            }
        }

        return true;
    }

    void _3MF_Importer::_apply_transform(ModelInstance& instance, const Transform3d& transform)
    {
        Slic3r::Geometry::Transformation t(transform);
        // invalid scale value, return
        if (!t.get_scaling_factor().all())
            return;

        instance.set_transformation(t);
    }

    bool _3MF_Importer::_handle_start_config(const char** attributes, unsigned int num_attributes)
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_end_config()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_config_object(const char** attributes, unsigned int num_attributes)
    {
        int object_id = get_attribute_value_int(attributes, num_attributes, ID_ATTR);
        IdToMetadataMap::iterator object_item = m_objects_metadata.find(object_id);
        if (object_item != m_objects_metadata.end()) {
            add_error("Found duplicated object id");
            return false;
        }

        // 因github #3435添加，目前未被PrusaSlicer使用
        // int instances_count_id = get_attribute_value_int(attributes, num_attributes, INSTANCESCOUNT_ATTR);

        m_objects_metadata.insert({ object_id, ObjectMetadata() });
        m_curr_config.object_id = object_id;
        return true;
    }

    bool _3MF_Importer::_handle_end_config_object()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_config_volume(const char** attributes, unsigned int num_attributes)
    {
        IdToMetadataMap::iterator object = m_objects_metadata.find(m_curr_config.object_id);
        if (object == m_objects_metadata.end()) {
            add_error("Cannot assign volume to a valid object");
            return false;
        }

        m_curr_config.volume_id = (int)object->second.volumes.size();

        unsigned int first_triangle_id = (unsigned int)get_attribute_value_int(attributes, num_attributes, FIRST_TRIANGLE_ID_ATTR);
        unsigned int last_triangle_id = (unsigned int)get_attribute_value_int(attributes, num_attributes, LAST_TRIANGLE_ID_ATTR);

        object->second.volumes.emplace_back(first_triangle_id, last_triangle_id);
        return true;
    }

    bool _3MF_Importer::_handle_start_config_volume_mesh(const char** attributes, unsigned int num_attributes)
    {
        IdToMetadataMap::iterator object = m_objects_metadata.find(m_curr_config.object_id);
        if (object == m_objects_metadata.end()) {
            add_error("Cannot assign volume mesh to a valid object");
            return false;
        }
        if (object->second.volumes.empty()) {
            add_error("Cannot assign mesh to a valid olume");
            return false;
        }

        ObjectMetadata::VolumeMetadata& volume = object->second.volumes.back();

        int edges_fixed         = get_attribute_value_int(attributes, num_attributes, MESH_STAT_EDGES_FIXED       );
        int degenerate_facets   = get_attribute_value_int(attributes, num_attributes, MESH_STAT_DEGENERATED_FACETS);
        int facets_removed      = get_attribute_value_int(attributes, num_attributes, MESH_STAT_FACETS_REMOVED    );
        int facets_reversed     = get_attribute_value_int(attributes, num_attributes, MESH_STAT_FACETS_RESERVED   );
        int backwards_edges     = get_attribute_value_int(attributes, num_attributes, MESH_STAT_BACKWARDS_EDGES   );

        volume.mesh_stats = { edges_fixed, degenerate_facets, facets_removed, facets_reversed, backwards_edges };

        return true;
    }

    bool _3MF_Importer::_handle_end_config_volume()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_end_config_volume_mesh()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_handle_start_config_metadata(const char** attributes, unsigned int num_attributes)
    {
        IdToMetadataMap::iterator object = m_objects_metadata.find(m_curr_config.object_id);
        if (object == m_objects_metadata.end()) {
            add_error("Cannot assign metadata to valid object id");
            return false;
        }

        std::string type = get_attribute_value_string(attributes, num_attributes, TYPE_ATTR);
        std::string key = get_attribute_value_string(attributes, num_attributes, KEY_ATTR);
        std::string value = get_attribute_value_string(attributes, num_attributes, VALUE_ATTR);

        // filter the prusa model config keys
        std::vector<std::string> valid_keys = {
            "name",
            "volume_type",
            "matrix",
            "source_file",
            "source_object_id",
            "source_volume_id",
            "source_offset_x",
            "source_offset_y",
            "source_offset_z",
            "extruder",
            "modifier"
        };

        auto itor = std::find(valid_keys.begin(), valid_keys.end(), key);
        if (itor == valid_keys.end()) {
            // do nothing if not valid keys
            return true;
        }

        if (type == OBJECT_TYPE)
            object->second.metadata.emplace_back(key, value);
        else if (type == VOLUME_TYPE) {
            if (size_t(m_curr_config.volume_id) < object->second.volumes.size())
                object->second.volumes[m_curr_config.volume_id].metadata.emplace_back(key, value);
        }
        else {
            add_error("Found invalid metadata type");
            return false;
        }

        return true;
    }

    bool _3MF_Importer::_handle_end_config_metadata()
    {
        // do nothing
        return true;
    }

    bool _3MF_Importer::_generate_volumes(ModelObject& object, const Geometry& geometry, const ObjectMetadata::VolumeMetadataList& volumes, ConfigSubstitutionContext& config_substitutions)
    {
        if (!object.volumes.empty()) {
            add_error("Found invalid volumes count");
            return false;
        }

        unsigned int geo_tri_count = (unsigned int)geometry.triangles.size();
        unsigned int renamed_volumes_count = 0;

        for (const ObjectMetadata::VolumeMetadata& volume_data : volumes) {
            if (geo_tri_count <= volume_data.first_triangle_id || geo_tri_count <= volume_data.last_triangle_id || volume_data.last_triangle_id < volume_data.first_triangle_id) {
                add_error("Found invalid triangle id");
                return false;
            }

            Transform3d volume_matrix_to_object = Transform3d::Identity();
            bool        has_transform 		    = false;
            // extract the volume transformation from the volume's metadata, if present
            for (const Metadata& metadata : volume_data.metadata) {
                if (metadata.key == MATRIX_KEY) {
                    volume_matrix_to_object = Slic3r::Geometry::transform3d_from_string(metadata.value);
                    has_transform 			= ! volume_matrix_to_object.isApprox(Transform3d::Identity(), 1e-10);
                    break;
                }
            }

            // splits volume out of imported geometry
            indexed_triangle_set its;
            its.indices.assign(geometry.triangles.begin() + volume_data.first_triangle_id, geometry.triangles.begin() + volume_data.last_triangle_id + 1);
            const size_t triangles_count = its.indices.size();
            if (triangles_count == 0) {
                add_error("An empty triangle mesh found");
                return false;
            }

            {
                int min_id = its.indices.front()[0];
                int max_id = min_id;
                for (const Vec3i32& face : its.indices) {
                    for (const int tri_id : face) {
                        if (tri_id < 0 || tri_id >= int(geometry.vertices.size())) {
                            add_error("Found invalid vertex id");
                            return false;
                        }
                        min_id = std::min(min_id, tri_id);
                        max_id = std::max(max_id, tri_id);
                    }
                }
                its.vertices.assign(geometry.vertices.begin() + min_id, geometry.vertices.begin() + max_id + 1);

                // rebase indices to the current vertices list
                for (Vec3i32& face : its.indices)
                    for (int& tri_id : face)
                        tri_id -= min_id;
            }

            if (m_prusaslicer_generator_version &&
                *m_prusaslicer_generator_version >= *Semver::parse("2.4.0-alpha1") &&
                *m_prusaslicer_generator_version < *Semver::parse("2.4.0-alpha3"))
                // PrusaSlicer 2.4.0-alpha2 包含一个错误，其中对象的所有顶点都针对该对象包含的每个体积进行了保存。
                // 删除未被任何面引用的顶点。
                its_compactify_vertices(its, true);

            TriangleMesh triangle_mesh(std::move(its), volume_data.mesh_stats);

            if (m_version == 0) {
                // if the 3mf was not produced by PrusaSlicer and there is only one instance,
                // bake the transformation into the geometry to allow the reload from disk command
                // to work properly
                if (object.instances.size() == 1) {
                    triangle_mesh.transform(object.instances.front()->get_transformation().get_matrix(), false);
                    object.instances.front()->set_transformation(Slic3r::Geometry::Transformation());
                    //FIXME do the mesh fixing?
                }
            }
            if (triangle_mesh.volume() < 0)
                triangle_mesh.flip_triangles();

			ModelVolume* volume = object.add_volume(std::move(triangle_mesh));
            // stores the volume matrix taken from the metadata, if present
            if (has_transform)
                volume->source.transform = Slic3r::Geometry::Transformation(volume_matrix_to_object);

            // recreate custom supports, seam, mm segmentation and fuzzy skin from previously loaded attribute
            volume->supported_facets.reserve(triangles_count);
            volume->seam_facets.reserve(triangles_count);
            volume->mmu_segmentation_facets.reserve(triangles_count);
            volume->fuzzy_skin_facets.reserve(triangles_count);
            for (size_t i=0; i<triangles_count; ++i) {
                size_t index = volume_data.first_triangle_id + i;
                assert(index < geometry.custom_supports.size());
                assert(index < geometry.custom_seam.size());
                assert(index < geometry.mmu_segmentation.size());
                if (! geometry.custom_supports[index].empty())
                    volume->supported_facets.set_triangle_from_string(i, geometry.custom_supports[index]);
                if (! geometry.custom_seam[index].empty())
                    volume->seam_facets.set_triangle_from_string(i, geometry.custom_seam[index]);
                if (! geometry.mmu_segmentation[index].empty())
                    volume->mmu_segmentation_facets.set_triangle_from_string(i, geometry.mmu_segmentation[index]);
                if (! geometry.fuzzy_skin[index].empty())
                	volume->fuzzy_skin_facets.set_triangle_from_string(i, geometry.fuzzy_skin[index]);
            }
            volume->supported_facets.shrink_to_fit();
            volume->seam_facets.shrink_to_fit();
            volume->mmu_segmentation_facets.shrink_to_fit();
            volume->fuzzy_skin_facets.shrink_to_fit();

            // apply the remaining volume's metadata
            for (const Metadata& metadata : volume_data.metadata) {
                if (metadata.key == NAME_KEY)
                    volume->name = metadata.value;
                else if ((metadata.key == MODIFIER_KEY) && (metadata.value == "1"))
					volume->set_type(ModelVolumeType::PARAMETER_MODIFIER);
                else if (metadata.key == VOLUME_TYPE_KEY)
                    volume->set_type(type_from_string(metadata.value));
                else if (metadata.key == SOURCE_FILE_KEY)
                    volume->source.input_file = metadata.value;
                else if (metadata.key == SOURCE_OBJECT_ID_KEY)
                    volume->source.object_idx = ::atoi(metadata.value.c_str());
                else if (metadata.key == SOURCE_VOLUME_ID_KEY)
                    volume->source.volume_idx = ::atoi(metadata.value.c_str());
                else if (metadata.key == SOURCE_OFFSET_X_KEY)
                    volume->source.mesh_offset(0) = ::atof(metadata.value.c_str());
                else if (metadata.key == SOURCE_OFFSET_Y_KEY)
                    volume->source.mesh_offset(1) = ::atof(metadata.value.c_str());
                else if (metadata.key == SOURCE_OFFSET_Z_KEY)
                    volume->source.mesh_offset(2) = ::atof(metadata.value.c_str());
                else if (metadata.key == SOURCE_IN_INCHES)
                    volume->source.is_converted_from_inches = metadata.value == "1";
                else if (metadata.key == SOURCE_IN_METERS)
                    volume->source.is_converted_from_meters = metadata.value == "1";
                else
                    volume->config.set_deserialize(metadata.key, metadata.value, config_substitutions);
            }

            // this may happen for 3mf saved by 3rd part softwares
            if (volume->name.empty()) {
                volume->name = object.name;
                if (renamed_volumes_count > 0)
                    volume->name += "_" + std::to_string(renamed_volumes_count + 1);
                ++renamed_volumes_count;
            }
        }

        return true;
    }

    void XMLCALL _3MF_Importer::_handle_start_model_xml_element(void* userData, const char* name, const char** attributes)
    {
        _3MF_Importer* importer = (_3MF_Importer*)userData;
        if (importer != nullptr)
            importer->_handle_start_model_xml_element(name, attributes);
    }

    void XMLCALL _3MF_Importer::_handle_end_model_xml_element(void* userData, const char* name)
    {
        _3MF_Importer* importer = (_3MF_Importer*)userData;
        if (importer != nullptr)
            importer->_handle_end_model_xml_element(name);
    }

    void XMLCALL _3MF_Importer::_handle_model_xml_characters(void* userData, const XML_Char* s, int len)
    {
        _3MF_Importer* importer = (_3MF_Importer*)userData;
        if (importer != nullptr)
            importer->_handle_model_xml_characters(s, len);
    }

    void XMLCALL _3MF_Importer::_handle_start_config_xml_element(void* userData, const char* name, const char** attributes)
    {
        _3MF_Importer* importer = (_3MF_Importer*)userData;
        if (importer != nullptr)
            importer->_handle_start_config_xml_element(name, attributes);
    }

    void XMLCALL _3MF_Importer::_handle_end_config_xml_element(void* userData, const char* name)
    {
        _3MF_Importer* importer = (_3MF_Importer*)userData;
        if (importer != nullptr)
            importer->_handle_end_config_xml_element(name);
    }

    class _3MF_Exporter : public _3MF_Base
    {
        struct BuildItem
        {
            unsigned int id;
            Transform3d transform;
            bool printable;

            BuildItem(unsigned int id, const Transform3d& transform, const bool printable)
                : id(id)
                , transform(transform)
                , printable(printable)
            {
            }
        };

        struct Offsets
        {
            unsigned int first_vertex_id;
            unsigned int first_triangle_id;
            unsigned int last_triangle_id;

            Offsets(unsigned int first_vertex_id)
                : first_vertex_id(first_vertex_id)
                , first_triangle_id(-1)
                , last_triangle_id(-1)
            {
            }
        };

        typedef std::map<const ModelVolume*, Offsets> VolumeToOffsetsMap;

        struct ObjectData
        {
            ModelObject* object;
            VolumeToOffsetsMap volumes_offsets;

            explicit ObjectData(ModelObject* object)
                : object(object)
            {
            }
        };

        typedef std::vector<BuildItem> BuildItemsList;
        typedef std::map<int, ObjectData> IdToObjectDataMap;

        bool m_fullpath_sources{ true };
        bool m_zip64 { true };

    public:
        bool save_model_to_file(const std::string& filename, Model& model, const DynamicPrintConfig* config, bool fullpath_sources, const ThumbnailData* thumbnail_data, bool zip64);

    private:
        bool _save_model_to_file(const std::string& filename, Model& model, const DynamicPrintConfig* config, const ThumbnailData* thumbnail_data);
        bool _add_content_types_file_to_archive(mz_zip_archive& archive);
        bool _add_thumbnail_file_to_archive(mz_zip_archive& archive, const ThumbnailData& thumbnail_data);
        bool _add_relationships_file_to_archive(mz_zip_archive& archive);
        bool _add_model_file_to_archive(const std::string& filename, mz_zip_archive& archive, const Model& model, IdToObjectDataMap& objects_data);
        bool _add_object_to_model_stream(mz_zip_writer_staged_context &context, unsigned int& object_id, ModelObject& object, BuildItemsList& build_items, VolumeToOffsetsMap& volumes_offsets);
        bool _add_mesh_to_object_stream(mz_zip_writer_staged_context &context, ModelObject& object, VolumeToOffsetsMap& volumes_offsets);
        bool _add_build_to_model_stream(std::stringstream& stream, const BuildItemsList& build_items);
        bool _add_layer_height_profile_file_to_archive(mz_zip_archive& archive, Model& model);
        bool _add_layer_config_ranges_file_to_archive(mz_zip_archive& archive, Model& model);
        bool _add_sla_support_points_file_to_archive(mz_zip_archive& archive, Model& model);
        bool _add_sla_drain_holes_file_to_archive(mz_zip_archive& archive, Model& model);
        bool _add_print_config_file_to_archive(mz_zip_archive& archive, const DynamicPrintConfig &config);
        bool _add_model_config_file_to_archive(mz_zip_archive& archive, const Model& model, const IdToObjectDataMap &objects_data);
        bool _add_custom_gcode_per_print_z_file_to_archive(mz_zip_archive& archive, Model& model, const DynamicPrintConfig* config);
    };

    bool _3MF_Exporter::save_model_to_file(const std::string& filename, Model& model, const DynamicPrintConfig* config, bool fullpath_sources, const ThumbnailData* thumbnail_data, bool zip64)
    {
        clear_errors();
        m_fullpath_sources = fullpath_sources;
        m_zip64 = zip64;
        return _save_model_to_file(filename, model, config, thumbnail_data);
    }

    bool _3MF_Exporter::_save_model_to_file(const std::string& filename, Model& model, const DynamicPrintConfig* config, const ThumbnailData* thumbnail_data)
    {
        mz_zip_archive archive;
        mz_zip_zero_struct(&archive);

        if (!open_zip_writer(&archive, filename)) {
            add_error("Unable to open the file");
            return false;
        }

        // 添加内容类型文件（"[Content_Types].xml"）。
        // 此文件的内容对于每个PrusaSlicer 3mf都是相同的。
        if (!_add_content_types_file_to_archive(archive)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        if (thumbnail_data != nullptr && thumbnail_data->is_valid()) {
            // 添加Metadata/thumbnail.png文件。
            if (!_add_thumbnail_file_to_archive(archive, *thumbnail_data)) {
                close_zip_writer(&archive);
                boost::filesystem::remove(filename);
                return false;
            }
        }

        // 添加关系文件（"_rels/.rels"）。
        // 此文件的内容对于每个PrusaSlicer 3mf都是相同的。
        // 关系文件包含对几何文件"3D/3dmodel.model"的引用，该名称选择为与CURA兼容。
        if (!_add_relationships_file_to_archive(archive)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        // 添加模型文件（"3D/3dmodel.model"）。
        // 这是唯一包含所有ModelVolume的所有几何数据（顶点和三角形）的文件。
        IdToObjectDataMap objects_data;
        if (!_add_model_file_to_archive(filename, archive, model, objects_data)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        // 添加层高配置文件（"Metadata/Slic3r_PE_layer_heights_profile.txt"）。
        // 所有ModelObject的所有层高配置文件都存储在这里，由Model中基于1的ModelObject索引进行索引。
        // 该索引不同于3MF文件对象实例的对象ID索引！
        if (!_add_layer_height_profile_file_to_archive(archive, model)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        // 添加层配置范围文件（"Metadata/Slic3r_PE_layer_config_ranges.txt"）。
        // 所有ModelObject的所有层高配置文件都存储在这里，由Model中基于1的ModelObject索引进行索引。
        // 该索引不同于3MF文件对象实例的对象ID索引！
        if (!_add_layer_config_ranges_file_to_archive(archive, model)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        // 添加sla支撑点文件（"Metadata/Slic3r_PE_sla_support_points.txt"）。
        // 所有ModelObject的所有sla支撑点都存储在这里，由Model中基于1的ModelObject索引进行索引。
        // 该索引不同于3MF文件对象实例的对象ID索引！
        if (!_add_sla_support_points_file_to_archive(archive, model)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        if (!_add_sla_drain_holes_file_to_archive(archive, model)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }


        // 添加自定义GCode按高度文件（"Metadata/Prusa_Slicer_custom_gcode_per_print_z.xml"）。
        // 整个模型的所有自定义GCode按高度都存储在这里
        if (!_add_custom_gcode_per_print_z_file_to_archive(archive, model, config)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        // 添加slic3r打印配置文件（"Metadata/Slic3r_PE.config"）。
        // 此文件包含FullPrintConfig / SLAFullPrintConfig的内容。
        if (config != nullptr) {
            if (!_add_print_config_file_to_archive(archive, *config)) {
                close_zip_writer(&archive);
                boost::filesystem::remove(filename);
                return false;
            }
        }

        // 添加slic3r模型配置文件（"Metadata/Slic3r_PE_model.config"）。
        // 此文件包含所有ModelObject及其ModelVolume的所有属性（名称、参数覆盖）。
        // 由于每个ModelObject仅存储一个索引三角形集数据，体积在其各自索引三角形集数据中的偏移量
        // 也存储在这里。
        if (!_add_model_config_file_to_archive(archive, model, objects_data)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            return false;
        }

        if (!mz_zip_writer_finalize_archive(&archive)) {
            close_zip_writer(&archive);
            boost::filesystem::remove(filename);
            add_error("Unable to finalize the archive");
            return false;
        }

        close_zip_writer(&archive);

        return true;
    }

    bool _3MF_Exporter::_add_content_types_file_to_archive(mz_zip_archive& archive)
    {
        std::stringstream stream;
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        stream << "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n";
        stream << " <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n";
        stream << " <Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n";
        stream << " <Default Extension=\"png\" ContentType=\"image/png\"/>\n";
        stream << "</Types>";

        std::string out = stream.str();

        if (!mz_zip_writer_add_mem(&archive, CONTENT_TYPES_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
            add_error("Unable to add content types file to archive");
            return false;
        }

        return true;
    }

    bool _3MF_Exporter::_add_thumbnail_file_to_archive(mz_zip_archive& archive, const ThumbnailData& thumbnail_data)
    {
        bool res = false;

        size_t png_size = 0;
        void* png_data = tdefl_write_image_to_png_file_in_memory_ex((const void*)thumbnail_data.pixels.data(), thumbnail_data.width, thumbnail_data.height, 4, &png_size, MZ_DEFAULT_LEVEL, 1);
        if (png_data != nullptr) {
            res = mz_zip_writer_add_mem(&archive, THUMBNAIL_FILE.c_str(), (const void*)png_data, png_size, MZ_DEFAULT_COMPRESSION);
            mz_free(png_data);
        }

        if (!res)
            add_error("Unable to add thumbnail file to archive");

        return res;
    }

    bool _3MF_Exporter::_add_relationships_file_to_archive(mz_zip_archive& archive)
    {
        std::stringstream stream;
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        stream << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
        stream << " <Relationship Target=\"/" << MODEL_FILE << "\" Id=\"rel-1\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>\n";
        stream << " <Relationship Target=\"/" << THUMBNAIL_FILE << "\" Id=\"rel-2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/thumbnail\"/>\n";
        stream << "</Relationships>";

        std::string out = stream.str();

        if (!mz_zip_writer_add_mem(&archive, RELATIONSHIPS_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
            add_error("Unable to add relationships file to archive");
            return false;
        }

        return true;
    }

    static void reset_stream(std::stringstream &stream)
    {
        stream.str("");
        stream.clear();
        // https://en.cppreference.com/w/cpp/types/numeric_limits/max_digits10
        // 只要至少使用了max_digits10位数字（float为9，double为17），浮点数值与文本之间的转换就是精确的。
        // 即使中间文本表示不精确，也能保证产生相同的浮点数值。
        // std::stream precision的默认值只有6位！
        stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    }

    bool _3MF_Exporter::_add_model_file_to_archive(const std::string& filename, mz_zip_archive& archive, const Model& model, IdToObjectDataMap& objects_data)
    {
        mz_zip_writer_staged_context context;
        if (!mz_zip_writer_add_staged_open(&archive, &context, MODEL_FILE.c_str(),
            m_zip64 ?
                // 最大预期和允许的3MF文件大小为16GiB。
                // 这将ZIP文件切换到64位模式，这会为文件记录增加少量开销。
                (uint64_t(1) << 30) * 16 :
                // 最大预期3MF文件大小为4GB-1。这是与Windows 10 3D模型修复API互操作的解决方法，参见
                // GH issue #6193。
                (uint64_t(1) << 32) - 1,
            nullptr, nullptr, 0, MZ_DEFAULT_COMPRESSION, nullptr, 0, nullptr, 0)) {
            add_error("Unable to add model file to archive");
            return false;
        }

        {
            std::stringstream stream;
            reset_stream(stream);
            stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            stream << "<" << MODEL_TAG << " unit=\"millimeter\" xml:lang=\"en-US\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\" xmlns:slic3rpe=\"http://schemas.slic3r.org/3mf/2017/06\">\n";
            stream << " <" << METADATA_TAG << " name=\"" << SLIC3RPE_3MF_VERSION << "\">" << VERSION_3MF << "</" << METADATA_TAG << ">\n";

            if (model.is_fdm_support_painted())
                stream << " <" << METADATA_TAG << " name=\"" << SLIC3RPE_FDM_SUPPORTS_PAINTING_VERSION << "\">" << FDM_SUPPORTS_PAINTING_VERSION << "</" << METADATA_TAG << ">\n";

            if (model.is_seam_painted())
                stream << " <" << METADATA_TAG << " name=\"" << SLIC3RPE_SEAM_PAINTING_VERSION << "\">" << SEAM_PAINTING_VERSION << "</" << METADATA_TAG << ">\n";

            if (model.is_mm_painted())
                stream << " <" << METADATA_TAG << " name=\"" << SLIC3RPE_MM_PAINTING_VERSION << "\">" << MM_PAINTING_VERSION << "</" << METADATA_TAG << ">\n";

            std::string name = xml_escape(boost::filesystem::path(filename).stem().string());
            stream << " <" << METADATA_TAG << " name=\"Title\">" << name << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"Designer\">" << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"Description\">" << name << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"Copyright\">" << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"LicenseTerms\">" << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"Rating\">" << "</" << METADATA_TAG << ">\n";
            // Orca: 隐私：不在3mf中存储创建和修改日期
            stream << " <" << METADATA_TAG << " name=\"CreationDate\">" << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"ModificationDate\">" << "</" << METADATA_TAG << ">\n";
            stream << " <" << METADATA_TAG << " name=\"Application\">" << SLIC3R_APP_KEY << "-" << SLIC3R_VERSION << "</" << METADATA_TAG << ">\n";
            stream << " <" << RESOURCES_TAG << ">\n";
            std::string buf = stream.str();
            if (! buf.empty() && ! mz_zip_writer_add_staged_data(&context, buf.data(), buf.size())) {
                add_error("Unable to add model file to archive");
                return false;
            }
        }

        // 实例变换，由3MF对象ID索引（这是所有ModelObject的所有实例的线性序列化）。
        BuildItemsList build_items;

        // 此处的object_id是3MF文件中ModelObject第一个实例的基于1的标识符，
        // 所有ModelObject的所有对象实例都以基于1的线性方式存储和索引。
        // 因此，此处的object_id列表可能不是连续的。
        unsigned int object_id = 1;
        for (ModelObject* obj : model.objects) {
            if (obj == nullptr)
                continue;

            // 3MF文件中对应ModelObject第一个实例的对象索引。
            unsigned int curr_id = object_id;
            IdToObjectDataMap::iterator object_it = objects_data.insert({ curr_id, ObjectData(obj) }).first;
            // 将单个ModelObject中包含的所有ModelVolume的几何数据存储到单个3MF索引三角形集对象中。
            // object_it->second.volumes_offsets将包含该单个索引三角形集中ModelVolume的偏移量。
            // object_id将递增以指向下一个ModelObject的第一个实例。
            if (!_add_object_to_model_stream(context, object_id, *obj, build_items, object_it->second.volumes_offsets)) {
                add_error("Unable to add object to archive");
                mz_zip_writer_add_staged_finish(&context);
                return false;
            }
        }

        {
            std::stringstream stream;
            reset_stream(stream);
            stream << " </" << RESOURCES_TAG << ">\n";

            // 以线性方式存储所有ModelObject的所有ModelInstance的变换。
            if (!_add_build_to_model_stream(stream, build_items)) {
                add_error("Unable to add build to archive");
                mz_zip_writer_add_staged_finish(&context);
                return false;
            }

            stream << "</" << MODEL_TAG << ">\n";

            std::string buf = stream.str();

            if ((! buf.empty() && ! mz_zip_writer_add_staged_data(&context, buf.data(), buf.size())) ||
                ! mz_zip_writer_add_staged_finish(&context)) {
                add_error("Unable to add model file to archive");
                return false;
            }
        }

        return true;
    }

    bool _3MF_Exporter::_add_object_to_model_stream(mz_zip_writer_staged_context &context, unsigned int& object_id, ModelObject& object, BuildItemsList& build_items, VolumeToOffsetsMap& volumes_offsets)
    {
        std::stringstream stream;
        reset_stream(stream);
        unsigned int id = 0;
        for (const ModelInstance* instance : object.instances) {
			assert(instance != nullptr);
            if (instance == nullptr)
                continue;

            unsigned int instance_id = object_id + id;
            stream << "  <" << OBJECT_TAG << " id=\"" << instance_id << "\" type=\"model\">\n";

            if (id == 0) {
                std::string buf = stream.str();
                reset_stream(stream);
                if ((! buf.empty() && ! mz_zip_writer_add_staged_data(&context, buf.data(), buf.size())) ||
                    ! _add_mesh_to_object_stream(context, object, volumes_offsets)) {
                    add_error("Unable to add mesh to archive");
                    return false;
                }
            }
            else {
                stream << "   <" << COMPONENTS_TAG << ">\n";
                stream << "    <" << COMPONENT_TAG << " objectid=\"" << object_id << "\"/>\n";
                stream << "   </" << COMPONENTS_TAG << ">\n";
            }

            Transform3d t = instance->get_matrix();
            // instance_id只是build_items中基于1的索引。
            assert(instance_id == build_items.size() + 1);
            build_items.emplace_back(instance_id, t, instance->printable);

            stream << "  </" << OBJECT_TAG << ">\n";

            ++id;
        }

        object_id += id;
        std::string buf = stream.str();
        return buf.empty() || mz_zip_writer_add_staged_data(&context, buf.data(), buf.size());
    }

#if EXPORT_3MF_USE_SPIRIT_KARMA_FP
    template <typename Num>
    struct coordinate_policy_fixed : boost::spirit::karma::real_policies<Num>
    {
        static int floatfield(Num n) { return fmtflags::fixed; }
        // 保留浮点精度的十进制位数，以便存储到文本文件并解析回来。
        static unsigned precision(Num /* n */) { return std::numeric_limits<Num>::max_digits10 + 1; }
        // 无尾随零，因此对于fmtflags::fixed格式，通常产生的十进制数字远少于max_digits10。
        static bool trailing_zeros(Num /* n */) { return false; }
    };
    template <typename Num>
    struct coordinate_policy_scientific : coordinate_policy_fixed<Num>
    {
        static int floatfield(Num n) { return fmtflags::scientific; }
    };
    // 基于新的坐标策略定义一个新的生成器类型。
    using coordinate_type_fixed      = boost::spirit::karma::real_generator<float, coordinate_policy_fixed<float>>;
    using coordinate_type_scientific = boost::spirit::karma::real_generator<float, coordinate_policy_scientific<float>>;
#endif // EXPORT_3MF_USE_SPIRIT_KARMA_FP

    bool _3MF_Exporter::_add_mesh_to_object_stream(mz_zip_writer_staged_context &context, ModelObject& object, VolumeToOffsetsMap& volumes_offsets)
    {
        std::string output_buffer;
        output_buffer += "   <";
        output_buffer += MESH_TAG;
        output_buffer += ">\n    <";
        output_buffer += VERTICES_TAG;
        output_buffer += ">\n";

        auto flush = [this, &output_buffer, &context](bool force = false) {
            if ((force && ! output_buffer.empty()) || output_buffer.size() >= 65536 * 16) {
                if (! mz_zip_writer_add_staged_data(&context, output_buffer.data(), output_buffer.size())) {
                    add_error("Error during writing or compression");
                    return false;
                }
                output_buffer.clear();
            }
            return true;
        };

        auto format_coordinate = [](float f, char *buf) -> char* {
            assert(is_decimal_separator_point());
#if EXPORT_3MF_USE_SPIRIT_KARMA_FP
            // 比sprintf("%.9g")略快，但karma浮点数格式化器存在问题，
            // https://github.com/boostorg/spirit/pull/586
            // 导出的字符串比保证无损往返所需的长度少一位。
            // 此处代码保留以备boost改进时使用。
            coordinate_type_fixed      const coordinate_fixed      = coordinate_type_fixed();
            coordinate_type_scientific const coordinate_scientific = coordinate_type_scientific();
            // 以固定格式"f"格式化。
            char *ptr = buf;
            boost::spirit::karma::generate(ptr, coordinate_fixed, f);
            // 以科学计数法格式"f"格式化。
            char *ptr2 = ptr;
            boost::spirit::karma::generate(ptr2, coordinate_scientific, f);
            // 返回较短字符串的结尾。
            auto len2 = ptr2 - ptr;
            if (ptr - buf > len2) {
                // 将较短的科学计数法形式移到前面。
                memcpy(buf, ptr, len2);
                ptr = buf + len2;
            }
            // 返回指向结尾的指针。
            return ptr;
#else
            // 可往返转换的浮点数，尽可能短。
            return buf + sprintf(buf, "%.9g", f);
#endif
        };

        char buf[256];
        unsigned int vertices_count = 0;
        for (ModelVolume* volume : object.volumes) {
            if (volume == nullptr)
                continue;

            volumes_offsets.insert({ volume, Offsets(vertices_count) });

            const indexed_triangle_set &its = volume->mesh().its;
            if (its.vertices.empty()) {
                add_error("Found invalid mesh");
                return false;
            }

            vertices_count += (int)its.vertices.size();

            const Transform3d& matrix = volume->get_matrix();

            for (size_t i = 0; i < its.vertices.size(); ++i) {
                Vec3f v = (matrix * its.vertices[i].cast<double>()).cast<float>();
                char *ptr = buf;
                boost::spirit::karma::generate(ptr, boost::spirit::lit("     <") << VERTEX_TAG << " x=\"");
                ptr = format_coordinate(v.x(), ptr);
                boost::spirit::karma::generate(ptr, "\" y=\"");
                ptr = format_coordinate(v.y(), ptr);
                boost::spirit::karma::generate(ptr, "\" z=\"");
                ptr = format_coordinate(v.z(), ptr);
                boost::spirit::karma::generate(ptr, "\"/>\n");
                *ptr = '\0';
                output_buffer += buf;
                if (! flush())
                    return false;
            }
        }

        output_buffer += "    </";
        output_buffer += VERTICES_TAG;
        output_buffer += ">\n    <";
        output_buffer += TRIANGLES_TAG;
        output_buffer += ">\n";

        unsigned int triangles_count = 0;
        for (ModelVolume* volume : object.volumes) {
            if (volume == nullptr)
                continue;

            bool is_left_handed = volume->is_left_handed();
            VolumeToOffsetsMap::iterator volume_it = volumes_offsets.find(volume);
            assert(volume_it != volumes_offsets.end());

            const indexed_triangle_set &its = volume->mesh().its;

            // updates triangle offsets
            volume_it->second.first_triangle_id = triangles_count;
            triangles_count += (int)its.indices.size();
            volume_it->second.last_triangle_id = triangles_count - 1;

            for (int i = 0; i < int(its.indices.size()); ++ i) {
                {
                    const Vec3i32 &idx = its.indices[i];
                    char *ptr = buf;
                    boost::spirit::karma::generate(ptr, boost::spirit::lit("     <") << TRIANGLE_TAG <<
                        " v1=\"" << boost::spirit::int_ <<
                        "\" v2=\"" << boost::spirit::int_ <<
                        "\" v3=\"" << boost::spirit::int_ << "\"",
                        idx[is_left_handed ? 2 : 0] + volume_it->second.first_vertex_id,
                        idx[1] + volume_it->second.first_vertex_id,
                        idx[is_left_handed ? 0 : 2] + volume_it->second.first_vertex_id);
                    *ptr = '\0';
                    output_buffer += buf;
                }

                std::string custom_supports_data_string = volume->supported_facets.get_triangle_as_string(i);
                if (! custom_supports_data_string.empty()) {
                    output_buffer += " ";
                    output_buffer += CUSTOM_SUPPORTS_ATTR;
                    output_buffer += "=\"";
                    output_buffer += custom_supports_data_string;
                    output_buffer += "\"";
                }

                std::string custom_seam_data_string = volume->seam_facets.get_triangle_as_string(i);
                if (! custom_seam_data_string.empty()) {
                    output_buffer += " ";
                    output_buffer += CUSTOM_SEAM_ATTR;
                    output_buffer += "=\"";
                    output_buffer += custom_seam_data_string;
                    output_buffer += "\"";
                }

                std::string mmu_painting_data_string = volume->mmu_segmentation_facets.get_triangle_as_string(i);
                if (! mmu_painting_data_string.empty()) {
                    output_buffer += " ";
                    output_buffer += MMU_SEGMENTATION_ATTR;
                    output_buffer += "=\"";
                    output_buffer += mmu_painting_data_string;
                    output_buffer += "\"";
                }

                std::string fuzzy_skin_data_string = volume->fuzzy_skin_facets.get_triangle_as_string(i);
                if (!fuzzy_skin_data_string.empty()) {
                    output_buffer += " ";
                    output_buffer += FUZZY_SKIN_ATTR;
                    output_buffer += "=\"";
                    output_buffer += fuzzy_skin_data_string;
                    output_buffer += "\"";
                }

                output_buffer += "/>\n";

                if (! flush())
                    return false;
            }
        }

        output_buffer += "    </";
        output_buffer += TRIANGLES_TAG;
        output_buffer += ">\n   </";
        output_buffer += MESH_TAG;
        output_buffer += ">\n";

        // 强制刷新。
        return flush(true);
    }

    bool _3MF_Exporter::_add_build_to_model_stream(std::stringstream& stream, const BuildItemsList& build_items)
    {
        // 这发生在空项目中
        if (build_items.size() == 0) {
            add_error("No build item found");
            return true;
        }

        stream << " <" << BUILD_TAG << ">\n";

        for (const BuildItem& item : build_items) {
            stream << "  <" << ITEM_TAG << " " << OBJECTID_ATTR << "=\"" << item.id << "\" " << TRANSFORM_ATTR << "=\"";
            for (unsigned c = 0; c < 4; ++c) {
                for (unsigned r = 0; r < 3; ++r) {
                    stream << item.transform(r, c);
                    if (r != 2 || c != 3)
                        stream << " ";
                }
            }
            stream << "\" " << PRINTABLE_ATTR << "=\"" << item.printable << "\"/>\n";
        }

        stream << " </" << BUILD_TAG << ">\n";

        return true;
    }

    bool _3MF_Exporter::_add_layer_height_profile_file_to_archive(mz_zip_archive& archive, Model& model)
    {
        assert(is_decimal_separator_point());
        std::string out = "";
        char buffer[1024];

        unsigned int count = 0;
        for (const ModelObject* object : model.objects) {
            ++count;
            const std::vector<double>& layer_height_profile = object->layer_height_profile.get();
            if (layer_height_profile.size() >= 4 && layer_height_profile.size() % 2 == 0) {
                sprintf(buffer, "object_id=%d|", count);
                out += buffer;

                // 将层高配置文件存储为以分号分隔的单个列表。
                for (size_t i = 0; i < layer_height_profile.size(); ++i) {
                    sprintf(buffer, (i == 0) ? "%f" : ";%f", layer_height_profile[i]);
                    out += buffer;
                }

                out += "\n";
            }
        }

        if (!out.empty()) {
            if (!mz_zip_writer_add_mem(&archive, LAYER_HEIGHTS_PROFILE_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
                add_error("Unable to add layer heights profile file to archive");
                return false;
            }
        }

        return true;
    }

    bool _3MF_Exporter::_add_layer_config_ranges_file_to_archive(mz_zip_archive& archive, Model& model)
    {
        std::string out = "";
        pt::ptree tree;

        unsigned int object_cnt = 0;
        for (const ModelObject* object : model.objects) {
            object_cnt++;
            const t_layer_config_ranges& ranges = object->layer_config_ranges;
            if (!ranges.empty())
            {
                pt::ptree& obj_tree = tree.add("objects.object","");

                obj_tree.put("<xmlattr>.id", object_cnt);

                // 存储层配置范围。
                for (const auto& range : ranges) {
                    pt::ptree& range_tree = obj_tree.add("range", "");

                    // store minX and maxZ
                    range_tree.put("<xmlattr>.min_z", range.first.first);
                    range_tree.put("<xmlattr>.max_z", range.first.second);

                    // store range configuration
                    const ModelConfig& config = range.second;
                    for (const std::string& opt_key : config.keys()) {
                        pt::ptree& opt_tree = range_tree.add("option", config.opt_serialize(opt_key));
                        opt_tree.put("<xmlattr>.opt_key", opt_key);
                    }
                }
            }
        }

        if (!tree.empty()) {
            std::ostringstream oss;
            pt::write_xml(oss, tree);
            out = oss.str();

            // 输出字符串的后处理（美化），以获得更好的预览效果
            boost::replace_all(out, "><object",      ">\n <object");
            boost::replace_all(out, "><range",       ">\n  <range");
            boost::replace_all(out, "><option",      ">\n   <option");
            boost::replace_all(out, "></range>",     ">\n  </range>");
            boost::replace_all(out, "></object>",    ">\n </object>");
            // OR just
            boost::replace_all(out, "><",            ">\n<");
        }

        if (!out.empty()) {
            if (!mz_zip_writer_add_mem(&archive, LAYER_CONFIG_RANGES_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
                add_error("Unable to add layer heights profile file to archive");
                return false;
            }
        }

        return true;
    }

    bool _3MF_Exporter::_add_sla_support_points_file_to_archive(mz_zip_archive& archive, Model& model)
    {
        assert(is_decimal_separator_point());
        std::string out = "";
        char buffer[1024];

        unsigned int count = 0;
        for (const ModelObject* object : model.objects) {
            ++count;
            const std::vector<sla::SupportPoint>& sla_support_points = object->sla_support_points;
            if (!sla_support_points.empty()) {
                sprintf(buffer, "object_id=%d|", count);
                out += buffer;

                // 将层高配置文件存储为以空格分隔的单个列表。
                for (size_t i = 0; i < sla_support_points.size(); ++i) {
                    sprintf(buffer, (i==0 ? "%f %f %f %f %f" : " %f %f %f %f %f"),  sla_support_points[i].pos(0), sla_support_points[i].pos(1), sla_support_points[i].pos(2), sla_support_points[i].head_front_radius, (float)sla_support_points[i].is_new_island);
                    out += buffer;
                }
                out += "\n";
            }
        }

        if (!out.empty()) {
            // 在开头添加版本头：
            out = std::string("support_points_format_version=") + std::to_string(support_points_format_version) + std::string("\n") + out;

            if (!mz_zip_writer_add_mem(&archive, SLA_SUPPORT_POINTS_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
                add_error("Unable to add sla support points file to archive");
                return false;
            }
        }
        return true;
    }

    bool _3MF_Exporter::_add_sla_drain_holes_file_to_archive(mz_zip_archive& archive, Model& model)
    {
        assert(is_decimal_separator_point());
        const char *const fmt = "object_id=%d|";
        std::string out;

        unsigned int count = 0;
        for (const ModelObject* object : model.objects) {
            ++count;
            sla::DrainHoles drain_holes = object->sla_drain_holes;

            // 在最初的实现中，孔被放置在网格上方1mm处。
            // 这是一个糟糕的想法，参考点在2.3中更改为
            // 精确地位于网格上。出于兼容性原因，提升后的位置仍然保存在3MF中。
            for (sla::DrainHole& hole : drain_holes) {
                hole.pos -= hole.normal.normalized();
                hole.height += 1.f;
            }

            if (!drain_holes.empty()) {
                out += string_printf(fmt, count);

                // 将层高配置文件存储为以空格分隔的单个列表。
                for (size_t i = 0; i < drain_holes.size(); ++i)
                    out += string_printf((i == 0 ? "%f %f %f %f %f %f %f %f" : " %f %f %f %f %f %f %f %f"),
                                         drain_holes[i].pos(0),
                                         drain_holes[i].pos(1),
                                         drain_holes[i].pos(2),
                                         drain_holes[i].normal(0),
                                         drain_holes[i].normal(1),
                                         drain_holes[i].normal(2),
                                         drain_holes[i].radius,
                                         drain_holes[i].height);

                out += "\n";
            }
        }

        if (!out.empty()) {
            // 在开头添加版本头：
            out = std::string("drain_holes_format_version=") + std::to_string(drain_holes_format_version) + std::string("\n") + out;

            if (!mz_zip_writer_add_mem(&archive, SLA_DRAIN_HOLES_FILE.c_str(), static_cast<const void*>(out.data()), out.length(), mz_uint(MZ_DEFAULT_COMPRESSION))) {
                add_error("Unable to add sla support points file to archive");
                return false;
            }
        }
        return true;
    }

    bool _3MF_Exporter::_add_print_config_file_to_archive(mz_zip_archive& archive, const DynamicPrintConfig &config)
    {
        assert(is_decimal_separator_point());
        char buffer[1024];
        sprintf(buffer, "; %s\n\n", header_slic3r_generated().c_str());
        std::string out = buffer;

        for (const std::string &key : config.keys())
            if (key != "compatible_printers")
                out += "; " + key + " = " + config.opt_serialize(key) + "\n";

        if (!out.empty()) {
            if (!mz_zip_writer_add_mem(&archive, PRINT_CONFIG_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
                add_error("Unable to add print config file to archive");
                return false;
            }
        }

        return true;
    }

    bool _3MF_Exporter::_add_model_config_file_to_archive(mz_zip_archive& archive, const Model& model, const IdToObjectDataMap &objects_data)
    {
        std::stringstream stream;
        // 以全精度存储网格变换，因为体积是以变换后的形式存储的，需要在加载时尽可能精确地变换回来。
		stream << std::setprecision(std::numeric_limits<double>::max_digits10);
        stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        stream << "<" << CONFIG_TAG << ">\n";

        for (const IdToObjectDataMap::value_type& obj_metadata : objects_data) {
            const ModelObject* obj = obj_metadata.second.object;
            if (obj != nullptr) {
                // 因github #3435添加的实例数量输出，目前未被PrusaSlicer使用
                stream << " <" << OBJECT_TAG << " " << ID_ATTR << "=\"" << obj_metadata.first << "\" " << INSTANCESCOUNT_ATTR << "=\"" << obj->instances.size() << "\">\n";

                // stores object's name
                if (!obj->name.empty())
                    stream << "  <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << OBJECT_TYPE << "\" " << KEY_ATTR << "=\"name\" " << VALUE_ATTR << "=\"" << xml_escape(obj->name) << "\"/>\n";

                // stores object's config data
                for (const std::string& key : obj->config.keys()) {
                    stream << "  <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << OBJECT_TYPE << "\" " << KEY_ATTR << "=\"" << key << "\" " << VALUE_ATTR << "=\"" << obj->config.opt_serialize(key) << "\"/>\n";
                }

                for (const ModelVolume* volume : obj_metadata.second.object->volumes) {
                    if (volume != nullptr) {
                        const VolumeToOffsetsMap& offsets = obj_metadata.second.volumes_offsets;
                        VolumeToOffsetsMap::const_iterator it = offsets.find(volume);
                        if (it != offsets.end()) {
                            // stores volume's offsets
                            stream << "  <" << VOLUME_TAG << " ";
                            stream << FIRST_TRIANGLE_ID_ATTR << "=\"" << it->second.first_triangle_id << "\" ";
                            stream << LAST_TRIANGLE_ID_ATTR << "=\"" << it->second.last_triangle_id << "\">\n";

                            // stores volume's name
                            if (!volume->name.empty())
                                stream << "   <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << VOLUME_TYPE << "\" " << KEY_ATTR << "=\"" << NAME_KEY << "\" " << VALUE_ATTR << "=\"" << xml_escape(volume->name) << "\"/>\n";

                            // stores volume's modifier field (legacy, to support old slicers)
                            if (volume->is_modifier())
                                stream << "   <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << VOLUME_TYPE << "\" " << KEY_ATTR << "=\"" << MODIFIER_KEY << "\" " << VALUE_ATTR << "=\"1\"/>\n";
                            // stores volume's type (overrides the modifier field above)
                            stream << "   <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << VOLUME_TYPE << "\" " << KEY_ATTR << "=\"" << VOLUME_TYPE_KEY << "\" " <<
                                VALUE_ATTR << "=\"" << ModelVolume::type_to_string(volume->type()) << "\"/>\n";

                            // stores volume's local matrix
                            stream << "   <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << VOLUME_TYPE << "\" " << KEY_ATTR << "=\"" << MATRIX_KEY << "\" " << VALUE_ATTR << "=\"";
                            Transform3d matrix = volume->get_matrix() * volume->source.transform.get_matrix();
                            for (int r = 0; r < 4; ++r) {
                                for (int c = 0; c < 4; ++c) {
                                    stream << matrix(r, c);
                                    if (r != 3 || c != 3)
                                        stream << " ";
                                }
                            }
                            stream << "\"/>\n";

                            // stores volume's source data
                            {
                                std::string input_file = xml_escape(m_fullpath_sources ? volume->source.input_file : boost::filesystem::path(volume->source.input_file).filename().string());
                                std::string prefix = std::string("   <") + METADATA_TAG + " " + TYPE_ATTR + "=\"" + VOLUME_TYPE + "\" " + KEY_ATTR + "=\"";
                                if (! volume->source.input_file.empty()) {
                                    stream << prefix << SOURCE_FILE_KEY      << "\" " << VALUE_ATTR << "=\"" << input_file << "\"/>\n";
                                    stream << prefix << SOURCE_OBJECT_ID_KEY << "\" " << VALUE_ATTR << "=\"" << volume->source.object_idx << "\"/>\n";
                                    stream << prefix << SOURCE_VOLUME_ID_KEY << "\" " << VALUE_ATTR << "=\"" << volume->source.volume_idx << "\"/>\n";
                                    stream << prefix << SOURCE_OFFSET_X_KEY  << "\" " << VALUE_ATTR << "=\"" << volume->source.mesh_offset(0) << "\"/>\n";
                                    stream << prefix << SOURCE_OFFSET_Y_KEY  << "\" " << VALUE_ATTR << "=\"" << volume->source.mesh_offset(1) << "\"/>\n";
                                    stream << prefix << SOURCE_OFFSET_Z_KEY  << "\" " << VALUE_ATTR << "=\"" << volume->source.mesh_offset(2) << "\"/>\n";
                                }
                                assert(! volume->source.is_converted_from_inches || ! volume->source.is_converted_from_meters);
                                if (volume->source.is_converted_from_inches)
                                    stream << prefix << SOURCE_IN_INCHES << "\" " << VALUE_ATTR << "=\"1\"/>\n";
                                else if (volume->source.is_converted_from_meters)
                                    stream << prefix << SOURCE_IN_METERS << "\" " << VALUE_ATTR << "=\"1\"/>\n";
                            }

                            // stores volume's config data
                            for (const std::string& key : volume->config.keys()) {
                                stream << "   <" << METADATA_TAG << " " << TYPE_ATTR << "=\"" << VOLUME_TYPE << "\" " << KEY_ATTR << "=\"" << key << "\" " << VALUE_ATTR << "=\"" << volume->config.opt_serialize(key) << "\"/>\n";
                            }

                            // stores mesh's statistics
                            const RepairedMeshErrors& stats = volume->mesh().stats().repaired_errors;
                            stream << "   <" << MESH_TAG << " ";
                            stream << MESH_STAT_EDGES_FIXED        << "=\"" << stats.edges_fixed        << "\" ";
                            stream << MESH_STAT_DEGENERATED_FACETS << "=\"" << stats.degenerate_facets  << "\" ";
                            stream << MESH_STAT_FACETS_REMOVED     << "=\"" << stats.facets_removed     << "\" ";
                            stream << MESH_STAT_FACETS_RESERVED    << "=\"" << stats.facets_reversed    << "\" ";
                            stream << MESH_STAT_BACKWARDS_EDGES    << "=\"" << stats.backwards_edges    << "\"/>\n";

                            stream << "  </" << VOLUME_TAG << ">\n";
                        }
                    }
                }

                stream << " </" << OBJECT_TAG << ">\n";
            }
        }

        stream << "</" << CONFIG_TAG << ">\n";

        std::string out = stream.str();

        if (!mz_zip_writer_add_mem(&archive, MODEL_CONFIG_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
            add_error("Unable to add model config file to archive");
            return false;
        }

        return true;
    }

bool _3MF_Exporter::_add_custom_gcode_per_print_z_file_to_archive( mz_zip_archive& archive, Model& model, const DynamicPrintConfig* config)
{
    return true;
    //std::string out = "";

    //if (!model.custom_gcode_per_print_z.gcodes.empty()) {
    //    pt::ptree tree;
    //    pt::ptree& main_tree = tree.add("custom_gcodes_per_print_z", "");

    //    for (const CustomGCode::Item& code : model.custom_gcode_per_print_z.gcodes) {
    //        pt::ptree& code_tree = main_tree.add("code", "");

    //        // store data of custom_gcode_per_print_z
    //        code_tree.put("<xmlattr>.print_z"   , code.print_z  );
    //        code_tree.put("<xmlattr>.type"      , static_cast<int>(code.type));
    //        code_tree.put("<xmlattr>.extruder"  , code.extruder );
    //        code_tree.put("<xmlattr>.color"     , code.color    );
    //        code_tree.put("<xmlattr>.extra"     , code.extra    );

    //        //BBS
    //        std::string gcode = //code.type == CustomGCode::ColorChange ? config->opt_string("color_change_gcode")    :
    //                            code.type == CustomGCode::PausePrint  ? config->opt_string("machine_pause_gcode")     :
    //                            code.type == CustomGCode::Template    ? config->opt_string("template_custom_gcode")   :
    //                            code.type == CustomGCode::ToolChange  ? "tool_change"   : code.extra;
    //        code_tree.put("<xmlattr>.gcode"     , gcode   );
    //    }

    //    pt::ptree& mode_tree = main_tree.add("mode", "");
    //    // store mode of a custom_gcode_per_print_z
    //    mode_tree.put("<xmlattr>.value", model.custom_gcode_per_print_z.mode == CustomGCode::Mode::SingleExtruder ? CustomGCode::SingleExtruderMode :
    //                                     model.custom_gcode_per_print_z.mode == CustomGCode::Mode::MultiAsSingle ?  CustomGCode::MultiAsSingleMode :
    //                                     CustomGCode::MultiExtruderMode);

    //    if (!tree.empty()) {
    //        std::ostringstream oss;
    //        boost::property_tree::write_xml(oss, tree);
    //        out = oss.str();

    //        // Post processing("beautification") of the output string
    //        boost::replace_all(out, "><", ">\n<");
    //    }
    //}

    //if (!out.empty()) {
    //    if (!mz_zip_writer_add_mem(&archive, CUSTOM_GCODE_PER_PRINT_Z_FILE.c_str(), (const void*)out.data(), out.length(), MZ_DEFAULT_COMPRESSION)) {
    //        add_error("Unable to add custom Gcodes per print_z file to archive");
    //        return false;
    //    }
    //}

    //return true;
}

// 根据可用的配置值执行转换。
//FIXME 提供一个存储项目文件(3MF)的PrusaSlicer版本。
static void handle_legacy_project_loaded(unsigned int version_project_file, DynamicPrintConfig& config)
{
    if (! config.has("brim_object_gap")) {
        if (auto *opt_elephant_foot   = config.option<ConfigOptionFloat>("elefant_foot_compensation", false); opt_elephant_foot) {
            // 从旧版PrusaSlicer转换，该版本应用的brim分离等于象脚补偿。
            auto *opt_brim_separation = config.option<ConfigOptionFloat>("brim_object_gap", true);
            opt_brim_separation->value = opt_elephant_foot->value;
        }
    }
}

bool load_3mf(const char* path, DynamicPrintConfig& config, ConfigSubstitutionContext& config_substitutions, Model* model, bool check_version)
{
    if (path == nullptr || model == nullptr)
        return false;

    // 所有导入应使用"C"区域设置进行数字格式化。
    CNumericLocalesSetter locales_setter;
    _3MF_Importer         importer;
    bool res = importer.load_model_from_file(path, *model, config, config_substitutions, check_version);
    importer.log_errors();
    handle_legacy_project_loaded(importer.version(), config);
    return res;
}

bool store_3mf(const char* path, Model* model, const DynamicPrintConfig* config, bool fullpath_sources, const ThumbnailData* thumbnail_data, bool zip64)
{
    // 所有导出应使用"C"区域设置进行数字格式化。
    CNumericLocalesSetter locales_setter;

    if (path == nullptr || model == nullptr)
        return false;

    _3MF_Exporter exporter;
    bool res = exporter.save_model_to_file(path, *model, config, fullpath_sources, thumbnail_data, zip64);
    if (!res)
        exporter.log_errors();

    return res;
}
} // namespace Slic3r
