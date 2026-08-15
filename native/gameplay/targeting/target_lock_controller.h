#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "native/engine/core/tick_clock.h"
#include "native/engine/math/vec2.h"
#include "native/gameplay/combat/event.h"

// 目标锁定模式（Plan 2）：Automatic 距离优先自动选敌，Manual 手动循环。
enum class TargetLockMode { Automatic = 0, Manual = 1 };

// 锁定候选：由 Loop 统一收集（遭遇敌人/野外敌人/Boss），Boss 不强制抢锁。
struct TargetLockCandidate {
  EntityId id = 0;
  Vec2 position;
  bool alive = true;
  bool attackable = true;
  bool boss = false;
};

// 锁定结果：唯一目标 ID 与表现层需要的模式/距离/角度/锁定环可见性。
// 攻击结算、投射物、镜头、脚下环、轮廓和血条高亮必须消费同一个 id。
struct TargetLockResult {
  std::optional<EntityId> id;
  TargetLockMode mode = TargetLockMode::Automatic;
  float distance = 0.0f;
  float angle = 0.0f;
  bool showMarker = false;
};

struct TargetLockConfig {
  // 自动获取距离与角度上限（与 SoftTargeting 默认一致）。
  float maxDistance = 0.75f;
  float maxAngle = 1.0471976f;
  // 攻击停止后锁定环保持显示的窗口（毫秒）。
  Tick markerFadeMs = 800;
};

// 唯一目标状态机：取代 Loop 直接调用 SoftTargeting。独占模式、当前 ID、
// 超距/死亡重选与自动锁定活跃窗口；候选测量复用 MeasureTarget 纯函数。
class TargetLockController {
 public:
  explicit TargetLockController(TargetLockConfig config = {});

  // 自动模式刷新：距离优先 (distance, angle, id) 选敌；连招活跃且当前
  // 目标仍存活且在维持距离内时保持当前 ID；attackTriggered/comboActive
  // 刷新活跃窗口，窗口外 showMarker 淡出。
  TargetLockResult updateAutomatic(Vec2 player, float cameraYaw,
                                   const std::vector<TargetLockCandidate>& candidates,
                                   bool attackTriggered, bool comboActive,
                                   Tick now);

  // 手动模式：单击锁定最近目标，再次单击按 (distance, id) 顺序循环
  //（环绕）。手动维持距离为自动获取距离的 1.5 倍，不受镜头角度限制。
  // 无有效候选时回到 Automatic 且 ID 空。手动模式锁定环始终显示。
  TargetLockResult cycleManual(Vec2 player, float cameraYaw,
                               const std::vector<TargetLockCandidate>& candidates,
                               Tick now);

  // 解除手动锁定：切回 Automatic 并立即按自动规则刷新，
  // 不伪造攻击活跃窗口。
  TargetLockResult releaseManual(Vec2 player, float cameraYaw,
                                 const std::vector<TargetLockCandidate>& candidates,
                                 Tick now);

  // 每帧维护：手动模式下目标死亡/超距/卸载时重选最近有效候选，
  // 无候选时回到 Automatic；自动模式下等价无攻击触发的 updateAutomatic。
  TargetLockResult refresh(Vec2 player, float cameraYaw,
                           const std::vector<TargetLockCandidate>& candidates,
                           Tick now);

  // 目标失效（死亡/卸载）通知：当前目标命中时立即放弃，下次刷新重选。
  void invalidate(EntityId id);

  // 完全复位：清空当前目标、模式与活跃窗口。
  void clear();

  // 锁定环可见度（0..1）：手动模式常亮 0.92 基线；自动模式活跃窗口内
  // 0.72 基线，停止战斗后随 markerFadeMs 线性衰减到 0。无目标恒 0。
  float markerVisibility(Tick now) const;

  TargetLockMode mode() const { return mode_; }
  std::optional<EntityId> currentId() const { return currentId_; }

 private:
  TargetLockConfig config_;
  TargetLockMode mode_ = TargetLockMode::Automatic;
  std::optional<EntityId> currentId_;
  // 最近一次攻击/连招活跃时间；无活跃记录时为 kNoActivity。
  Tick lastActivityMs_ = kNoActivity;

  static constexpr Tick kNoActivity = INT64_MIN;
};
