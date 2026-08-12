# 技术决策

## 2026-08-12：出生安全区、真实速度步态与停步自由环绕

- 删除 `world.json` 的 `sz_spawn_scout`，出生台地不再配置追击型野外敌人；训练假人和其他区域刷怪保持不变。世界 JSON 仍是刷怪布局的唯一事实来源，不增加运行时 zone 特判。
- 主角移动动画比例由控制器平滑后的真实速度归一化得到，不再直接使用瞬时摇杆幅度。走/跑选择使用迟滞，实际 `walk/run` clip 变化也必须走姿态交叉混合。
- 主角完全停稳后保持最后移动朝向；相机可自由环绕 360°。软锁定继续负责选敌、攻击路由、目标表现和战斗镜头距离，但不再常驻覆盖停步主角朝向。
- 影响范围：世界布局生成、野外刷怪数据测试、玩家移动状态发布、蒙皮动画实例和 Loop 相机/锁定集成测试。验证方式详见 `docs/superpowers/specs/2026-08-12-player-locomotion-camera-and-spawn-design.md`。

## 2026-08-11：单区环境视觉层保留高度场玩法真值

- 出生台地—中枢回廊采用“手工视觉层 + 逻辑高度场”双层结构：
  `TerrainHeightfield` 继续唯一决定移动、攀爬和水域；`VisualTerrainCell`
  只补悬崖侧壁、岩层、道路收边与地标轮廓，世界归一化 GLB 使用单位矩阵绘制，
  不经过旧米制环境批次 fit，也不替换基础地面碰撞。
- 环境配置只以 `world.json.environmentVisual` 为事实来源，经 schema 校验和
  `generate_world_layout.mjs` 同时生成 Native 常量与 ArkTS 资源清单；图集、控制图和视觉 GLB 由
  `generate_environment_visuals.mjs` 确定性生成/校验，运行时不解析 JSON。
- 渲染继续使用 GLES3 前向路径：四层地表图集与手绘权重、岩层三平面采样、
  单级 3×3 PCF 方向光阴影、5×2 植被实例批次、轻量程序云、指数距离雾和
  局部水面。明确不引入延迟渲染、实时 GI、全屏 SSAO、级联阴影、SSR。
- `EnvironmentState` 统一输出光、环境色、天空、雾、云、风、降水与水面粗糙度；
  `EnvironmentQualityProfile` 从自动性能级别与手动预设合成的唯一有效档集中派生所有环境降级，
  植被使用稳定散列裁剪避免整片闪烁。
- 原创地表/植被图集由内置图像生成工作流生成后纳入 rawfile；植被源图未带 Alpha，
  因而片元阶段使用低饱和高亮背景色键控裁切。仅借鉴成熟二次元开放世界
  的层次组织，不复制现有游戏资产、纹样或具体场景。
- 验证：地表权重、天气映射、局部水体、植被确定性/排除/批次、纹理解码、
  世界生成契约、Bridge 资源链和 GLES shader 均有自动化覆盖。完整 HAP/真机五机位
  尚受本机 SDK 组件缺失阻塞，不能以旧 47.5 FPS 记录冒充本次基线。

## 2026-08-09：原神式地形升级（数据驱动地貌特征层 + 分区生态着色）

- 原地形只有全局低频正弦基础八度，水面高度 -0.012 随处积出随机水塘，
  没有地貌骨架、没有分区差异，地标/POI/主干道与地形完全脱节。本次把
  地形升级为原神式手工地貌：地貌骨架全部来自数据驱动的特征层，基础
  八度刻意压缓只留呼吸起伏。
- 决策一：数据驱动地形特征层。`assets/world/world.json` 新增
  `terrainFeatures`（22 个特征），schema 与构建期生成脚本
  （automation/assets/generate_world_layout.mjs）扩展，生成为
  `WorldLayout::kTerrainFeatures`；新桥接层 `native/gameplay/world/
  world_terrain.cpp`（makeWorldTerrain/worldRouteSegments）把生成头
  转换为引擎特征列表，`Loop::terrain = makeWorldTerrain()`。特征类型
  四种：Hill 加性丘 / Basin 双向拉平（湖盆与平顶台地中心精确收敛到
  targetHeight）/ Terrace 只抬不压台地 / Ridge 旋转正弦脊线；特征按
  数据顺序依次叠加，数据顺序即地貌合成顺序，可被确定性测试覆盖。
- 决策二：地形与内容布局锁定。22 个特征全部为地标/POI/主干道设计：
  辉光湖盆压出可游泳湖面（target -0.075）、湖心残塔平顶 mesa 提供
  攀爬崖壁 + 滑翔起点（target 0.055）、圣所高原整体抬升形成分区高差、
  灰烬劣地脊线与回廊悬崖提供穿越语言、世界边缘 smoothstep 掩码山脊环
  （edgeMountain 0.09，inner 0.42/outer 0.78）充当天际线远景并遮挡
  世界边界，中心玩法区掩码为 0 不受干扰。水面压低到 -0.045 后水域只由
  湖盆特征决定，基础八度不再产生杂散水塘；所有内容点（POI/宝箱/采集物/
  出生点）干地且坡度 < 0.55，由 test_terrain_heightfield 全量断言锁定。
- 决策三：分区生态着色（biome 语言）。地形片段着色器新增
  uDistrictRects/Grass/Sand/Rock（6 区）按世界坐标矩形距离加权混合
  沙/草/岩配色，相邻分区平滑过渡；三条主干道路径段（world.json
  `routes` → uRouteSegments）在地形上压出路径色带；水面升级双频涟漪 +
  菲涅尔 + 高光闪点。调色板为纯函数 `TerrainBiomeFor(districtId)`
  （terrain_biome.h，六区配色 + 默认回退），test_terrain_biome 锁定。
- 影响范围：terrain_heightfield.h/.cpp 重写（特征层 + 新默认配置），
  world.json/schema/生成脚本/world_layout.gen.h 扩展，world_terrain
  桥接新增，loop.h 接线，shader_3d.cpp/.h 与 surface.cpp 渲染升级，
  CMakeLists 增补。逻辑层地面贴合/攀爬坡度/水域判定与渲染网格采样
  同一高度场，视觉与逻辑严格一致；分块高度范围余量 0.09→0.12 覆盖
  特征层抬升。
- 验证：test_terrain_heightfield（重写，全内容点干地 + 坡度断言）、
  test_terrain_biome（新增）、test_world_layout_gen（扩展）通过；
  全量宿主测试 101 pass / 0 fail / 4 skipped；glslangValidator 对
  地形 frag/vert 零错误；HAP assembleHap BUILD SUCCESSFUL（设备侧
  surface.cpp/shader_3d.cpp 编译通过）。真机观感验收待做。

## 2026-08-09：主角重制模型兼容替换（新骨架 + 新动画语言）

