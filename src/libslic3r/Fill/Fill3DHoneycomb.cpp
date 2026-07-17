#include "../ClipperUtils.hpp"
#include "../ShortestPath.hpp"
#include "../Surface.hpp"
#include "FillBase.hpp"
#include "Fill3DHoneycomb.hpp"

namespace Slic3r {

// 符号函数
template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}
  
/*
创建在指定高度上的连续点序列，这些点构成空间填充截断八面体镶嵌的边缘水平切片。
八面体的朝向使得正方形面位于水平平面，边缘平行于 X 和 Y 轴。

致谢：David Eccles (gringer)。
*/

// 三角波函数
// 周期为 (gridSize * 2)，振幅为 (gridSize / 2)，
// 其中 triWave(pos = 0) = 0
static coordf_t triWave(coordf_t pos, coordf_t gridSize)
{
  float t = (pos / (gridSize * 2.)) + 0.25; // convert relative to grid size
  t = t - (int)t; // 提取小数部分
  return((1. - abs(t * 8. - 4.)) * (gridSize / 4.) + (gridSize / 4.));
}

// 截断八面体波形，具有周期和偏移
// 与三角波函数相同。Z 位置调整
// 最大偏移 [介于 -(gridSize / 4) 和 (gridSize / 4) 之间]，具有
// 周期 (gridSize * 2) 且 troctWave(Zpos = 0) = 0
static coordf_t troctWave(coordf_t pos, coordf_t gridSize, coordf_t Zpos)
{
  coordf_t Zcycle = triWave(Zpos, gridSize);
  coordf_t perpOffset = Zcycle / 2;
  coordf_t y = triWave(pos, gridSize);
  return((abs(y) > abs(perpOffset)) ?
	 (sgn(y) * perpOffset) :
	 (y * sgn(perpOffset)));
}

// 识别截断八面体波内曲线变化的关键点（以波形分数 t 表示）：
// 1. 波的起点（始终为 0.0）
// 2. 过渡到上部"水平"部分
// 3. 从上部"水平"部分过渡
// 4. 过渡到下部"水平"部分
// 5. 从下部"水平"部分过渡
/*    o---o
 *   /     \
 * o/       \
 *           \       /
 *            \     /
 *             o---o
 */
static std::vector<coordf_t> getCriticalPoints(coordf_t Zpos, coordf_t gridSize)
{
  std::vector<coordf_t> res = {0.};
  coordf_t perpOffset = abs(triWave(Zpos, gridSize) / 2.);

  coordf_t normalisedOffset = perpOffset / gridSize;
  // // 调试用：仅生成均匀分布的点
  // for(coordf_t i = 0; i < 2; i += 0.05){
  //   res.push_back(gridSize * i);
  // }
  // 注意：0 == 直线
  if(normalisedOffset > 0){
    res.push_back(gridSize * (0. + normalisedOffset));
    res.push_back(gridSize * (1. - normalisedOffset));
    res.push_back(gridSize * (1. + normalisedOffset));
    res.push_back(gridSize * (2. - normalisedOffset));
  }
  return(res);
}

// 生成与基本打印线方向相同的点数组（即列对应 Y 点，行对应 X 点）
// 注意：负偏移仅导致垂直方向的变化
static std::vector<coordf_t> colinearPoints(const coordf_t Zpos, coordf_t gridSize, std::vector<coordf_t> critPoints,
					     const size_t baseLocation, size_t gridLength)
{
  std::vector<coordf_t> points;
  points.push_back(baseLocation);
  for (coordf_t cLoc = baseLocation; cLoc < gridLength; cLoc+= (gridSize*2)) {
    for(size_t pi = 0; pi < critPoints.size(); pi++){
      points.push_back(baseLocation + cLoc + critPoints[pi]);
    }
  }
  points.push_back(gridLength);
  return points;
}

// 生成垂直于基本打印线方向的点数组（即列对应 X 点，行对应 Y 点）
  static std::vector<coordf_t> perpendPoints(const coordf_t Zpos, coordf_t gridSize, std::vector<coordf_t> critPoints,
					     size_t baseLocation, size_t gridLength,
                                             size_t offsetBase, coordf_t perpDir)
{
  std::vector<coordf_t> points;
  points.push_back(offsetBase);
  for (coordf_t cLoc = baseLocation; cLoc < gridLength; cLoc+= gridSize*2) {
    for(size_t pi = 0; pi < critPoints.size(); pi++){
      coordf_t offset = troctWave(critPoints[pi], gridSize, Zpos);
      points.push_back(offsetBase + (offset * perpDir));
    }
  }
  points.push_back(offsetBase);
  return points;
}

