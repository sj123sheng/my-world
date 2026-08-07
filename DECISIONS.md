# 技术决策

## 2026-08-07：主角佩剑按 handslot.r 骨骼挂载，立方体卷绕修复

- `SkinnedModel` 解析时保存与蒙皮调色板同序的关节名（`jointNames()`），
  武器挂点用纯函数 `FindJointIndex` 按名查找；KayKit 三份角色 GLB 的右手
  挂点均为 `handslot.r`，由 `test_model_assets` 对真实资产断言锁定。
- 主角佩剑为程序化网格 `createSword`（柄/护手/柄头三个盒体 + 菱形截面
  剑刃棱柱，逐面独立顶点），绘制时用 `角色矩阵 × 调色板关节矩阵`
  （globalTransform × inverseBind）挂载，严格跟随手部动画；剑身与本体
  共用同一轮廓光/描边/受击闪白决策，武器状态不进存档。
- 修复 `appendFace`（createCube 唯一来源）三角形卷绕与面法线反向的历史
  问题：GL_BACK 剔除下立方体各面原本不可见，影响所有盒体回退几何与
  剑柄；新卷绕由 `testCubeWindingMatchesFaceNormals` 与剑网格卷绕断言锁定。
- 剑网格挂载失败（关节缺失/模型未就绪）时静默回退无武器绘制，
  不影响角色本体与其余特效。

## 2026-08-07：原神式角色渲染：卡通着色、反向壳描边与普攻刀光

- 角色（主角/敌人/首领/NPC）漫反射由连续 Lambert 改为两段式卡通（cel）着色：
  片段着色器按 `smoothstep(uToonEdge±uToonSoftness, NdotL)` 量化明暗带，暗带乘以
  逐角色阴影色 `AssetProfile.shadowColor`（主角冷紫、敌人冷灰蓝、Boss 深紫、
  NPC 中性）；地形/水面/天空不受影响（uToon 默认 0，drawActor 绘制后恢复关闭）。
- 轮廓线改用反向壳（inverted hull）：主体 pass 之后剔除正面、把背面沿
  （蒙皮后）法线外推绘制纯色轮廓，与原有菲涅尔轮廓光叠加。线宽
  `AssetProfile.outlineWidth` 按体量分档（Boss 0.00062 > 主角 0.00038 >
  敌人 0.00032 > NPC 0.00030，世界单位），纯函数 `ActorOutlineWidthFor`
  在受击闪白窗口最多加宽 75%、锁定时 +12%、按出场进度线性缩放；线色
  `ActorOutlineColorFor` 与轮廓光同一决策派生后压暗，天然继承受击变白
  与锁定青金染色。顶点着色器外推宽度为模型局部空间，调用方按
  `世界宽度 / profile.scale` 换算。
- 普攻刀光与技能冲击波全部纯函数驱动：`SlashArcPoseAt`（0.26s 内扫掠
  -1.15→+1.15rad，缓出曲线，连段第 4 击放大 30% 并提亮）与
  `ShockwavePoseAt`（0.45s 缓出扩张 + 线性淡出）。Loop 只做边沿触发与
  计时推进，Surface 只按 pose 绘制；刀光几何 `createSlashArc` 为两端
  收拢的新月弧，双层加法混合（外柔晕 + 内亮芯）模拟渐变。主角刀光
  金白、敌人红色、终结段更亮；冲击波在三种源技能、终结技与首领
  吟唱/挥击边缘按源质配色生成。
- 刀光/冲击波状态是瞬时表现层数据，不进存档；列表上限（敌刀光 16、
  冲击波 24）防溢出，过期由时长纯函数统一判定后清理。
- 行为由 `test_combat_vfx`（曲线）、`test_asset_profile`（toon/描边分档
  与派生函数）、`test_shader_3d`（宿主机状态机）、`test_mesh`（弧线
  几何）断言锁定；OHOS arm64 HAP 构建通过。

## 2026-08-07：探索反馈事件由 Native 统一生产

- 地标发现、机关激活、路径门开启和探索奖励领取只在成功状态变化时产生一次
  `ExplorationFeedback` 事件；失败交互不产生成功反馈。