- 主角 player.glb 换为自研 UE 风格骨架模型：41 骨骨架
  （Root/Hip/Pelvis/L_Thigh…R_Hand），无 KayKit handslot.r 武器挂点，
  模型朝向 +X，且 Blender NLA 导出丢失动画名（8 条 clip 均为
  NlaTrack*）。为在不改引擎资产契约的前提下兼容替换，建立
  automation/assets/prepare_player_glb.py 转换管线：FK 采样识别
  clip 语义并重命名（walk/run/Jump_Idle/climb/glide/cast/Dive/
  Turn_180，另合成静态 idle 共 9 条）、剥离根运动（走/跑保留垂直
  弹跳、跳/爬整体钉死）、循环窗口拟合 + 端点混合闭合、按 KayKit
  朝向/偏移补 handslot.r 关节（含 inverseBindMatrix）、Armature
  绕 Y -90° 对齐引擎 +Z 前向。
- 引擎动画语言对应扩展：RenderAnimation 新增 Climb/Glide/Turn
  状态，walk/glide/cast/Jump_Idle 归入循环、Dive/Turn_180/climb
  一次性钳制尾帧；ResolveClip 回退链保证资产缺失时优雅降级
  （闪避→Dive、施法→cast、攀爬→run、滑翔→Jump_Idle）。Loop 新增
  转身状态机：Turn_180 clip 1.62s 内冻结朝向、结束吸附 180°
  （kPlayerTurnClipSeconds 与资产管线 [1.29,2.91] 裁剪一致）；
  glide/climb/swim 分别路由到专属 clip。PlayerController 新增
  turnSpeedScale 参数（默认 1.0）。
- 武器挂点查找改为 FindWeaponJointIndex 回退链：handslot.r→
  R_Hand→RightHand→mixamorig:RightHand，重制主角骨架无 handslot.r
  时仍能挂载程序化佩剑，KayKit 敌人/首领不受影响；surface.cpp
  四处挂点调用统一走该入口。行为由 test_render_animation
  （重制模型动画语言 + 回退链用例）与 test_model_assets
  （42 关节、9 clip manifest）锁定。

## 2026-08-08：首领普攻刀光（首领挥击语言补全）

- 敌人普攻释放已有原型元素色刀光 + 拖尾，但首领普攻挥击落地只有
  火花/齐射/冲击波，近战重劈/旋转冲击缺少挥击刀光，首领普攻的
  "挥击"维度断链。现近战变体（0 重劈/2 旋转冲击）挥击落地瞬间
  挥出阶段元素色大刀光 + 同源拖尾（EnemySlashArc 增 trailKind 字段，
  敌人默认红 kind 8、首领写阶段火花 kind），scale 换算成
  enemyAssetProfile 相对倍率复用敌方刀光渲染管线；吟唱束流（1）
  是远程语言不产生刀光。

## 2026-08-08：玩家受击变体轮换（受击身法去重复）

- 敌人受击已按受击次数奇偶在 hit/Hit_B 间轮换，但玩家 variant 从未
  写入，连续受击始终播放同一 hit clip，重复感明显。现每次新受击
  翻转 player3dAnimation.variant（hit/Hit_B 交替，ResolveClip 变体
  链同源），与敌人受击语言一致；遭遇重置归零。

## 2026-08-08：主角方向闪避动画（闪避运动语言方向化）

- 闪避此前无论方向一律播放 Dodge_Forward，KayKit 资产内
  Dodge_Left/Right/Backward 三个方向 clip 从未使用，侧/后闪避
  姿态与运动方向脱节。现按移动方向相对角色朝向的带符号夹角选
  前/侧/后闪避姿态（PlayerDodgeClipFor 纯函数锁定，|angle|<=pi/4
  前闪避、(pi/4,3pi/4) 侧闪避、>=3pi/4 后闪避，正角 = 左），
  ResolveClip 偏好链扩展到 Dodge，资产缺失逐级回退
  Dodge_Forward→run，静止闪退回退前闪避。

## 2026-08-08：滑翔 FOV 微效（滑翔速度感镜头维度）

- 滑翔此前只有动画姿态 + 风线粒子，镜头维度无速度反馈。现滑翔中
  视场角渐入放宽 +4°（GlideFovOffsetFor 纯函数锁定，0.3s 线性渐入，
  结束后计时器双倍速回落沿同一曲线渐出），与风线粒子共同给出空中
  速度感；与共鸣 FOV 冲击（收窄）叠加不冲突，遭遇重置归零。

## 2026-08-08：敌人死亡元素溶解染色（元素死亡语言）

- 敌人死亡此前只有动画淡出 + 外部爆发特效，尸体本身颜色不变，
  元素系敌人"溶解在自己的元素里"的死亡语言缺失。现尸体淡出期间
  基色向原型元素色线性溶解（DeathDissolveTintFor 纯函数锁定，
  mix = 0.6*(1-fadeAlpha) 随淡出进度增大），遭遇敌人/野怪同语言；
  物理系与未开始淡出原样返回，保持亮金击杀爆裂语义不叠加。

## 2026-08-08：终结技本体染色扩展（元素爆发体态同源）

- 技能释放本体染色此前只覆盖三源质技能吟唱，终结技（元素爆发）
  吟唱期间本体不发光，与金白/青蓝/暗紫爆发特效脱节。现终结技
  吟唱期间本体按出战角色元素染色（skillCastSource 扩展 Ultimate
  分支取 playerSlashSource，已随切人同步），元素爆发体态语言与
  UltimateVfxFor 爆发特效同源；物理角色 playerSlashSource<0 不染色，
  保持通用亮金爆发语义。

## 2026-08-08：主角技能释放本体染色（元素能量聚于本体）

- 源质技能释放此前只有外部特效（光柱/束流/贴花/FOV/暗场），角色
  本体在吟唱期间不发光，"元素能量聚于本体"的施法语言缺失。现
  吟唱期间本体向技能元素色高比例呼吸混合（SkillCastBodyTintFor
  纯函数锁定，混合比 0.20~0.30 强于附魔染色，与脚下附魔环共享
  1.6s 周期），施法状态从外部特效延伸到角色本体；非施法原样返回
  与升级前完全等价，受击闪白后置优先级更高。

## 2026-08-08：首领转阶段失衡动画（转阶段体态语言）

- 首领转阶段此前只有特效/剪影/光环变化，首领本体姿态不变，转阶段
  瞬间缺少"被打破"的体态反馈。现转阶段瞬间（1→2→3，出场 0→1 由
  出场渐入表达除外）首领进入 0.7s 硬直（BossPhaseBreakStaggerSeconds
  纯函数锁定，重于普通受击 0.2s、不轻于破韧硬直），发布侧强制受击
  动画并走 Hit_B 重反应变体（StaggerVariantFor 同源），与转阶段爆发
  特效同帧叠加；phaseBreakSeconds 逐帧衰减、遭遇重置清空。

## 2026-08-08：破韧硬直接线补全（发布侧失衡落地）

- 4354410 的破韧硬直只提交了纯函数（PoiseBreakStaggerSeconds/
  StaggerVariantFor）与 enemyStaggerSeconds 状态字段，loop.cpp 的
  发布侧接线被外部并发进程抹掉，硬直实际从未生效。现补全：破韧
  事件写入 enemyStaggerSeconds，发布侧在硬直窗口强制受击动画并走
  Hit_B 重反应变体（遭遇敌人/野怪同语言），计时器逐帧衰减、遭遇
  重置清空；纯函数测试补齐。

## 2026-08-08：敌人附着本体染色（元素态延伸到本体）