static inline Pointfs zip(const std::vector<coordf_t> &x, const std::vector<coordf_t> &y)
{
    assert(x.size() == y.size());
    Pointfs out;
    out.reserve(x.size());
    for (size_t i = 0; i < x.size(); ++ i)
        out.push_back(Vec2d(x[i], y[i]));
    return out;
}

// 生成一组曲线（2D 点数组），描述截断正八面体的水平切片。
static std::vector<Pointfs> makeActualGrid(coordf_t Zpos, coordf_t gridSize, size_t boundsX, size_t boundsY)
{
  std::vector<Pointfs> points;
  std::vector<coordf_t> critPoints = getCriticalPoints(Zpos, gridSize);
  coordf_t zCycle = fmod(Zpos + gridSize/2, gridSize * 2.) / (gridSize * 2.);
  bool printVert = zCycle < 0.5;
  if (printVert) {
    int perpDir = -1;
    for (coordf_t x = 0; x <= (boundsX); x+= gridSize, perpDir *= -1) {
      points.push_back(Pointfs());
      Pointfs &newPoints = points.back();
      newPoints = zip(
		      perpendPoints(Zpos, gridSize, critPoints, 0, boundsY, x, perpDir),
		      colinearPoints(Zpos, gridSize, critPoints, 0, boundsY));
      if (perpDir == 1)
	std::reverse(newPoints.begin(), newPoints.end());
    }
  } else {
    int perpDir = 1;
    for (coordf_t y = gridSize; y <= (boundsY); y+= gridSize, perpDir *= -1) {
      points.push_back(Pointfs());
      Pointfs &newPoints = points.back();
      newPoints = zip(
		      colinearPoints(Zpos, gridSize, critPoints, 0, boundsX),
		      perpendPoints(Zpos, gridSize, critPoints, 0, boundsX, y, perpDir));
      if (perpDir == -1)
	std::reverse(newPoints.begin(), newPoints.end());
    }
  }
  return points;
}

// 生成一组曲线（2D 点数组），描述具有指定网格大小的截断正八面体的水平切片。
// gridWidth 和 gridHeight 分别定义边界框的宽度和高度
static Polylines makeGrid(coordf_t z, coordf_t gridSize, coordf_t boundWidth, coordf_t boundHeight, bool fillEvenly)
{
  std::vector<Pointfs> polylines = makeActualGrid(z, gridSize, boundWidth, boundHeight);
  Polylines result;
  result.reserve(polylines.size());
  for (std::vector<Pointfs>::const_iterator it_polylines = polylines.begin();
       it_polylines != polylines.end(); ++ it_polylines) {
    result.push_back(Polyline());
    Polyline &polyline = result.back();
    for (Pointfs::const_iterator it = it_polylines->begin(); it != it_polylines->end(); ++ it)
      polyline.points.push_back(Point(coord_t((*it)(0)), coord_t((*it)(1))));
  }
  return result;
}

// FillParams 包含以下有用信息：
// density <0 .. 1>  [填充空间的比例]
// anchor_length     [???]
// anchor_length_max [???]
// dont_connect()    [避免连接线]
// dont_adjust       [避免均匀填充空间]
// monotonic         [严格从左到右填充]
// complete          [完成每个循环]

