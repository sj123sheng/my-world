# 技术决策

## 2026-08-02：统一地面移动、相机投影与模型朝向坐标约定

- 逻辑世界 `(x, y)` 映射到 3D 地面 `(x, z)`，模型局部 `+Z` 是前方。
- 相机 yaw 的水平前向为 `{sin(yaw), cos(yaw)}`，屏幕右向为
  `{-cos(yaw), sin(yaw)}`；二者遵循右手坐标系。
- 玩家、敌人和首领的模型 yaw 使用 `atan2(worldX, worldZ)`。
- 二维回退视图以左上为原点，因此相机前方映射到负 view-y；转为 NDC 后显示在屏幕上方。
- 移动控制器、`ThirdPersonCamera`、`Camera3D`、`CameraRenderState` 和软锁定必须复用
  `CameraGroundBasisForYaw`，不得各自复制符号公式。
- 验证以真实 `Camera3D::viewProjection()` 投影四组 yaw 为准，并覆盖移动方向、模型面向、
  二维回退投影、软锁定和完整 HAP 构建。

## 2026-08-02：XComponent 是游戏触摸的唯一生产输入源

- `OH_NativeXComponent` 的 `DispatchTouchEvent` 直接接收真实像素坐标，统一驱动移动和相机。
- ArkTS `Joystick` 仅根据触摸绘制视觉反馈，必须保持透明命中，不得再通过
  `pushInput` 生产第二路输入。
- 禁止将 ArkTS 的 vp 坐标和 XComponent 的 px 坐标以同一 pointer id 写入原生
  触摸控制器；否则会重置摇杆原点，造成位移、朝向与视觉摇杆不一致。
- 桥接契约测试必须防止 `Joystick` 重新引入 `pushInput`，并确认 XComponent
  回调仍是唯一生产输入源。
- ArkTS 可交互按钮必须在按钮节点使用 `HitTestMode.Block`，阻断该 pointer
  进入 XComponent；全屏控制根节点使用 `None`，按钮簇空白容器使用
  `Transparent`，使非按钮区仍可控制视角。

## 2026-08-04：角色动画转场一律交叉混合，禁止姿态硬切

- 所有 clip 切换（攻击、受击、闪避、施法、死亡及回到待机/移动）都必须经过
  `SkinnedAnimationState` 的交叉混合路径，时长由纯函数
  `AnimationBlendSeconds(previous, requested)` 统一决定：移动互切 0.15s、
  进入动作 0.12s、动作恢复到移动/待机 0.2s、死亡转场 0.25s。
- 混合策略不得在 `Surface` 或玩法层重复实现；新增动作类别时只扩展
  `AnimationBlendSeconds`，并由 `test_render_animation` 与
  `test_skinned_model` 的混合断言锁定行为。

## 2026-08-04：角色轮廓光逐角色个性化，受击增强、锁定常亮

- 每个角色的轮廓光由 `ActorRimLightFor(AssetProfile, hitFlashSeconds)`
  统一决策：颜色/强度来自档案 `outlineColor`/`outlineStrength`
  （玩家青绿、敌人紫、Boss 品红），受击 0.15s 窗口内强度额外 +0.9
  并向白色靠拢，档案未配置时退回中性轮廓光。
- `Surface` 绘制角色时必须经 `drawActor` 传入档案与闪白计时，绘制结束
  恢复中性轮廓光；禁止在调用点直接 `setRim` 硬编码角色颜色。
- 行为由 `test_asset_profile` 的轮廓光断言锁定（平静态、闪白增强与
  封顶、三角色颜色互异）。
- 软锁定目标轮廓常亮：`Loop` 发布 `targetMarker3d.targetId` 与
  `boss3d.targeted`（首领用 `EncounterController::kBossId` 判定），
  `drawActor` 传入 `targeted` 后 `ActorRimLightFor` 额外 +0.55 强度并
  向锁定环青金色 `{0.35, 0.85, 0.80}` 混合 0.45，与脚下指示环构成
  双重锁定反馈；锁定与受击闪白增强可叠加。