- 敌人元素附着此前只有脚下光环 + 上升粒子，本体基色不变，元素态
  在体表断链（与主角附魔本体染色对称的缺口）。现附着期间本体基色
  向附着元素均色低比例呼吸混合（AuraBodyTintFor 纯函数锁定，
  混合比 0.12~0.20 与脚下附着光环共享 1.6s 周期），多元素附着先取
  均色再混合保证与施加顺序无关；前摇染色/受击闪白后置优先级更高；
  遭遇敌人/训练假人同语言，野怪无附着渲染不参与。

## 2026-08-08：主角附魔本体染色（元素态延伸到本体）

- 附魔期间武器/刀光/伤害飘字/命中火花/脚下环/周身粒子均已按源质
  染色，但角色本体基色不变，元素态在体表断链。现附魔期间本体基色
  向附魔元素色低比例呼吸混合（InfusedBodyTintFor 纯函数锁定，
  混合比 0.10~0.18 与脚下附魔环共享 1.6s 周期），低比例保证不抢
  受击闪白/锁定轮廓的语义优先级；无附魔原样返回与升级前完全等价。

## 2026-08-08：首领阶段剪影（卸甲语言）

- 首领转阶段此前只有特效/光环/血条颜色变化，模型剪影三个阶段完全
  一致。现随阶段推进逐步卸甲：阶段 1 帽+披风+盾全副武装（封锁
  重甲）、阶段 2 卸盾（放开手脚）、阶段 3 卸帽仅披风（蚀质暴露
  本体，与终段狂暴轮廓光/粒子同语言）；BossPhaseAttachmentSetFor
  纯函数锁定装备集下标，未知阶段回退阶段 1 套装，装备集表未构建
  时回退全局开关；转阶段爆发特效掩盖挂件切换瞬间。

## 2026-08-08：首领终段狂暴视觉（终段体态常驻语言）

- 首领阶段语言此前只存在于转阶段瞬间（爆发火花/冲击波/光柱）与
  脚下光环配色，终段没有区别于其他阶段的常駐体态表达。现阶段 3
  体表叠加暗紫呼吸轮廓光（BossBerserkRimFor 纯函数锁定，0.45~1.05
  强度与预警环共享 0.8s 节奏，drawActor rimOverride 叠层保留受击/
  锁定/出场分档），脚下光环透明度提升一档（BossBerserkAuraBoostFor），
  周身蚀质粒子持续上涌（0.12s 节奏，AuraParticleVelocity 同源）；
  其余阶段与升级前完全等价。

## 2026-08-08：破韧硬直动画（破韧窗口体态失衡）

- 破韧此前只有火花/冲击波/卡肉/FOV，敌人本体无反应。现破韧瞬间
  敌人进入 0.6s 硬直（PoiseBreakStaggerSeconds 纯函数），强制受击
  动画并走 Hit_B 重反应变体（StaggerVariantFor），后仰切到更深档
  （峰值约 8°，HitRecoilTiltFor 同源曲线）；计时器逐帧衰减、遭遇
  重置清空。

## 2026-08-08：滑翔风线粒子（滑翔速度感补全）

- 滑翔时角色仍只有地面脚步尾迹（且离地后贴地尾迹不合理）。现
  滑翔中每 0.07s 在角色周身高度发射两颗逆移动方向掠过的淡蓝
  风线（GlideWindVelocityFor 纯函数锁定：相对风 1.5 倍移速 +
  轻微下飘，kind 3 与移动尾迹同源），离地（跳跃/滑翔）停用地面
  脚步尾迹；攀爬/游泳保持原语言。

## 2026-08-08：主角跳跃/落地/滑翔动画（离地运动语言补全）

- 主角跳跃/滑翔此前仍播放跑步/待机动画（KayKit 资产内 5 个跳跃
  clip 从未使用）。现空中播放 Jump_Start（前 0.18s 蹬地）→
  Jump_Idle，滑翔复用空中姿态（资产无专属滑翔 clip），落地边沿
  播放 0.25s Jump_Land + 脚下淡蓝轻尘（PlayerJumpClipFor 纯函数
  锁定，ResolveClip 偏好链扩展到 Jump，缺失资产逐级回退与升级前
  一致）；playerAirSeconds/playerLandSeconds 遭遇重置归零。

## 2026-08-08：附魔命中染色（附魔链补全到命中火花）

- 元素附魔期间刀光/武器拖尾/伤害飘字已按源质染色，但命中火花/
  贴花打物理敌人时仍回退金橙，附魔状态在命中点断链。现附魔期间
  命中火花/贴花/挥击火花/普攻投射物按攻击元素着色
  （InfusedHitSparkKindFor/InfusedHitDecalColorFor 纯函数锁定，
  与 AuraSparkKindFor/AuraColorFor 同源），攻击元素优先于目标
  自身元素；无附魔行为与升级前完全一致。

## 2026-08-08：敌人原型武器差异化（武器与动作语言对齐）

- 六类敌人原型此前一律持法杖，与已差异化的攻击 clip 冲突
  （RiftClaw 徒手爪击却持杖、Bruiser 双手重劈持杖）。现按原型
  配武器（EnemyWeaponKindFor 纯函数锁定：0 徒手无武器/1,2,4 法杖
  施法语言/3 重棍与首领同分量/5 长剑旋转斩），剪影与动作语言
  一致；训练假人保持法杖，武器染色仍走实体 tint 派生。

## 2026-08-08：闪避残影（闪避运动语言补全）

- 闪避此前只有半透明化/尘土/完美闪避反馈，运动轨迹本身无视觉
  残留。现无敌帧窗口内逐帧采样玩家位姿（playerGhostHistory，
  0.24s 上限自动清理），窗口结束后 0.12s 余韵淡出，复用主体已
  上传的蒙皮调色板在当前姿态画两道过去位置残影
  （DodgeGhostAlphaFor 纯函数锁定：按年龄线性衰减峰值 0.28，
  淡蓝与冲刺尘土同源）；无描边无武器不抢主体，遭遇重置清空。

## 2026-08-08：前摇蓄力膨胀（攻击前兆落到体态）

- 前摇前兆已有预警环/聚能粒子/身体染色，但体态本身无变化。现
  前摇期间实体缩放随同一 0.8s 呼吸脉冲轻微放大（WindupScaleFor
  纯函数锁定：1.0 → 1.0+maxInflate，敌人 3.5%、首领 2% 收敛），
  windupPulse01 单点提取与身体染色同源同相位，用体态"吸气"暗示
  力量积蓄；非前摇不受影响，缩放轴心在脚底不影响贴地/血条。

## 2026-08-08：受击旋转后仰（命中身法补全）

- 受击反馈此前只有闪白/平移后仰/方向粒子/Hit 动画，模型姿态本身
  不随命中倾斜。现命中窗口内模型绕局部侧向轴向后倾仰
  （HitRecoilTiltFor 纯函数锁定：与闪白同窗口同平方衰减，敌人/
  假人 0.15s 峰值约 6°，玩家/首领 0.2s 窗口，首领峰值减半控制
  大体量摆动），与平移后仰叠加把"被打实"物理化；窗口外/死亡
  淡出期自然归零，不改变任何 gameplay 位移。