void Fill3DHoneycomb::_fill_surface_single(
    const FillParams                &params,
    unsigned int                     thickness_layers,
    const std::pair<float, Point>   &direction,
    ExPolygon                        expolygon,
    Polylines                       &polylines_out)
{
    // 此填充图案不支持旋转
    // 支持填充角度
    auto infill_angle   = float(this->angle);
    if (std::abs(infill_angle) >= EPSILON) expolygon.rotate(-infill_angle);
    BoundingBox bb = expolygon.contour.bounding_box();

    // 注意：在 X/Y/Z 等比例缩放的情况下，图案将创建一个垂直拉伸的截断八面体；因此 Z 首先通过缩放 sqrt(2) 进行预调整
    coordf_t zScale = sqrt(2);

    // 调整以考虑八边形曲线的额外距离
    // 注意：这仅严格适用于矩形区域，其中总 Z 行程距离是间距的倍数……但它应该至少比之前假设直线的估计要好
    // = 4 * 积分(func=4*x(sqrt(2) - 1) + 1, from=0, to=0.25)
    // = (sqrt(2) + 1) / 2 [... 我认为]
    // 对首选网格大小进行初步估计
    coordf_t gridSize = (scale_(this->spacing) * ((zScale + 1.) / 2.) * params.multiline  / params.density);

    // 此密度计算对于许多 > 25% 的值是不正确的，可能是由于量化误差，
    // 因此该值用作初步估计，然后调整 Z 比例以使层图案一致/对称
    // 这意味着生成的填充不会是理想的截断八面体，
    // 但它应该比等效的量化版本看起来更好

    coordf_t layerHeight = scale_(thickness_layers);
    // 向上取整为每 Z 层的整数值
    // （如果接近完美，则稍微调整）
    coordf_t layersPerModule = floor((gridSize * 2) / (zScale * layerHeight) + 0.05);
    if(params.density > 0.42){ // >42% 密度的精确层模式
      layersPerModule = 2;
      // 为部分八面体路径重新调整网格大小
      // （基于建模估计的 1.1 比例）
      gridSize = (scale_(this->spacing) * 1.1 * params.multiline  / params.density);
      // 重新调整 zScale 以使分层一致
      zScale = (gridSize * 2) / (layersPerModule * layerHeight);
    } else {
      if(layersPerModule < 2){
	layersPerModule = 2;
      }
      // 重新调整 zScale 以使分层一致
      zScale = (gridSize * 2) / (layersPerModule * layerHeight);
      // 重新调整网格大小以考虑新的 zScale
      gridSize = (scale_(this->spacing) * ((zScale + 1.) / 2.) * params.multiline  / params.density);
      // 重新计算 layersPerModule 和 zScale
      layersPerModule = floor((gridSize * 2) / (zScale * layerHeight) + 0.05);
      if(layersPerModule < 2){
	layersPerModule = 2;
      }
      zScale = (gridSize * 2) / (layersPerModule * layerHeight);
    }

    // 将边界框对齐到蜂巢网格模块的倍数
    // （一个模块是 2*$gridSize，因为一个 $gridSize 半模块在增长而另一个 $gridSize 半模块在收缩）
    bb.merge(align_to_grid(bb.min, Point(gridSize*4, gridSize*4)));

    // 生成图案
    Polylines polylines =
      makeGrid(
	       scale_(this->z) * zScale,
	       gridSize,
	       bb.size()(0),
	       bb.size()(1),
	       !params.dont_adjust);

    // 将图案移动到适当位置
    for (Polyline &pl : polylines){
      pl.translate(bb.min);
      pl.simplify(5 * spacing); // 简化为 5 倍线宽
    }

    // Apply multiline offset if needed
    multiline_fill(polylines, params, spacing);

    // 将图案裁剪到边界，链接裁剪后的多段线
    polylines = intersection_pl(polylines, to_polygons(expolygon));

    if (! polylines.empty()) {
    // 移除非常小的片段，但注意不要移除连接薄壁的填充线！
    // 填充周长线应间隔大约一个填充线宽。
    const double minlength = scale_(0.8 * this->spacing);
    polylines.erase(
	std::remove_if(polylines.begin(), polylines.end(), [minlength](const Polyline &pl) { return pl.length() < minlength; }),
	polylines.end());
    }

    // 从 fliplines 复制
    if (!polylines.empty()) {
        int infill_start_idx = polylines_out.size(); // 仅旋转属于我们的部分。
        // 连接线
        chain_or_connect_infill(std::move(polylines), expolygon, polylines_out, this->spacing, params);

        // 旋转回来
        if (std::abs(infill_angle) >= EPSILON) {
          for (auto it = polylines_out.begin() + infill_start_idx; it != polylines_out.end(); ++it) 
            it->rotate(infill_angle);
        }
    }
}

} // namespace Slic3r