- 高光同样逐角色分档：`AssetProfile.specularStrength/specularShininess`
  主角盔甲强锐（0.42/32）、Boss 宽厚（0.3/20）、敌人哑光退后
  （0.14/12），`drawActor` 绘制后与轮廓光一并恢复中性值
  （0.28/24）；参数由 `test_asset_profile` 的大小关系断言锁定。
- Boss 出场轮廓光渐入：`Loop` 在首领激活且未击败期间累加
  `boss3d.entranceSeconds`（退出/击败归零），`ActorRimLightFor` 的
  `appearance` 参数按纯函数 `BossEntranceReveal`（0.8s 线性）缩放
  最终强度，颜色不变；受击/锁定增强同样受出场进度缩放。

## 2026-08-04：跑动步频随移动输入幅度缩放，禁止滑步

- 地面移速与摇杆幅度成正比，因此跑动动画播放速率必须由纯函数
  `RunPlaybackRate(moveRatio)` 统一决定（满幅 1.0、线性降到下限
  0.45），`SkinnedModel::update` 按该速率推进当前 clip 时间，混合期间
  前一 clip 按切换时快照的 `previousRate` 各自推进。
- `Loop` 发布 `surface.player3dAnimation.moveRatio`（摇杆幅度夹取
  0..1）；非跑动动作一律保持原速率，敌人/首领暂用默认 1.0。
- 行为由 `test_render_animation`（速率曲线与夹取）与
  `test_skinned_model`（半幅摇杆同墙钟位移更小）断言锁定。
- 低速步态分层：`ShouldUseWalkClip(moveRatio)`（阈值 0.35）为真时
  `ResolveClip` 把 `Walking_B` 插到候选链最前，缺失自动回退 run；
  走/跑切换复用动作转场交叉混合，资产无行走 clip 时行为与升级
  前一致，由 `test_render_animation`/`test_skinned_model` 断言锁定。

## 2026-08-04：一次性动画钳制尾帧，禁止循环重播

- clip 播放模式由纯函数 `IsLoopingClip(name)` 分类：`idle`/`run`/
  `Spellcasting`（持续吟唱）循环播放，其余（attack/hit/death/
  Dodge_Forward/单次施法等）播完后钳制在尾帧，避免尸体倒地动作
  或攻击挥砍循环重播。
- 分类在 `copyAnimations` 写入 `OwnedClip.oneShot`，采样时由
  `samplePose` 统一执行钳制/取模；新增 clip 命名必须落入该分类，
  行为由 `test_render_animation` 与 `test_skinned_model` 的尾帧断言锁定。

## 2026-08-04：敌人尸体尾帧保持后线性淡出移除

- 死亡淡出曲线由纯函数 `DeathFadeAlpha(deathSeconds)` 统一决定：
  死亡后保持倒地尾帧 0.35s，再经 0.55s 线性淡出到 0；`Loop` 按
  实体 id 在 `surface.enemyDeathSeconds` 逐帧累加（复活归零、离开
  快照清理），`drawActor` 按 alpha 淡出绘制并在 alpha 归零后整体
  跳过绘制。
- 玩家与首领不适用尸体淡出（玩家死亡走结算、首领走击败演出），
  淡出参数固定传 1.0；行为由 `test_render_animation` 的曲线断言与
  桥接契约的发布/绘制断言锁定。

## 2026-08-04：受击/死亡动画变体轮换，打破重复感

- KayKit 资产自带 `Hit_B`/`Death_B` 变体；`ResolveClip` 增加
  `variant` 参数，奇数变体把 B 版 clip 插到候选链最前，缺失时
  自动回退主 clip，资产无变体时行为与升级前一致。
- 变体由 `Loop` 按实体发布到 `animation.variant`：存活敌人按受击
  计数（`surface.enemyHitCounts`，Damage 事件累加）奇偶轮换受击
  反应；死亡时叠加实体 id 奇偶，让群体倒地姿态互不相同且死亡
  期间恒定，避免尸体在两个姿态间跳变。
- 行为由 `test_render_animation`（候选链与回退）、`test_skinned_model`
  （Hit_B fixture 真正切换）与桥接契约的计数/发布断言锁定。