## 2026-08-08：前摇身体染色（攻击前兆落到模型本体）

- 前摇前兆此前都在实体之外（脚下预警环/聚能粒子/蓄力火花），
  模型本体无变化。现前摇期间敌人/首领基色向预警色混合并随
  0.8s 呼吸脉冲（WindupBodyTintFor 纯函数锁定：混合比 0.35~0.65，
  预警色与脚下环同源），在模型本体上给出"它要攻击了"的最直接
  前兆；受击闪白调用侧后置仍优先，死亡/非前摇不受影响。

## 2026-08-08：玩家死亡爆发 VFX（死亡时刻补全）

- 玩家死亡此前只有死亡动画，没有任何瞬间反馈（Death 事件处理
  显式排除 kPlayerId）。现主角倒下瞬间周身暗红火花（受击红 kind，
  PlayerDeathVfxFor 纯函数锁定）+ 暗红冲击波 + 重档 FOV + 72ms
  卡肉 + 2×FP_ONE 震动，把死亡拎成重击时刻；死亡动画与复活/
  重置流程不变。

## 2026-08-08：武器附魔粒子（附魔状态周身化）

- 附魔状态此前只有刀光/拖尾/武器染色 + 脚下光环，角色周身缺少
  持续的元素能量感。现附魔期间主角周身按 AuraParticleInterval()
  节奏发射元素上升粒子（与敌人附着光环上升粒子同源：
  AuraParticleVelocity/AuraSparkKindFor/AuraParticleInterval），
  附魔状态从武器延伸到角色周身，对齐原神武器附魔的能量语言；
  切人重置附魔时粒子随 playerSlashSource 自然停止。

## 2026-08-08：终结技暗场聚焦（元素爆发演出补全）

- 原神元素爆发的标志性演出之一是"世界压暗、聚光角色"。现终结技
  吟唱期间全屏深蓝渐暗（UltimateDimAlphaFor 纯函数锁定：0.15s
  淡入到 0.22 上限），吟唱结束双倍速回落；与既有光柱/FOV/卡肉
  叠加，爆发从"角色身上的特效"升级为"接管整个画面"的演出。
  暗场累加器在遭遇重置时归零，防止跨遭遇残留。

## 2026-08-08：终锻打断爆发（首领机制打断高光）

- 三阶段终锻机制要求玩家在吟唱内用共鸣终结技打断，但打断成功
  瞬间没有任何视觉反馈（机制静默转 None），玩家不知道自己打断
  成功了。现 Loop 侧边沿检测（prevFinalForgeCasting：吟唱中→
  机制 None 且非超时失败）触发亮金碎裂爆发：24 颗亮金火花 +
  冲击波 + 光柱 + 符阵 + 80ms 卡肉 + 重档 FOV + 2×FP_ONE 震动，
  与首领仪式时刻同规格；超时失败（failedMechanic）不触发，
  打断成功与失败观感明确区分。
- 补充：打断瞬间播放 CastBarBroken 噪声碎裂音效——该音效此前只有
  事件映射无任何触发源，现与打断爆发同帧落地，视听同步。

## 2026-08-08：伤害飘字元素着色（元素伤害数字语言）

- 伤害飘字此前只有白/金/红三档，附魔状态在数字上不可读。
  现 DamageNumberKind 扩展三系元素 kind（Radiance/Current/
  Corruption），主角附魔期间（playerSlashSource>=0）对敌伤害
  飘字按源质色着色（元素色优先于大额金色，对齐原神元素伤害
  数字语言）；玩家受击红/物理大额金保持不变，渲染层 tint 与
  AuraColorFor 全链路同源。

## 2026-08-08：首领仪式时刻相机震动（震动矩阵补全）

- 首领出场/转阶段/死亡爆发此前只有卡肉 + FOV 冲击。现仪式时刻
  （bossCameraFeedback 消费点）统一追加 2×FP_ONE 相机震动（与
  首领砸地/元素反应同级），首领大时刻的镜头位移维度补全；
  震动矩阵覆盖：受击/终结段/反应/破韧/首领砸地/首领仪式。

## 2026-08-08：切人附魔重置（附魔跟随角色）

- 切人此前不重置武器附魔，新角色继承上一角色的源质染色，
  与"角色有自己的元素"的语言冲突。现切人瞬间把 playerSlashSource
  重置为新角色自身源质（CharacterSourceFor 纯函数锁定：1辉印/
  2脉流/3蚀质/其余物理 -1），刀光/拖尾/武器染色/附魔光环随之
  切换；物理角色切出即回中性。CharacterSwitchVfxFor/UltimateVfxFor
  重构为同源消费 CharacterSourceFor，角色元素映射单点维护。

## 2026-08-08：终结技元素着色（元素爆发按角色源质）

- 终结技此前固定亮金，角色池扩展后所有角色爆发同色。现按出战
  角色源质着色（UltimateVfxFor 纯函数锁定：1辉印金白/2脉流青蓝/
  3蚀质暗紫，物理/未知保持通用亮金），火花/束流/冲击波/符阵/
  光柱全链路同色，与切人出场/元素技能同语言——每个角色的爆发
  有自己的颜色，对齐原神"元素爆发即角色名片"的语言。

## 2026-08-08：反应/破韧相机震动（高光时刻分量补齐）

- 元素反应与破韧此前只有 FOV 冲击 + 卡肉，镜头位移维度缺席。
  现两事件经 triggerCameraShake 触发震动：元素反应 2×FP_ONE
  （战斗最高光时刻，与首领砸地同级），破韧 FP_ONE（与卡肉/FOV
  同节奏的轻震）；与受击震动同通道，峰值取值不叠加。

## 2026-08-08：终结段相机震动（分量链补全）

- 连段终结段（第 4 击）此前有放大刀光 + 地面冲击波 + 贴花 +
  专属重劈动画，但镜头无反应。现挥击瞬间触发轻档相机震动
  （triggerCameraShake(FP_ONE)，幅度与受击同级、轻于首领砸地），
  补全终结段"刀光 + 地面 + 动画 + 镜头"的完整分量链。

## 2026-08-08：元素死亡光柱（元素死亡语言补全）

- 元素系敌人死亡此前只有元素火花 + 小型冲击波，缺少垂直维度
  的死亡签名。现死亡点升起小型元素色光柱（与共鸣/终结光柱
  同曲线，高度 0.10×实体缩放），元素死亡语言与附着/技能/死亡
  爆发全链路同源；物理系亮金击杀爆裂保持原样不叠加光柱，
  光柱上限 16 防群怪死亡刷屏。

## 2026-08-08：首领死亡爆发 VFX（出场仪式的收尾呼应）

- 首领击败此前只有死亡动画 + 渐隐，没有专属视觉高潮。现击败
  边沿（bossPrevDefeated，存活→击败只触发一次）在首领周身爆发
  阶段元素色大火花（32 颗，规模 1.3×）+ 冲击波 + 光柱 + 符阵，
  镜头反馈（重卡肉 + FOV 冲击）与出场/转阶段同源，把击杀拎成
  高光时刻；新遭遇重置 bossPrevDefeated 防止跨遭遇误触发。

