#ifndef libslic3r_SeamPlacer_hpp_
#define libslic3r_SeamPlacer_hpp_

#include <limits>
#include <optional>
#include <vector>
#include <memory>
#include <atomic>

#include "libslic3r/libslic3r.h"
#include "libslic3r/ExtrusionEntity.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/BoundingBox.hpp"
#include "libslic3r/AABBTreeIndirect.hpp"
#include "libslic3r/KDTreeIndirect.hpp"

namespace Slic3r {

class PrintObject;
class ExtrusionLoop;
class Print;
class Layer;

namespace EdgeGrid {
class Grid;
}

namespace SeamPlacerImpl {


struct GlobalModelInfo;
struct SeamComparator;

enum class EnforcedBlockedSeamPoint {
  Blocked = 0,
  Neutral = 1,
  Enforced = 2,
};

// 表示单个周长环的结构
struct Perimeter {
  size_t start_index{};
  size_t end_index{}; //包含！
  size_t seam_index{};
  float flow_width{};

  // 在对齐期间，最终位置可以存储在此处。在这种情况下，finalized设置为true。
  // 注意，最终接缝位置不限于周长环的点。理论上它可以是任何位置。
  // 随机位置也使用此灵活性来设置最终接缝点位置
  bool finalized = false;
  Vec3f final_seam_position = Vec3f::Zero();
};

// 所有周长处理都在此结构上进行。对于每个周长点，创建其对应的候选点，
// 然后计算所有需要的属性，最后为每个周长选择一个点作为接缝。
// 此接缝位置可以进一步对齐
struct SeamCandidate {
  SeamCandidate(const Vec3f &pos, Perimeter &perimeter,
                float local_ccw_angle,
                EnforcedBlockedSeamPoint type) :
                                                 position(pos), perimeter(perimeter), visibility(0.0f), overhang(0.0f), embedded_distance(0.0f), local_ccw_angle(
                                                                                                                                                     local_ccw_angle), type(type), central_enforcer(false) {
  }
  const Vec3f position;
  // 指向此点所在Perimeter环的指针。由环的所有点共享
  Perimeter &perimeter;
  float visibility;
  float overhang;
  float unsupported_dist;
  // 合并层区域内的距离，用于检测隐藏在打印内部的周长点（例如多材料连接）
  // 负号表示在打印内部，来自EdgeGrid结构
  float embedded_distance;
  float local_ccw_angle;
  EnforcedBlockedSeamPoint type;
  bool central_enforcer; //将此候选标记为周长上强制段的中点 - 对对齐很重要
};

struct SeamCandidateCoordinateFunctor {
  SeamCandidateCoordinateFunctor(const std::vector<SeamCandidate> &seam_candidates) :
                                                                                      seam_candidates(seam_candidates) {
  }
  const std::vector<SeamCandidate> &seam_candidates;
  float operator()(size_t index, size_t dim) const {
    return seam_candidates[index].position[dim];
  }
};
} // namespace SeamPlacerImpl

struct PrintObjectSeamData
{
  using SeamCandidatesTree = KDTreeIndirect<3, float, SeamPlacerImpl::SeamCandidateCoordinateFunctor>;

  struct LayerSeams
  {
    Slic3r::deque<SeamPlacerImpl::Perimeter> perimeters;
    std::vector<SeamPlacerImpl::SeamCandidate> points;
    std::unique_ptr<SeamCandidatesTree> points_tree;
  };
  // PrintObjects (PO)的映射 -> PO的层向量 -> 周长向量
  std::vector<LayerSeams> layers;
  // PrintObjects (PO)的映射 -> PO的层向量 -> 唯一指针指向KD树
  // 包含给定层的所有点

  void clear()
  {
    layers.clear();
  }
};

class SeamPlacer {
public:
  // 在网格上生成的样本数。每个样本有sqr_rays_per_sample_point*sqr_rays_per_sample_point条射线投射
  static constexpr size_t raycasting_visibility_samples_count = 30000;
  static constexpr size_t fast_decimation_triangle_count_target = 16000;
  //每个样本点的射线数平方
  static constexpr size_t sqr_rays_per_sample_point = 5;

  // 吸附角度 - 大于此值的角度将在接缝绘制期间被吸附
  static constexpr float sharp_angle_snapping_threshold = 55.0f * float(PI) / 180.0f;
  // 仍能产生良好结果的接缝放置悬垂角度，以度为单位，从垂直方向测量
  static constexpr float overhang_angle_threshold = 45.0f * float(PI) / 180.0f;

  // 确定角度与可见性相比的重要性（中性值为1.0f。）
  static constexpr float angle_importance_aligned = 0.6f;
  static constexpr float angle_importance_nearest = 1.0f; // 对最近模式使用更高的角度重要性，以对抗可见性信息噪声

  // 对于长多边形边，如果它们靠近自定义接缝绘制，则使用此步长进行过采样
  static constexpr float enforcer_oversampling_distance = 0.2f;

  // 在搜索接缝簇进行对齐时：
  // 以下值描述了一个点可以有多差的分值，但仍然可以被选入接缝簇而不是同一层上的原始接缝点
  static constexpr float seam_align_score_tolerance = 0.3f;
  // seam_align_tolerable_dist_factor - 从当前位置搜索接缝的距离，最终距离为seam_align_tolerable_dist_factor * flow_width
  static constexpr float seam_align_tolerable_dist_factor = 4.0f;
  // 使对齐发生所需的簇中最小接缝数
  static constexpr size_t seam_align_minimum_string_seams = 6;
  // 每个段覆盖的毫米数；确定给定字符串的样条数
  static constexpr size_t seam_align_mm_per_segment = 4.0f;

  //以下数据结构包含所有PrintObject的所有周长点。
  std::unordered_map<const PrintObject*, PrintObjectSeamData> m_seam_per_object;

  void init(const Print &print, std::function<void(void)> throw_if_canceled_func);

  void place_seam(const Layer *layer, ExtrusionLoop &loop, const Point &last_pos, float& overhang) const;
private:
  void gather_seam_candidates(const PrintObject *po, const SeamPlacerImpl::GlobalModelInfo &global_model_info);
  void calculate_candidates_visibility(const PrintObject *po,
                                       const SeamPlacerImpl::GlobalModelInfo &global_model_info);
  void calculate_overhangs_and_layer_embedding(const PrintObject *po);
  void align_seam_points(const PrintObject *po, const SeamPlacerImpl::SeamComparator &comparator);
  std::vector<std::pair<size_t, size_t>> find_seam_string(const PrintObject *po,
                                                          std::pair<size_t, size_t> start_seam,
                                                          const SeamPlacerImpl::SeamComparator &comparator) const;
  std::optional<std::pair<size_t, size_t>> find_next_seam_in_layer(
      const std::vector<PrintObjectSeamData::LayerSeams> &layers,
      const Vec3f& projected_position,
      const size_t layer_idx, const float max_distance,
      const SeamPlacerImpl::SeamComparator &comparator) const;
};

} // namespace Slic3r

#endif // libslic3r_SeamPlacer_hpp_