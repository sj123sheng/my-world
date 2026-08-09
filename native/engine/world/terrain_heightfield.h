#pragma once

#include <cstdint>
#include <vector>

// 确定性程序化地形高度场（开放世界探索基础）。
// 基础层用低频正弦组合生成世界 [0,1]x[0,1] 上的缓起伏，再叠加
// 数据驱动的地形特征层（原神式手工地貌），保证：
// 1. 同坐标查询永远返回同一高度（无随机状态），可被确定性测试覆盖；
// 2. 逻辑层据此做地面贴合、攀爬坡度判定与水面区域判定；
// 3. 渲染层采样同一函数生成地形网格，逻辑与视觉严格一致。
// 地貌构成：基础八度刻意压缓（单独叠加永远不会产生可攀爬坡度），
// 地貌骨架全部来自特征层——湖盆、平顶台地、高原、劣地脊线、缓丘、
// 攀爬悬崖与天际线 accent 峰；世界边缘再叠加平滑掩码抬升的山脊环，
// 充当天际线远景并遮挡世界边界，掩码在世界中心（出生点与战斗区）
// 附近为 0，不干扰核心玩法区。
// 逻辑坐标 (x, y) 对应 3D 世界 (x, height, z=y)。
struct TerrainConfig {
  // 基础起伏幅度与频率：宽缓低频，单独不产生可攀爬坡度。
  float amplitude = 0.016f;
  float frequency = 2.0f;
  // 次级褶皱：给地形增加细节但不显著抬升坡度。
  float detailAmplitude = 0.003f;
  float detailFrequency = 7.0f;
  // 水面高度：地面高度低于该值的区域视为水域（游泳）。
  // 压低到 -0.045 后基础八度不再随处积出随机水塘，水域只由湖盆特征决定。
  float waterLevel = -0.045f;
  // 可行走最大坡度（高度变化/水平距离）：超过即视为可攀爬面。
  float climbSlopeThreshold = 0.55f;
  // 有限差分步长：用于坡度估计。
  float slopeSampleStep = 0.004f;
  // 山脊 octave：相位错开的中频褶皱，进一步丰富地貌层次。
  float ridgeAmplitude = 0.006f;
  float ridgeFrequency = 5.0f;
  // 边缘山脊环：按到世界中心距离的 smoothstep 掩码在世界边缘抬升的山体高度。
  // 高度与内外半径共同保证环坡最大坡度 < climbSlopeThreshold，
  // 边缘内容点（宝箱/采集物）仍可正常行走。
  float edgeMountainHeight = 0.09f;
  // 掩码内圈半径：该范围内山体贡献为 0（保护出生点与中心玩法区）。
  float edgeMountainInnerRadius = 0.42f;
  // 掩码外圈半径：达到该距离后山体完全生效。
  float edgeMountainOuterRadius = 0.78f;
};

struct Vec2;

// 地形特征类型（与 world.json terrainFeatures.kind 一一对应，
// 生成头 WorldTerrainFeatureDef.kind 的整数值与此枚举严格一致）。
enum class TerrainFeatureKind : int32_t {
  // 加性丘：高度 += mask * amplitude（amplitude 可为负表示洼地）。
  Hill = 0,
  // 双向拉平：高度 += mask * (targetHeight - 高度)，中心处精确收敛到
  // targetHeight；湖盆（target < 水面）与平顶台地都用它保证高度确定性。
  Basin = 1,
  // 只抬升台地：高度 += mask * max(0, targetHeight - 高度)，保留原有起伏。
  Terrace = 2,
  // 掩码脊线：高度 += mask * amplitude * 旋转正弦乘积（frequency/angleRadians
  // 控制频率与走向），用于劣地/高原纹理。
  Ridge = 3,
};

// 单个地形特征：椭圆径向掩码（中心 1 → 半径处 0，smoothstep 过渡），
// feather 控制过渡带宽度占半径的比例（0..1）。所有字段有限时行为确定。
struct TerrainFeature {
  TerrainFeatureKind kind = TerrainFeatureKind::Hill;
  float x = 0.5f;
  float y = 0.5f;
  float radiusX = 0.1f;
  float radiusY = 0.1f;
  float amplitude = 0.0f;
  float targetHeight = 0.0f;
  float frequency = 0.0f;
  float angleRadians = 0.0f;
  float feather = 0.5f;
};

class TerrainHeightfield {
 public:
  explicit TerrainHeightfield(TerrainConfig config = {});
  // 带特征层构造：特征按数组顺序依次叠加（数据顺序即地貌合成顺序）。
  TerrainHeightfield(TerrainConfig config, std::vector<TerrainFeature> features);

  // 地面高度查询：对任意有限输入返回有限结果；
  // 越界坐标按世界边界钳制采样。
  float heightAt(float x, float y) const;
  // 坡度估计：|∇h| 的近似模长（高度变化率），>=0。
  float slopeAt(float x, float y) const;
  // 是否可攀爬面：坡度超过阈值。
  bool climbableAt(float x, float y) const;
  // 是否水域：地面高度低于水面。
  bool waterAt(float x, float y) const;

  // 单个特征在 (x, y) 的椭圆掩码值 [0,1]：纯函数，供测试断言。
  static float featureMask(const TerrainFeature& feature, float x, float y);

  const TerrainConfig& config() const { return config_; }
  const std::vector<TerrainFeature>& features() const { return features_; }

 private:
  TerrainConfig config_;
  std::vector<TerrainFeature> features_;
};