## 2026-08-08：主角附魔光环 + 首领阶段光环（持续状态光环）

- 元素附魔此前只有武器染色/刀光染色，角色状态在远处不可读；
  首领阶段语言只在爆发瞬间出现。现 drawAuraRings 扩展两个常驻
  呼吸环：主角附魔期间脚下浮现源质色淡环（透明度 0.55 降档，
  与武器染色/刀光染色同 playerSlashSource 状态驱动）；首领脚下
  持续阶段元素色环（透明度 0.7，与转阶段爆发/血条阶段色同源）。
  附着光环的呼吸曲线/加法混合复用，状态环从爆发延伸到常驻。

## 2026-08-08：前摇聚能粒子（蓄力前兆连续化）

- 敌人/首领前摇此前只有上升沿一次蓄力火花 + 预警环，吟唱窗口
  中段没有持续的"正在蓄力"指示。现吟唱期间每 60ms 向实体周身
  发射一轮向内汇聚粒子（ConvergingSparkMotion 纯函数：出生点在
  半径圆环、速度指向圆心、寿命结束恰好抵达中心），颜色与蓄力
  火花同源（敌人按原型元素 kind、首领按阶段 kind），把蓄力从
  一次性提示变成连续能量流，对齐原神敌人蓄力前兆语言。

## 2026-08-08：首领挥击落地相机震动（砸地体量感）

- 首领普攻挥击落地此前只有火花/齐射/冲击波，镜头无反应，
  闪避成功后重击落地的分量感缺失。现 spawnEnemyReleaseVfx
  改返回 EnemyReleaseVfxResult（出场/转阶段镜头反馈与挥击落地
  分层），落地边沿经 VfxSystem.triggerCameraShake(2*FP_ONE)
  触发相机震动——与受击震动同通道同曲线（正弦振荡×线性衰减），
  幅度加倍突出首领体量；新增公开方法与 consume(CameraShake)
  共享峰值取值语义，测试锁定两路径一致。

## 2026-08-08：完美闪避 VFX（补齐原神完美闪避语言）

- 无敌帧内闪过敌人攻击时，战斗层已发 Dodge gameplay 事件
  （wasInvulnerableAt 判定），但此前只有闪避音效 + 全屏蓝闪，
  没有专属视觉爆发。现 Loop 消费 Dodge 事件：主角周身淡蓝火花 +
  冲击波（PerfectDodgeVfxFor/DodgeDustColor 纯函数锁定，与冲刺
  尘土同淡蓝闪避语言）+ 中档 FOV 冲击 + 56ms 卡肉替代原版慢镜感，
  把"擦弹闪过"从隐性判定拎成高光时刻。

## 2026-08-08：三系技能释放剪影差异化（技能不再同形）

- 三系元素技能此前释放动效完全同形（火花+投射物+冲击波+符阵，
  仅颜色不同）。现按源质语义加差异化点缀（SkillCastAccentFor
  纯函数锁定）：辉印=光柱（辉印降临）、脉流=追加 3 发束流
  （流动投射物强化）、蚀质=贴地蚀斑（腐蚀残留）。三技能释放
  轮廓从此可剪影区分，对齐原神"每个技能有自己的形状"的语言。

## 2026-08-08：攻击动画差异化（连段/原型/首领变体）

- 此前主角四段连招、六类敌人原型、首领三普攻变体共用同一条
  attack clip，动作语言单一。现按语义分派 KayKit 库存中的专用
  clip（PlayerAttackClipFor/EnemyAttackClipFor/BossAttackClipFor/
  PlayerComboSegmentFor 纯函数锁定）：主角 1斜劈 2横斩 3突刺
  4双手重劈（终结段呼应放大刀光/地面冲击波）；敌人爪击/仪式/
  盾击/重斩/法术射击/旋转斩；首领重劈/吟唱束流/旋转冲击。
- 机制：ActorRenderState 增加 attackClip 偏好字段，ResolveClip
  攻击意图下优先选用、资产缺失自动回退通用 attack，不新增资产。

## 2026-08-08：敌人血条元素边框（系别可读不抢血量可读性）

- 敌人头顶血条此前统一深色底 + 血量渐变填充，系别信息只在技能/
  附着特效里。现元素系敌人（Priest/Caster/Elite）血条深色底外圈
  一层薄元素色框（AuraColorFor 同源，EnemyElementFor 判定），
  扫一眼即读系别；血量填充仍走绿→红渐变保持可读，物理敌人
  保持纯深色底不抢戏。边框在背景条之后绘制（z 偏移 -0.0002），
  滞后条/前景条层级不受影响。

## 2026-08-08：FOV 冲击轻重分档（镜头重量层级）

- 此前所有 FOV 冲击（反应/终结技/首领/破韧）共用固定 -7° 振幅，
  元素技能释放与角色切换没有镜头反馈。现引入三档振幅
  （FovPunchMaxOffsetFor 纯函数：元素技能 -4° / 切人 -5° /
  反应、终结技、首领、破韧 -7°），Surface 新增 fovPunchMaxOffset
  字段按触发点写入，渲染层据此收窄视场角；元素技能释放同时补
  40ms 卡肉。高频小动作与低频大爆发的镜头冲击分层，对齐原神
  技能分量语言。

## 2026-08-08：角色切换出场仪式强化（切人即出场）

- 切人此前只有一圈元素火花 + 音效，出场感弱。现按出战角色源质
  （CharacterSwitchVfxFor 纯函数：1辉印金白/2脉流青蓝/3蚀质暗紫，
  其余通用金橙）释放完整出场仪式：16 颗元素火花 + 冲击波 + 光柱 +
  符阵，配合 FOV 收窄冲击与 48ms 卡肉，把静默切换拎成一次出场，
  对齐原神切人镜头语言；元素色与附着/技能全链路同源。

## 2026-08-08：锁定标记元素色化（锁定即读系别）

- 软锁定指示环此前固定青蓝。现锁定元素目标时指示环混入 45% 目标
  元素色（TargetMarkerRenderState 增加 element 字段，逻辑层经
  resolveEnemyElement 写入），锁定同时提示目标系别；物理/首领/假人
  保持青蓝"已锁定"语义。元素语言延伸到锁定反馈，全链路一致。

## 2026-08-08：首领血条阶段元素着色（HUD 阶段语言统一）

- Boss 血条此前固定品红（#AA4F86），阶段信息只在调试文本里。
  现血条颜色随 bossPhase 切换：1=辉印金白、2=脉流青蓝、3=蚀质暗紫，
  与 3D 侧 BossPhaseVfxFor（转阶段爆发/出场爆发/预警环）同色，
  HUD 与战斗场景的阶段语言统一，玩家扫一眼血条即知当前阶段。

## 2026-08-08：首领机制吟唱条（补齐吟唱进度可视化）

- bossCastRatio（机制吟唱进度）此前已由 Native 快照发布并经 N-API
  传到 HUD（Hud.ets @Prop），但从未渲染——玩家看不到审判光束等
  机制的吟唱进度。现在 Boss 血条下方渲染金色吟唱条（仅吟唱期间
  显示），给玩家明确的打断/闪避时机窗口，对齐原神首领机制的
  读条语言。

