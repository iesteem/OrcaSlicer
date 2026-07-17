
#include "CutUtils.hpp"
#include "Geometry.hpp"
#include "libslic3r.h"
#include "Model.hpp"
#include "TriangleMeshSlicer.hpp"
#include "TriangleSelector.hpp"
#include "ObjectID.hpp"

#include <boost/log/trivial.hpp>

namespace Slic3r {

using namespace Geometry;

static void apply_tolerance(ModelVolume* vol)
{
    ModelVolume::CutInfo& cut_info = vol->cut_info;

    assert(cut_info.is_connector);
    if (!cut_info.is_processed)
        return;

    Vec3d sf = vol->get_scaling_factor();

    // 使"孔"变宽
    sf[X] += double(cut_info.radius_tolerance);
    sf[Y] += double(cut_info.radius_tolerance);

    // 使"孔"更深
    sf[Z] += double(cut_info.height_tolerance);

    vol->set_scaling_factor(sf);

    // 根据新深度修正偏移
    Vec3d rot_norm = rotation_transform(vol->get_rotation()) * Vec3d::UnitZ();
    if (rot_norm.norm() != 0.0)
        rot_norm.normalize();

    double z_offset = 0.5 * static_cast<double>(cut_info.height_tolerance);
    if (cut_info.connector_type == CutConnectorType::Plug || 
        cut_info.connector_type == CutConnectorType::Snap)
        z_offset -= 0.05; // 添加小Z偏移以更好地预览

    vol->set_offset(vol->get_offset() + rot_norm * z_offset);
}

static void add_cut_volume(TriangleMesh& mesh, ModelObject* object, const ModelVolume* src_volume, const Transform3d& cut_matrix, const std::string& suffix = {}, ModelVolumeType type = ModelVolumeType::MODEL_PART)
{
    if (mesh.empty())
        return;

    mesh.transform(cut_matrix);
    ModelVolume* vol = object->add_volume(mesh);
    vol->set_type(type);

    vol->name = src_volume->name + suffix;
    // 不复制配置的ID。
    vol->config.assign_config(src_volume->config);
    assert(vol->config.id().valid());
    assert(vol->config.id() != src_volume->config.id());
    vol->set_material(src_volume->material_id(), *src_volume->material());
    vol->cut_info = src_volume->cut_info;
}

static void process_volume_cut( ModelVolume* volume, const Transform3d& instance_matrix, const Transform3d& cut_matrix,
                                ModelObjectCutAttributes attributes, TriangleMesh& upper_mesh, TriangleMesh& lower_mesh)
{
    const auto volume_matrix = volume->get_matrix();

    const Transformation cut_transformation = Transformation(cut_matrix);
    const Transform3d invert_cut_matrix = cut_transformation.get_rotation_matrix().inverse() * translation_transform(-1 * cut_transformation.get_offset());

    // 通过组合变换矩阵变换网格。
    // 如果复合变换是左手系，则翻转三角形。
    TriangleMesh mesh(volume->mesh());
    mesh.transform(invert_cut_matrix * instance_matrix * volume_matrix, true);

    indexed_triangle_set upper_its, lower_its;
    cut_mesh(mesh.its, 0.0f, &upper_its, &lower_its);
    if (attributes.has(ModelObjectCutAttribute::KeepUpper))
        upper_mesh = TriangleMesh(upper_its);
    if (attributes.has(ModelObjectCutAttribute::KeepLower))
        lower_mesh = TriangleMesh(lower_its);
}

static void process_connector_cut(  ModelVolume* volume, const Transform3d& instance_matrix, const Transform3d& cut_matrix,
                                    ModelObjectCutAttributes attributes, ModelObject* upper, ModelObject* lower,
                                    std::vector<ModelObject*>& dowels)
{
    assert(volume->cut_info.is_connector);
    volume->cut_info.set_processed();

    const auto volume_matrix = volume->get_matrix();

    // ! 不要对连接器应用实例变换。
    // 这个变换已经存在了
    if (volume->cut_info.connector_type != CutConnectorType::Dowel) {
        if (attributes.has(ModelObjectCutAttribute::KeepUpper)) {
            ModelVolume* vol = nullptr;
            if (volume->cut_info.connector_type == CutConnectorType::Snap) {
                TriangleMesh mesh = TriangleMesh(its_make_cylinder(1.0, 1.0, PI / 180.));

                vol = upper->add_volume(std::move(mesh));
                vol->set_transformation(volume->get_transformation());
                vol->set_type(ModelVolumeType::NEGATIVE_VOLUME);

                vol->cut_info = volume->cut_info;
                vol->name = volume->name;
            }
            else
                vol = upper->add_volume(*volume);

            vol->set_transformation(volume_matrix);
            apply_tolerance(vol);
        }
        if (attributes.has(ModelObjectCutAttribute::KeepLower)) {
            ModelVolume* vol = lower->add_volume(*volume);
            vol->set_transformation(volume_matrix);
            // 对于下部，如果此连接器是插头，将类型从NEGATIVE_VOLUME更改为MODEL_PART
            vol->set_type(ModelVolumeType::MODEL_PART);
        }
    }
    else {
        if (attributes.has(ModelObjectCutAttribute::CreateDowels)) {
            ModelObject* dowel{ nullptr };
            // 克隆对象以复制实例、材料等。
            volume->get_object()->clone_for_cut(&dowel);

            // 如果此连接器是销钉，则添加一个与连接器相同的实体部件
            ModelVolume* vol = dowel->add_volume(*volume);
            vol->set_type(ModelVolumeType::MODEL_PART);

            // 但丢弃此体积块的旋转和Z偏移
            vol->set_rotation(Vec3d::Zero());
            vol->set_offset(Z, 0.0);

            dowels.push_back(dowel);
        }

        // 切割销钉
        apply_tolerance(volume);

        // 执行切割
        TriangleMesh upper_mesh, lower_mesh;
        process_volume_cut(volume, Transform3d::Identity(), cut_matrix, attributes, upper_mesh, lower_mesh);

        // 添加小Z偏移以更好地预览
        upper_mesh.translate((-0.05 * Vec3d::UnitZ()).cast<float>());
        lower_mesh.translate((0.05 * Vec3d::UnitZ()).cast<float>());

        // 添加切割部件到相关对象
        add_cut_volume(upper_mesh, upper, volume, cut_matrix, "_A", volume->type());
        add_cut_volume(lower_mesh, lower, volume, cut_matrix, "_B", volume->type());
    }
}

static void process_modifier_cut(ModelVolume* volume, const Transform3d& instance_matrix, const Transform3d& inverse_cut_matrix,
                                 ModelObjectCutAttributes attributes, ModelObject* upper, ModelObject* lower)
{
    const auto volume_matrix = instance_matrix * volume->get_matrix();

    // 修饰器不被切割，但我们仍需要将实例变换添加到修饰器体积变换中以正确保留其形状。
    volume->set_transformation(Transformation(volume_matrix));

    if (attributes.has(ModelObjectCutAttribute::KeepAsParts)) {
        upper->add_volume(*volume);
        return;
    }

    // 负体积块/连接器的一些逻辑。仅添加所需的修饰器
    auto bb = volume->mesh().transformed_bounding_box(inverse_cut_matrix * volume_matrix);
    bool is_crossed_by_cut = bb.min[Z] <= 0 && bb.max[Z] >= 0;
    if (attributes.has(ModelObjectCutAttribute::KeepUpper) && (bb.min[Z] >= 0 || is_crossed_by_cut))
        upper->add_volume(*volume);
    if (attributes.has(ModelObjectCutAttribute::KeepLower) && (bb.max[Z] <= 0 || is_crossed_by_cut))
        lower->add_volume(*volume);
}

static void process_solid_part_cut(ModelVolume* volume, const Transform3d& instance_matrix, const Transform3d& cut_matrix,
                            ModelObjectCutAttributes attributes, ModelObject* upper, ModelObject* lower)
{
    // 执行切割
    TriangleMesh upper_mesh, lower_mesh;
    process_volume_cut(volume, instance_matrix, cut_matrix, attributes, upper_mesh, lower_mesh);

    // 添加所需的切割部件到对象

    if (attributes.has(ModelObjectCutAttribute::KeepAsParts)) {
        add_cut_volume(upper_mesh, upper, volume, cut_matrix, "_A");
        if (!lower_mesh.empty()) {
            add_cut_volume(lower_mesh, upper, volume, cut_matrix, "_B");
            upper->volumes.back()->cut_info.is_from_upper = false;
        }
        return;
    }

    if (attributes.has(ModelObjectCutAttribute::KeepUpper))
        add_cut_volume(upper_mesh, upper, volume, cut_matrix);

    if (attributes.has(ModelObjectCutAttribute::KeepLower) && !lower_mesh.empty())
        add_cut_volume(lower_mesh, lower, volume, cut_matrix);
}

static void reset_instance_transformation(ModelObject* object, size_t src_instance_idx, 
                                          const Transform3d& cut_matrix = Transform3d::Identity(),
                                          bool place_on_cut = false, bool flip = false)
{
    // 重置实例变换，除了偏移和Z旋转

    for (size_t i = 0; i < object->instances.size(); ++i) {
        auto& obj_instance = object->instances[i];
        const double rot_z = obj_instance->get_rotation().z();
        
        Transformation inst_trafo = Transformation(obj_instance->get_transformation().get_matrix_no_scaling_factor());
        // 添加对镜像的考虑
        if (obj_instance->is_left_handed())
            inst_trafo = inst_trafo * Transformation(scale_transform(Vec3d(-1, 1, 1)));

        obj_instance->set_transformation(inst_trafo);

        Vec3d rotation = Vec3d::Zero();
        if (!flip && !place_on_cut) {
            if ( i != src_instance_idx)
            rotation[Z] = rot_z;
        }
        else {
            Transform3d rotation_matrix = Transform3d::Identity();
            if (flip)
                rotation_matrix = rotation_transform(PI * Vec3d::UnitX());

            if (place_on_cut)
                rotation_matrix = rotation_matrix * Transformation(cut_matrix).get_rotation_matrix().inverse();

            if (i != src_instance_idx)
                rotation_matrix = rotation_transform(rot_z * Vec3d::UnitZ()) * rotation_matrix;

            rotation = Transformation(rotation_matrix).get_rotation();
        }

        obj_instance->set_rotation(rotation);
    }
}


Cut::Cut(const ModelObject* object, int instance, const Transform3d& cut_matrix,
         ModelObjectCutAttributes attributes/*= ModelObjectCutAttribute::KeepUpper | ModelObjectCutAttribute::KeepLower | ModelObjectCutAttribute::KeepAsParts*/)
    : m_instance(instance), m_cut_matrix(cut_matrix), m_attributes(attributes)
{
    m_model = Model();
    if (object)
        m_model.add_object(*object);
}

void Cut::post_process(ModelObject* object, ModelObjectPtrs& cut_object_ptrs, bool keep, bool place_on_cut, bool flip)
{
    if (!object) return;

    if (keep && !object->volumes.empty()) {
        reset_instance_transformation(object, m_instance, m_cut_matrix, place_on_cut, flip);
        cut_object_ptrs.push_back(object);
    }
    else
        m_model.objects.push_back(object); // 将在 m_model.clear_objects() 中被删除
}

void Cut::post_process(ModelObject* upper, ModelObject* lower, ModelObjectPtrs& cut_object_ptrs)
{
    post_process(upper, cut_object_ptrs,
        m_attributes.has(ModelObjectCutAttribute::KeepUpper),
        m_attributes.has(ModelObjectCutAttribute::PlaceOnCutUpper),
        m_attributes.has(ModelObjectCutAttribute::FlipUpper));

    post_process(lower, cut_object_ptrs,
        m_attributes.has(ModelObjectCutAttribute::KeepLower),
        m_attributes.has(ModelObjectCutAttribute::PlaceOnCutLower),
        m_attributes.has(ModelObjectCutAttribute::PlaceOnCutLower) || m_attributes.has(ModelObjectCutAttribute::FlipLower));
}


void Cut::finalize(const ModelObjectPtrs& objects)
{
    // 从模型中清除临时对象
    m_model.clear_objects();

    // 添加到模型结果对象
    m_model.objects = objects;
}


const ModelObjectPtrs& Cut::perform_with_plane()
{
    if (!m_attributes.has(ModelObjectCutAttribute::KeepUpper) && !m_attributes.has(ModelObjectCutAttribute::KeepLower)) {
        m_model.clear_objects();
        return m_model.objects;
    }

    ModelObject* mo = m_model.objects.front();

    BOOST_LOG_TRIVIAL(trace) << "ModelObject::cut - start";

    // 克隆对象以复制实例、材料等。
    ModelObject* upper{ nullptr };
    if (m_attributes.has(ModelObjectCutAttribute::KeepUpper))
        mo->clone_for_cut(&upper);

    ModelObject* lower{ nullptr };
    if (m_attributes.has(ModelObjectCutAttribute::KeepLower) && !m_attributes.has(ModelObjectCutAttribute::KeepAsParts))
        mo->clone_for_cut(&lower);

    std::vector<ModelObject*> dowels;

    // 因为变换将直接应用于网格，
    // 我们重置所有实例和体积块的变换，
    // 除了实例上的平移和Z旋转，它们保留在变换矩阵中且不应用于网格变换。

    const auto              instance_matrix = mo->instances[m_instance]->get_transformation().get_matrix_no_offset();
    const Transformation    cut_transformation = Transformation(m_cut_matrix);
    const Transform3d       inverse_cut_matrix = cut_transformation.get_rotation_matrix().inverse() * translation_transform(-1. * cut_transformation.get_offset());

    for (ModelVolume* volume : mo->volumes) {
        volume->reset_extra_facets();

        if (!volume->is_model_part()) {
            if (volume->cut_info.is_processed)
                process_modifier_cut(volume, instance_matrix, inverse_cut_matrix, m_attributes, upper, lower);
            else
                process_connector_cut(volume, instance_matrix, m_cut_matrix, m_attributes, upper, lower, dowels);
        }
        else if (!volume->mesh().empty())
            process_solid_part_cut(volume, instance_matrix, m_cut_matrix, m_attributes, upper, lower);
    }

    // 后处理切割部件

    if (m_attributes.has(ModelObjectCutAttribute::KeepAsParts) && upper->volumes.empty()) {
        m_model = Model();
        m_model.objects.push_back(upper);
        return m_model.objects;
    }

    ModelObjectPtrs cut_object_ptrs;

    if (m_attributes.has(ModelObjectCutAttribute::KeepAsParts) && !upper->volumes.empty()) {
        reset_instance_transformation(upper, m_instance, m_cut_matrix);
        cut_object_ptrs.push_back(upper);
    }
    else {
        // 删除所有不与实体部件边界框相交的修饰器
        auto delete_extra_modifiers = [this](ModelObject* mo) {
            if (!mo) return;
            const BoundingBoxf3 obj_bb = mo->instance_bounding_box(m_instance);
            const Transform3d inst_matrix = mo->instances[m_instance]->get_transformation().get_matrix();

            for (int i = int(mo->volumes.size()) - 1; i >= 0; --i)
                if (const ModelVolume* vol = mo->volumes[i];
                    !vol->is_model_part() && !vol->is_cut_connector()) {
                    auto bb = vol->mesh().transformed_bounding_box(inst_matrix * vol->get_matrix());
                    if (!obj_bb.intersects(bb))
                        mo->delete_volume(i);
                }
        };

        post_process(upper, lower, cut_object_ptrs);
        delete_extra_modifiers(upper);
        delete_extra_modifiers(lower);

        if (m_attributes.has(ModelObjectCutAttribute::CreateDowels) && !dowels.empty()) {
            for (auto dowel : dowels) {
                reset_instance_transformation(dowel, m_instance);
                dowel->name += "-Dowel-" + dowel->volumes[0]->name;
                cut_object_ptrs.push_back(dowel);
            }
        }
    }

    BOOST_LOG_TRIVIAL(trace) << "ModelObject::cut - end";

    finalize(cut_object_ptrs);

    return m_model.objects;
}

static void distribute_modifiers_from_object(ModelObject* from_obj, const int instance_idx, ModelObject* to_obj1, ModelObject* to_obj2)
{
    auto              obj1_bb = to_obj1 ? to_obj1->instance_bounding_box(instance_idx) : BoundingBoxf3();
    auto              obj2_bb = to_obj2 ? to_obj2->instance_bounding_box(instance_idx) : BoundingBoxf3();
    const Transform3d inst_matrix = from_obj->instances[instance_idx]->get_transformation().get_matrix();

    for (ModelVolume* vol : from_obj->volumes)
        if (!vol->is_model_part()) {
            // 不添加已处理的连接器修饰器
            if (vol->cut_info.is_connector && !vol->cut_info.is_processed)
                continue;
            auto bb = vol->mesh().transformed_bounding_box(inst_matrix * vol->get_matrix());
            // 不添加不与实体部件相交的修饰器
            if (obj1_bb.intersects(bb))
                to_obj1->add_volume(*vol);
            if (obj2_bb.intersects(bb))
                to_obj2->add_volume(*vol);
        }
}

static void merge_solid_parts_inside_object(ModelObjectPtrs& objects)
{
    for (ModelObject* mo : objects) {
        TriangleMesh mesh;
        // 合并所有实体部件但不包括连接器
        for (const ModelVolume* mv : mo->volumes) {
            if (mv->is_model_part() && !mv->is_cut_connector()) {
                TriangleMesh m = mv->mesh();
                m.transform(mv->get_matrix());
                mesh.merge(m);
            }
        }
        if (!mesh.empty()) {
            ModelVolume* new_volume = mo->add_volume(mesh);
            new_volume->name = mo->name;
            // 删除所有已合并的实体部件但不包括连接器
            for (int i = int(mo->volumes.size()) - 2; i >= 0; --i) {
                const ModelVolume* mv = mo->volumes[i];
                if (mv->is_model_part() && !mv->is_cut_connector())
                    mo->delete_volume(i);
            }
            // 确保体积块以实体部件开头以便正确切片
            mo->sort_volumes(true);
        }
    }
}


const ModelObjectPtrs& Cut::perform_by_contour(std::vector<Part> parts, int dowels_count)
{
    ModelObject* cut_mo = m_model.objects.front();

    // 克隆对象以复制实例、材料等。
    ModelObject* upper{ nullptr };
    if (m_attributes.has(ModelObjectCutAttribute::KeepUpper)) cut_mo->clone_for_cut(&upper);
    ModelObject* lower{ nullptr };
    if (m_attributes.has(ModelObjectCutAttribute::KeepLower)) cut_mo->clone_for_cut(&lower);

    if (upper && lower) {
        upper->name = upper->name + "_A";
        lower->name = lower->name + "_B";
    }

    const size_t cut_parts_cnt = parts.size();
    bool has_modifiers = false;

    // 将实体部件分配到上/下对象
    for (size_t id = 0; id < cut_parts_cnt; ++id) {
        if (parts[id].is_modifier)
            has_modifiers = true; // 修饰器将在稍后添加到相关部件中
        else if (ModelObject* obj = (parts[id].selected ? upper : lower))
            obj->add_volume(*(cut_mo->volumes[id]));
    }

    if (has_modifiers) {
        // 将修饰器分配到上/下对象
        distribute_modifiers_from_object(cut_mo, m_instance, upper, lower);
    }

    ModelObjectPtrs cut_object_ptrs;

    ModelVolumePtrs& volumes = cut_mo->volumes;
    if (volumes.size() == cut_parts_cnt) {
        // 表示对象不带连接器切割

        // 只需将上部和下部对象添加到 cut_object_ptrs
        post_process(upper, lower, cut_object_ptrs);

        // 现在将所有模型部件合并在一起：
        merge_solid_parts_inside_object(cut_object_ptrs);

        // 用切割对象替换模型中的初始对象
        finalize(cut_object_ptrs);
    }
    else if (volumes.size() > cut_parts_cnt) {
        // 表示对象带连接器切割

        // 所有体积块已分配到上/下对象，
        // 所以我们不再需要它们
        for (size_t id = 0; id < cut_parts_cnt; id++)
            delete* (volumes.begin() + id);
        volumes.erase(volumes.begin(), volumes.begin() + cut_parts_cnt);

        // 执行切割仅为了获取连接器
        Cut cut(cut_mo, m_instance, m_cut_matrix, m_attributes);
        const ModelObjectPtrs& cut_connectors_obj = cut.perform_with_plane();
        assert(dowels_count > 0 ? cut_connectors_obj.size() >= 3 : cut_connectors_obj.size() == 2);

        // 来自上部对象的连接器
        for (const ModelVolume* volume : cut_connectors_obj[0]->volumes)
            upper->add_volume(*volume, volume->type());

        // 来自下部对象的连接器
        for (const ModelVolume* volume : cut_connectors_obj[1]->volumes)
            lower->add_volume(*volume, volume->type());

        // 添加上部和下部对象到 cut_object_ptrs
        post_process(upper, lower, cut_object_ptrs);

        // 现在将所有模型部件合并在一起：
        merge_solid_parts_inside_object(cut_object_ptrs);

        // 用切割对象替换模型中的初始对象
        finalize(cut_object_ptrs);

        // 将销钉连接器作为单独对象添加到模型
        if (cut_connectors_obj.size() >= 3)
            for (size_t id = 2; id < cut_connectors_obj.size(); id++)
                m_model.add_object(*cut_connectors_obj[id]);
    }

    return m_model.objects;
}


const ModelObjectPtrs& Cut::perform_with_groove(const Groove& groove, const Transform3d& rotation_m, bool keep_as_parts/* = false*/)
{
    ModelObject* cut_mo = m_model.objects.front();

    // 克隆对象以复制实例、材料等。
    ModelObject* upper{ nullptr };
    cut_mo->clone_for_cut(&upper);
    ModelObject* lower{ nullptr };
    cut_mo->clone_for_cut(&lower);

    if (upper && lower) {
        upper->name = upper->name + "_A";
        lower->name = lower->name + "_B";
    }
    const double groove_half_depth = 0.5 * double(groove.depth);

    Model tmp_model_for_cut = Model();

    Model tmp_model = Model();
    tmp_model.add_object(*cut_mo);
    ModelObject* tmp_object = tmp_model.objects.front();

    auto add_volumes_from_cut = [](ModelObject* object, const ModelObjectCutAttribute attribute, const Model& tmp_model_for_cut) {
        const auto& volumes = tmp_model_for_cut.objects.front()->volumes;
        for (const ModelVolume* volume : volumes)
            if (volume->is_model_part()) {
                if ((attribute == ModelObjectCutAttribute::KeepUpper && volume->is_from_upper()) ||
                    (attribute != ModelObjectCutAttribute::KeepUpper && !volume->is_from_upper())) {
                    ModelVolume* new_vol = object->add_volume(*volume);
                    new_vol->reset_from_upper();
                }
            }
    };

    auto cut = [this, add_volumes_from_cut]
                (ModelObject* object, const Transform3d& cut_matrix, const ModelObjectCutAttribute add_volumes_attribute, Model& tmp_model_for_cut) {
        Cut cut(object, m_instance, cut_matrix);

        tmp_model_for_cut = Model();
        tmp_model_for_cut.add_object(*cut.perform_with_plane().front());
        assert(!tmp_model_for_cut.objects.empty());

        object->clear_volumes();
        add_volumes_from_cut(object, add_volumes_attribute, tmp_model_for_cut);
        reset_instance_transformation(object, m_instance);
    };

    // 按上平面切割

    const Transform3d cut_matrix_upper = translation_transform(rotation_m * (groove_half_depth * Vec3d::UnitZ())) * m_cut_matrix;
    {
        cut(tmp_object, cut_matrix_upper, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);
        add_volumes_from_cut(upper, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);
    }

    // 按下平面切割

    const Transform3d cut_matrix_lower = translation_transform(rotation_m * (-groove_half_depth * Vec3d::UnitZ())) * m_cut_matrix;
    {
        cut(tmp_object, cut_matrix_lower, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);
        add_volumes_from_cut(lower, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);
    }

    // 用两个角度切割中间部分并将部件添加到相关的上/下对象

    const double h_side_shift = 0.5 * double(groove.width + groove.depth / tan(groove.flaps_angle));

    // 按角度1平面切割
    {
        const Transform3d cut_matrix_angle1 = translation_transform(rotation_m * (-h_side_shift * Vec3d::UnitX())) * m_cut_matrix * rotation_transform(Vec3d(0, -groove.flaps_angle, -groove.angle));

        cut(tmp_object, cut_matrix_angle1, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);
        add_volumes_from_cut(lower, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);
    }

    // 按角度2平面切割
    {
        const Transform3d cut_matrix_angle2 = translation_transform(rotation_m * (h_side_shift * Vec3d::UnitX())) * m_cut_matrix * rotation_transform(Vec3d(0, groove.flaps_angle, groove.angle));

        cut(tmp_object, cut_matrix_angle2, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);
        add_volumes_from_cut(lower, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);
    }

    // 对中间部分应用容差
    {
        const double h_groove_shift_tolerance = groove_half_depth - (double)groove.depth_tolerance;

        const Transform3d cut_matrix_lower_tolerance = translation_transform(rotation_m * (-h_groove_shift_tolerance * Vec3d::UnitZ())) * m_cut_matrix;
        cut(tmp_object, cut_matrix_lower_tolerance, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);

        const double h_side_shift_tolerance = h_side_shift - 0.5 * double(groove.width_tolerance);

        const Transform3d cut_matrix_angle1_tolerance = translation_transform(rotation_m * (-h_side_shift_tolerance * Vec3d::UnitX())) * m_cut_matrix * rotation_transform(Vec3d(0, -groove.flaps_angle, -groove.angle));
        cut(tmp_object, cut_matrix_angle1_tolerance, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);

        const Transform3d cut_matrix_angle2_tolerance = translation_transform(rotation_m * (h_side_shift_tolerance * Vec3d::UnitX())) * m_cut_matrix * rotation_transform(Vec3d(0, groove.flaps_angle, groove.angle));
        cut(tmp_object, cut_matrix_angle2_tolerance, ModelObjectCutAttribute::KeepUpper, tmp_model_for_cut);
    }

    // 此部件现在可以添加到上部对象
    add_volumes_from_cut(upper, ModelObjectCutAttribute::KeepLower, tmp_model_for_cut);

    ModelObjectPtrs cut_object_ptrs;

    if (keep_as_parts) {
        // 从下部对象添加体积块到上部，但标记为下部
        const auto& volumes = lower->volumes;
        for (const ModelVolume* volume : volumes) {
            ModelVolume* new_vol = upper->add_volume(*volume);
            new_vol->cut_info.is_from_upper = false;
        }

        // 添加修饰器
        for (const ModelVolume* volume : cut_mo->volumes)
            if (!volume->is_model_part())
                upper->add_volume(*volume);

        cut_object_ptrs.push_back(upper);

        // 将下部对象添加到 cut_object_ptrs 只是为了正确地从 Model 析构函数中删除它并避免内存泄漏
        cut_object_ptrs.push_back(lower);
    }
    else {
        // 如果对象有修饰器则添加
        for (const ModelVolume* volume : cut_mo->volumes)
            if (!volume->is_model_part()) {
                distribute_modifiers_from_object(cut_mo, m_instance, upper, lower);
                break;
            }

        assert(!upper->volumes.empty() && !lower->volumes.empty());

        // 添加上部和下部部件到 cut_object_ptrs

        post_process(upper, lower, cut_object_ptrs);

        // 现在将所有模型部件合并在一起：
        merge_solid_parts_inside_object(cut_object_ptrs);
    }

    finalize(cut_object_ptrs);

    return m_model.objects;
}

} // namespace Slic3r