- 反馈事件是瞬时状态，不进入 V9 存档；快照字段只追加在尾部，供 ArkTS 边沿检测消费。
- Native 继续负责探索音效，ArkTS 只根据快照显示统一 `ExplorationToast` 并触发震动，
  不在 UI 中复制探索状态判断。

## 2026-08-07：动态路径门独立于静态建筑碰撞

- `BuildingCollision` 继续只负责静态环境；`ExplorationGateCollision` 根据
  `ExplorationContent` 当前状态生成关闭门 OBB，不复制或持久化第二份门状态。
- 玩家、野外敌人、遭遇敌人和首领统一先解算静态建筑、再解算动态路径门，避免同一门对不同
  实体出现通行语义分裂；机关激活与存档恢复后统一重建动态门集合。
- 路径门几何继续由 `assets/world/world.json` 经构建期生成进入 C++，运行时不解析 JSON；
  阻挡提示只作为聚合快照尾部字段进入 ArkTS，不改变 V9 存档字段顺序。

## 2026-08-07：单区精做作为开放世界垂直切片边界

- 本阶段将启明台地、翠风低地、辉光湖畔和中枢回廊收敛为一条可连续游玩的主区域，
  灰烬荒原与圣所高地只保留远景或锁定入口，不以地图面积替代内容密度。
- 垂直切片的体验目标是 30～60 分钟完成“探索 → 发现 → 战斗 → 成长 → 首领”闭环；
  跳跃、空中、滑翔、攀爬和游泳必须各自服务于路径、奖励或信息发现。
- 抽卡、背包、武器和圣遗物保留为轻量成长入口，但不得成为主线通关的运行时前置依赖。

## 2026-08-07：探索内容数据驱动与 V9 存档边界

- `assets/world/world.json` 是 POI、机关、路径门和探索奖励的唯一事实来源，
  `automation/assets/generate_world_layout.mjs` 在构建期生成 `native/generated/world_layout.gen.h`；
  运行时不解析 JSON，也不在玩法代码中复制探索节点坐标和文案。
- `PointOfInterest`、`PuzzleNode`、`TraversalGate` 和 `ExplorationReward` 通过探索事件进入
  任务、奖励、地图目标和存档，玩法系统之间不直接互相修改进度。
- 存档格式升级为 V9，只在 V8 字段尾部追加五个探索位掩码；读取 V1～V8 时探索字段强制归零，
  防止调用方复用 `SaveState` 时继承旧内存状态。
- ArkTS 只消费聚合探索快照并发出粗粒度命令，XComponent 仍是移动与相机的唯一生产输入源。

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

## 2026-08-06：世界布局采用构建期代码生成，不引入运行时 JSON 解析

- 世界布局（区块/锚点/NPC/刷怪区/宝箱/采集物）以 `assets/world/world.json`
  为单一事实来源，由 `automation/assets/generate_world_layout.mjs` 在构建期
  生成 `native/generated/world_layout.gen.h`（`namespace WorldLayout` 下的
  constexpr 结构体与数组），运行时零 JSON 依赖。
- 选择构建期代码生成而非运行时 JSON 解析：零新增依赖（生成脚本为手写
  轻量校验器，不引入 npm/JSON 解析库）、确定性可测（同输入产物逐字节
  一致，脚本幂等，重复运行不重写文件）、编译期即可捕获布局错误。
- 布局约束由 `config/schema/world.schema.json`（draft-07 子集）定义，
  生成脚本额外做语义校验：id 全局唯一（锚点≥8、NPC/宝箱/采集物≥32，
  避开旧布局与存档 bitmask）、坐标落在 [0.02, 0.98]、district 分块
  覆盖全 8×8 网格且互不重叠、实体必须落在其声明 district 的分块内。
- `automation/hvigor/check_rules.js` 挂接 world.json 存在性与基础结构
  校验，作为构建期第一道闸；完整校验与生成由生成脚本承担。
- 数据自洽性由 `test_world_layout_gen` 对生成头的断言锁定（id 下限、
  坐标界内、archetype 合法、district 不重叠）。