## 2026-08-08：普攻终结段地面反馈（连段收尾仪式感）

- 连段第 4 击（终结段）此前只有放大的刀光（SlashArcPoseAt 终结
  加成）与火花，缺少地面反馈。现挥击瞬间主角脚下爆出金橙冲击波 +
  贴地贴花（与终结刀光金橙同色），强化连段收尾仪式感，对齐原神
  普攻终结段的分量感。
- 触发条件复用连段边沿检测（comboSegment>=4），不新增状态字段。

## 2026-08-08：破韧爆发 VFX（补齐破韧视觉反馈）

- 破韧（PoiseBreak）此前只有合成音效与 vfxSystem 标志位，渲染层
  从未消费该标志，玩家打碎敌人韧性没有任何视觉反馈。现在事件
  循环直接处理 PoiseBreak gameplay 事件：受击者位置爆发 18 颗亮金
  碎裂火花 + 亮金冲击波，伴随 72ms 卡肉（重于普攻命中、轻于转
  阶段）+ FOV 冲击，传达"防线破碎、全力输出"窗口。
- 走 gameplay 事件通道（带 target）而非 vfxSystem 标志位，因为
  爆发需要实体位置；与死亡/元素反应事件同模板。

## 2026-08-08：首领出场爆发 VFX（原神首领出场仪式）

- 首领激活瞬间（未激活→激活边沿）周身爆发阶段元素色 VFX：
  24 颗大火花 + 冲击波 + 光柱 + 旋转符文环（BossPhaseVfxFor 同源
  配色），并施加与转阶段同源的 80ms 卡肉 + FOV 冲击；与既有出场
  渐入轮廓光（BossEntranceReveal）同帧叠加，形成完整的出场仪式。
- 边沿状态 surface.bossPrevActive 在首领停用/击败/resetInput 时
  归零，跨遭遇重新出场可再次触发；spawnEnemyReleaseVfx 返回值语义
  升级为"首领戏剧性事件"（出场/转阶段）统一驱动镜头反馈。

## 2026-08-08：命中反馈元素色化（火花/贴花/方向粒子）

- 命中元素敌人时，命中火花、贴地冲击贴花与受击方向性粒子统一按
  目标元素色爆散（AuraSparkKindFor/AuraColorFor 同源）——玩家每一
  次命中都能读出打的是什么系敌人，与附着/死亡/技能语言闭环。
- 玩家受击保持红色（危险语义）、物理命中保持金橙（通用命中），
  仅元素实体改变配色；元素归属复用死亡爆发的 resolveEnemyElement
  查询，不新增事件。

## 2026-08-08：闪避冲刺动效（脚下淡蓝尘土）

- 闪避此前只有全屏蓝闪（vfxDodgeFlash），缺少脚下反馈。现进入
  Dodging 状态上升沿在主角脚下爆出 10 颗淡蓝冲刺尘土（复用移动
  尾迹 kind 3 配色）+ 小型淡蓝贴地贴花，与全屏蓝闪呼应，强化
  原神式侧身闪避的灵动感。
- 边沿检测复用终结技同款 currentAction 状态机通道
  （prevActionForVfx），不新增状态字段。

## 2026-08-08：前摇预警环元素色化（危险语义 + 元素可读性）

- 敌方前摇预警环从统一红色升级为按原型元素染色：物理原型保持
  警示红，元素原型按 60% 元素色 + 40% 警示红混合——既保留
  "快闪避"的危险语义（红通道始终主导），又让玩家在闪避窗口
  内读出攻击携带的元素系别（Priest 金白 / Caster 青蓝 / Elite 暗紫）。
- 首领吟唱预警环随当前阶段主导源质着色（BossPhaseVfxFor 同源），
  转阶段后预警环颜色随之切换，与转阶段 VFX/阶段语言形成闭环。
- 配色为纯函数 WindupWarningColorFor/BossWindupWarningColorFor，
  test 锁定；渲染层 drawWindupWarnings 由统一色改为逐实体设色。

## 2026-08-08：元素附着施加爆发（附着瞬间元素迸发）

- 原神式附着反馈补全：此前附着只有持续态呼吸光环，缺少"附着
  瞬间"的迸发。现目标新附着源质（掩码新增位）时在其位置爆出
  8 颗元素火花 + 小型元素贴地贴花，颜色/火花 kind 与附着光环
  同源（AuraColorFor/AuraSparkKindFor）。
- 实现为附着掩码差分边沿检测（Loop::prevAuraMasks，实体 id →
  上一步掩码），不新增战斗事件、不改事件结构；遭遇敌人取自
  EncounterEnemySnapshot 附着位，训练假人取自战斗快照附着位，
  死亡实体掩码归零、离场实体边沿状态清理，resetInput 全量清空。

## 2026-08-08：武器附魔发光（刃面源质染色）

- 原神元素附魔语言补全最后一环：此前附魔只染刀光与拖尾，武器
  本体保持冷银。现附魔期间佩剑刃面基色向源质色混合（45% 刃色 +
  55% 元素色）并整体提亮 15%，武器本身泛元素光；无附魔原样刃色，
  受击闪白仍优先染白。
- 染色为纯函数 WeaponInfusionTintFor（lastSource<0 回退原刃色），
  与 SlashArcColorFor/WeaponTrailKindFor 同一 playerSlashSource
  状态驱动，test 锁定；drawActor 增加 infusionSource 参数，
  主角绘制传入 surface.playerSlashSource。

## 2026-08-08：NPC 市民装备变体（建模剪影差异化）

- NPC 模型槽位注入的是 player.glb 字节（GamePage 已实现 npc.glb
  优先 + player.glb 回退），但此前无 ModelKind::Npc 装备分支，
  NPC 恒为裸体低模。本次补齐：披风为基础着装（Knight_Cape 全局
  启用），按 id 取模分配三种逐实例变体——披风市民 / 头盔+披风
  民兵 / 披风+盾卫兵，与全副武装的主角（头盔+披风+盾）区分剪影。
- 变体选择为纯函数 NpcAttachmentVariantFor（id 取模、数量<=0
  回退 0），test 锁定；复用敌人原型的 buildAttachmentOverride 与
  drawActor 覆盖参数通道，不换资产即完成市民着装。

## 2026-08-08：元素死亡爆发（敌人死亡元素色爆散）

- 死亡爆发按原型元素色化（原神式死亡反馈）：resolveEnemyElement 从
  遭遇/野外快照（含死亡槽位）查实体元素归属，元素系敌人死亡爆出
  元素色火花（16 颗）+ 小型元素冲击波；物理系/首领/训练假人保持
  亮金击杀爆裂，保留统一击杀确认感。
- 颜色语言与 EnemyElementFor/AuraColorFor 同源：Priest 金白爆散、
  Caster 青蓝爆散、Elite 暗紫爆散，玩家凭死亡颜色读出击杀对象系别。

## 2026-08-08：终结技爆发强化与敌方技能元素色化

- 终结技（CastingUltimate 上升沿）在既有火花/束流/冲击波/符阵之上
  追加：施法者位置亮金光柱（与共鸣光柱同源曲线）+ FOV 收窄冲击 +
  64ms 卡肉（重于普攻命中 40ms、轻于转阶段 80ms），把终结一击从
  普通技能里拎出来，对齐原神元素爆发的镜头仪式感。
- 敌方技能元素色化（原神式敌方元素可读性）：EnemyElementFor 纯函数
  按原型定元素——Priest=辉印金白、Caster=脉流青蓝、Elite=蚀质暗紫，
  RiftClaw/Guard/Bruiser=物理红；蓄力火花/投射物 kind 与刀光颜色
  随之染色（EnemySlashArc 增加 color 字段，渲染层不再硬编码红）。
- 元素色与主角附魔/附着光环同语言（AuraColorFor 同源），玩家凭
  颜色即可读出敌人系别；物理系保持红色维持既有近战认知。

## 2026-08-08：首领转阶段 VFX（原神首领转阶段仪式）

- 首领阶段跳变（1→2→3）瞬间周身爆发阶段元素色 VFX：28 颗大火花 +
  冲击波环 + 光柱 + 旋转符文环，整体规模随阶段递增（1.0/1.15/1.3，
  终阶段最猛烈），并由调用侧追加 80ms 卡肉 + FOV 冲击（与元素反应
  同源镜头语言，上限 96ms）。
- 配色/火花 kind/规模为纯函数 BossPhaseVfxFor 决策：辉印封锁金白 /
  脉流风暴青蓝 / 蚀质崩塌暗紫，对应三阶段主导源质；未知阶段回退
  辉印配色，test_combat_vfx 锁定。
- 边沿检测在 spawnEnemyReleaseVfx 内（改为返回 bool，调用侧施加
  卡肉/FOV）；激活 0→1 由出场渐入表达不视为转阶段，bossPrevPhase
  在首领失活/击败/resetInput 时归零，避免跨遭遇误触发。

## 2026-08-08：击杀帧软锁定释放时序与野外刷怪测试隔离

- 软锁定释放复核必须用本步 encounter.update 结算后的新鲜候选：
  帧首候选是上一步快照，击杀同帧死者仍在旧列表，锁定会多挂一帧；
  击杀触发的命中卡肉（40ms 起）又冻结后续固定步，把延迟放大成
  幽灵锁定（test_loop_integration 基线失败根因）。释放时同步关闭
  targetMarker3d 并切回探索视角，避免残留一帧幽灵标记。
- WildSpawnSystem::candidates() 增加 target.alive() 兜底过滤：
  野外敌人 deathTick 在死亡次帧才结算，存活判定保证刚被击杀的
  敌人立即退出候选（与遭遇候选的 alive 过滤对齐）。
- WildSpawnSystem 新增 resetZones（清空槽位/事件并重建 zones）；
  时序敏感集成测试以空 zones 隔离出生点侦察敌 sz_spawn_scout，
  避免其在感知半径内立即仇恨攻击玩家、抢占软锁定干扰纯时序断言。

## 2026-08-08：元素附魔刀光（普攻染色跟随源质）

- 施放元素技能后武器附着对应源质（原神元素附魔语言）：普攻
  刀光与武器拖尾随之染色——辉印金白/脉流青蓝/蚀质暗紫，
  直到施放另一系源质替换；终结段固定金橙不受附魔影响。
- 染色决策为纯函数 SlashArcColorFor/WeaponTrailKindFor
  （未知源质回退默认金白），附魔状态存于 surface.playerSlashSource，
  遭遇重置归零；拖尾火花配色表扩至 kind 0..10。
- 把"技能释放"与"普攻表现"连成一条视觉语言：附魔后每一段
  挥击都带源质色刀光 + 拖尾，与附着光环/反应爆发形成闭环。

## 2026-08-08：敌人原型装备差异化（逐实例挂件覆盖）

- SkinnedModel::draw 新增逐实例挂件覆盖参数（与 attachmentNames()
  同序的 bool 表，nullptr 回退全局开关）；启用判定抽为纯函数
  AttachmentEnabledFor（覆盖优先、越界回退全局），test 锁定。
- 6 类敌人原型共享法师模型，用装备组合差异化剪影：
  RiftClaw/Bruiser=披风、Priest=帽+翻开法术书、Guard/Elite=
  帽+披风+合起法术书、Caster=帽+披风+翻开法术书；训练假人走
  全局默认（帽+披风+法术书），遭遇/野外敌人共用同一查表。
- 配合既有原型缩放/色调，敌人从"同一模型换色"升级为可辨识的
  装备剪影差异，向原神敌人辨识度靠拢。

## 2026-08-08：KayKit 模块化装备刚性挂件（角色建模升级）

- SkinnedModel loader 扩展：无 skin 的网格节点（KayKit 模块化装备：
  头盔/披风/盾牌/副手武器等）沿父链绑定最近皮肤关节，加载期把
  bind(J)×父链变换烘焙进顶点位置，成为单关节全权重蒙皮——与本体
  同一 VBO/IBO 绘制，描边/受击闪白/卡通/尸体淡出共用同一管线，
  反向壳描边自动覆盖装备轮廓。
- 挂件按节点名注册、默认全部关闭（一个 GLB 内含全部变体）；
  surface 按角色启用所需变体：骑士=头盔+披风+左手圆盾（右手保留
  程序化佩剑）、法师=帽子+披风+左手法术书（右手保留程序化法杖）、
  野蛮人=帽子+披风+左手圆盾（右手保留程序化重棍）。
- 缩小原神级建模差距的第一步：不换 GLB 资产即让三类角色从裸体
  低模变为穿戴装备的剪影；烘焙正确性由 makeRigidAttachmentGlb
  夹具 + 顶点位置断言锁定，真实资产审计更新到 test_model_assets。

## 2026-08-08：元素技能符文环（施法法阵）

- 三系元素技能与终结技释放瞬间，施法者脚下浮现旋转双新月符阵
  （原神技能法阵语言）：复用刀光新月网格，两弧相差 180°，
  外层柔晕 + 内层亮芯，缓出旋转约 240° + 淡入淡出，0.5s。
- 颜色与技能源质一致（辉印金白/脉流青蓝/蚀质暗紫/终结亮金），
  半径随玩家模型缩放（终结技更大）；与冲击波同层先于角色绘制，
  不进存档；曲线为纯函数 SkillRunePoseAt，test_combat_vfx 锁定。

## 2026-08-08：共鸣 FOV 冲击与反应卡肉

- Resonance 事件触发相机 FOV 收窄冲击（45°→38°，前 20% 快速下潜、
  后 80% 缓出恢复，0.45s）并追加 60ms 命中卡肉（上限提到 96ms），
  把元素反应从普通命中里拎出来，形成原神元素爆发的镜头仪式感。
- FOV 偏移为纯函数 FovPunchOffsetAt（maxOffset 传负值收窄），
  update3DCamera 逐帧应用并在无激活时恢复默认 45°；遭遇重置
  同步清空光柱与 FOV 计时。

## 2026-08-08：武器挥舞粒子拖尾（原神式武器流光）

- 普攻挥击窗口内沿刀光扫掠角（SlashArcPoseAt 同源）每帧发射一颗
  拖尾粒子：主角金白（kind=7）、敌方红（kind=8），切向速度 +
  轻微上扬，kind>=3 不受重力，流光沿挥舞轨迹漂浮消散。
- 发射点极坐标 = 角色朝向 + 刀光扫掠角，半径 1.9×刀光缩放
  （终结段同步放大），主角高度取 playerGroundHeight + 胸口档，
  与佩剑挂载/刀光共用同一挥击时间轴，不进存档。
- 曲线/速度为纯函数 WeaponTrailPoseAt/WeaponTrailVelocity，
  test_combat_vfx 锁定；火花配色表扩至 kind 0..8。

## 2026-08-08：共鸣爆发光柱

- 元素反应触发瞬间从受击点升起垂直元素光柱（原神元素爆发语言）：
  0.55s 缓出上升 → 满高保持 → 线性衰减；双层加法混合（外柔晕 +
  内亮芯），绕 Y 轴 billboard 始终面向相机，画在角色层之上包裹
  受击实体，深度只读不写。
- 光柱高度随实体模型缩放同步（ratio），共鸣爆发反应额外加高 60%；
  颜色复用 ReactionVfxFor 元素色，与火花/冲击波/贴花共用同一
  Resonance 事件瞬时表现管线，不进存档；曲线为纯函数
  LightPillarPoseAt，test_combat_vfx 锁定。

## 2026-08-08：bloom 后处理（原神式技能发光）

- 场景先渲染入全分辨率 FBO（RGBA8 + 深度 renderbuffer），再做
  亮通提取（软膝亮度阈值 0.62）→ 半分辨率 9-tap 高斯 ping-pong
  模糊 ×2 → 与默认帧缓冲加法合成（强度 0.85），让加法混合的
  刀光/冲击波/火花/附着光环产生溢出光晕，技能释放观感向原神靠拢。
- 三 pass 共用一个程序（uMode 切换），全屏三角形由 gl_VertexID
  生成（无 VBO）；阈值/强度/降采样/卷积权重为 bloom_pass.h
  纯函数（BloomParamsFor/BloomDownsampleSize/BloomGaussianWeight），
  由 tests/test_bloom_pass.cpp 断言锁定。
- 仅高画质预设（qualityPreset=0）启用，低画质整条管线跳过；
  程序编译或 FBO 创建失败自动回退直绘。FBO 随窗口尺寸惰性重建，
  销毁纳入 SurfaceGlResource::BloomPipeline 档位（先于 Shader3D）。

## 2026-08-08：元素附着光环可视化（原神式附着指示）

- 目标附着源质时脚下浮现对应元素色呼吸光环 + 周身上升元素粒子：
  辉印金白 / 脉流青蓝 / 蚀质暗紫，与元素反应爆发同色系语义一致；
  颜色/火花 kind/环脉动/粒子节奏全部是 combat_vfx.h 纯函数
  （AuraColorFor、AuraSparkKindFor、AuraRingPoseAt、AuraParticleVelocity），
  测试断言锁定。
- 附着位经 EncounterEnemySnapshot → Enemy3DRenderState.auraMask
  位掩码（bit0=辉印 bit1=脉流 bit2=蚀质）逐敌人发布；训练假人
  仅在训练模式取战斗快照附着位，其余模式恒 0，避免与遭遇敌人
  重复绘制。野外敌人无源质容器，不参与附着表现。
- 多源质同时附着绘制同心多环，相位错开 1/3 周期错峰脉动；
  粒子每 0.16s 每附着源质各发射一颗，径向外飘 + 上升，复用火花
  管线（kind>=4 不受重力自然上升消散）。环/粒子尺寸随模型缩放
  同步；瞬时表现不进存档。

## 2026-08-08：三源共鸣元素反应 3D 爆发特效

- Resonance 事件在受击点爆发大规模元素色反馈：16 颗元素火花 +
  冲击波环 + 贴地贴花，颜色/火花 kind 由纯函数 `ReactionVfxFor`
  按 ResonanceType 分档：折光金白、凝滞青蓝、崩解暗紫、共鸣爆发亮金，
  未知类型回退折光配色，不产生黑环。
- 反应类型取自战斗快照 `currentReaction`（同帧单次反应准确），
  不改战斗事件 schema；受击点位置经 resolveEntityPosition 解析，
  尺寸随受击实体模型缩放同步。
- 元素反应是核心原创机制，此前仅有 2D 全屏金光；3D 化后与
  火花/冲击波/贴花共用同一套瞬时表现管线，不进存档。

## 2026-08-08：受击方向性粒子沿攻击方向喷射

- 伤害命中时除环形火花外，额外沿攻击方向（攻击者→受击者）喷射 4 颗
  方向性火花：速度方向由纯函数 `DirectionalSparkVelocity` 决定
  （方向归一化 + ±60° 横向散布旋转 + 上扬分量，只旋转不缩放），
  LCG 决定散布/速度抖动，同输入可重现。
- kind<=2 的方向性火花复用既有火花重力，形成抛物线拖尾；攻击者位置
  不可解析（环境伤害）或攻防同点时跳过，不产生退化方向。
- 方向性粒子与环形火花/贴花/飘字同源于 Damage 事件，共享 128 火花
  上限与尺寸缩放（受击实体模型比例），不新增存档字段。

## 2026-08-08：命中贴地冲击贴花

- 伤害命中瞬间在受击点地面浮现短促源质色光斑（0.35s）：玩家受击红色、
  命中敌方金橙，重击（≥15）半径更大；半径按受击实体模型缩放同步。
- 贴花曲线由纯函数 `ImpactDecalPoseAt` 决定：前 40% 时长缓出扩张到满
  半径，之后半径保持仅线性淡出，避免贴花“游走”；渲染层用 shadowMesh
  单位圆盘加法混合绘制，深度只读不写。
- 贴花与火花/飘字同源于 Damage 事件，状态不进存档；列表上限 24 防溢出，
  Loop 负责计时与过期清理，Surface 只按 pose 绘制。

## 2026-08-07：敌方/首领武器挂载与火花速度对齐拉伸

- 敌方法杖（createStaff：杆 + 箍环 + 宝珠）与首领重棍（createClub：
  粗杆 + 椭球锤头）复用主角同一 `handslot.r` 挂点机制：提交 enemy.glb /
  boss.glb 时 `FindJointIndex` 解析挂点，绘制用 `角色矩阵 × 关节矩阵`
  跟随攻击/移动动画；野外敌人的原型缩放经角色矩阵自动传导到武器。
- 命中火花/投射物改为速度对齐拉伸：纯函数 `SparkStretchFor` 把世界速度
  按 cameraBillboard 的逆旋转（RotY(-yaw)·RotX(-pitch)·RotY(π)）投影到
  相机平面，得出流光旋转角与拉伸倍率（1 + 14×平面速度，封顶 3.2）；
  平面速度 < 0.02 保持圆形广告牌，尾迹/静止火花不被拉成细线。
- 修复 createCylinder 侧面卷绕：旧实现每段输出两组三角形，其中一组
  卷绕反向（GL_BACK 剔除下不可见）且重复覆盖；只保留外翻一组，
  三角形数减半，卷绕由 test_mesh 断言锁定。
- 拉伸/旋转只发生在渲染层广告牌矩阵，不改变火花逻辑积分与寿命。

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
