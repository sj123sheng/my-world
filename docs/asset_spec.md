# 角色模型资产规格书（独立高模资产验收标准）

更新时间：2026-08-08

本规格是外包、AI 生成或自制角色资产的入库验收标准。满足本规格的
GLB 放入 `entry/src/main/resources/rawfile/models/` 即可被引擎直接
消费，无需代码改动（身份层测试与许可清单除外，见"入库流程"）。

## 1. 资产槽位

| 槽位文件 | 必需性 | 用途 | 缺失行为 |
| --- | --- | --- | --- |
| `player.glb` | 必需 | 主角 | 静态 Mesh 回退 |
| `enemy.glb` | 必需 | 敌人共享模型（训练假人共用） | 静态 Mesh 回退 |
| `boss.glb` | 必需 | 首领 | 静态 Mesh 回退 |
| `npc.glb` | 可选 | NPC 市民 | 复用 player.glb 字节 |
| `enemy_<archetype>.glb`（0..5） | 可选 | 敌人原型独立高模 | 该原型回退共享 enemy.glb |

敌人原型编号：0=RiftClaw 1=Priest 2=Guard 3=Bruiser 4=Caster
5=Elite。独立原型槽位整体回退：模型、武器挂点、挂件覆盖三者同源，
要么全用独立模型，要么全回退共享模型。

## 2. 加载器硬约束（不满足则解析失败）

- 格式：glTF 2.0 二进制（`.glb`），纹理内嵌，禁止外部 URI。
- 图元模式必须为 `TRIANGLES`，全文件恰好一个 skin。
- 关节数 ≤ 64（着色器 uniform 数组硬上限）。
- 每顶点必须含 `POSITION / NORMAL / TEXCOORD_0 / JOINTS_0 /
  WEIGHTS_0`；不支持 `JOINTS_1 / WEIGHTS_1`，即每顶点最多 4 个
  骨骼影响，权重和不得为 0。
- 动画插值仅支持 `LINEAR / STEP`，禁止 `CUBICSPLINE`。
- 至少 1 张内嵌 base color 纹理（卡通管线依赖纹理采样）。

## 3. 骨骼契约

- **强烈推荐**复用 KayKit 41 骨骨架（骨骼名完全一致）：现有 76 条
  动画 clip 可原样复用，连招/闪避/跳跃/施法全部自动生效。
- 必需关节：`handslot.r`（右手武器挂点，按名查找；缺失时武器不
  挂载）。
- 自定义骨架允许，但必须自带第 4 节全部必需 clip，且保留
  `handslot.r` 命名。

## 4. 动作 clip 契约

必需 clip（缺一即测试失败）：`idle`、`run`、`attack`、`hit`、
`death`。

推荐变体 clip（缺失时 `ResolveClip` 自动回退，不报错但表现降级）：

| 分组 | clip 名 | 用途 |
| --- | --- | --- |
| 步态 | `Walking_B` | 低速行走（否则慢速跑） |
| 受击/死亡变体 | `Hit_B`、`Death_B` | 奇偶轮换打破重复感 |
| 方向闪避 | `Dodge_Forward/Left/Right/Backward` | 主角方向闪避 |
| 跳跃 | `Jump_Start`、`Jump_Idle`、`Jump_Land` | 起跳/空中/落地 |
| 主角四段连招 | `1H_Melee_Attack_Slice_Diagonal`、`1H_Melee_Attack_Slice_Horizontal`、`1H_Melee_Attack_Stab`、`2H_Melee_Attack_Chop` | 斜劈/横斩/突刺/终结重劈 |
| 敌人原型攻击 | `Unarmed_Melee_Attack_Punch_A`（0）、`Spellcast_Raise`（1）、`Block_Attack`（2）、`2H_Melee_Attack_Chop`（3）、`Spellcast_Shoot`（4）、`2H_Melee_Attack_Spin`（5） | 六原型攻击语言 |
| 技能 | `Spellcast_Raise`、`Spellcast_Shoot`、`Spellcasting`、`Spellcast_Long` | 三系技能与终结技 |

循环语义：`idle / run / Spellcasting` 循环播放，其余 clip 播完后
钳制尾帧。

## 5. 挂件（刚性装备）契约

- 无 skin 的网格节点、父链挂在皮肤关节上，会被加载器烘焙为单关节
  全权重挂件，随本体共用描边/闪白/卡通管线。
- `enemy_<archetype>.glb`：GLB 内全部挂件**默认启用**——独立原型
  资产应只包含该原型自己的剪影装备，不要塞变体。
- 共享 `enemy.glb` / `player.glb` / `boss.glb` / `npc.glb`：按节点
  名启用（KayKit 命名：`Knight_Helmet`、`Mage_Cape`、
  `Barbarian_Round_Shield` 等）。新资产要么沿用这些节点名，要么
  同步更新 `surface.cpp` 的 `tryInitializeModelAsset` 启用表。

## 6. 性能预算（移动端 GLES + 反向壳描边翻倍顶点负载）

| 槽位 | 三角面建议 | 说明 |
| --- | --- | --- |
| 主角 / Boss | ≤ 4 万 | 单体特写，预算最宽 |
| 敌人原型 | ≤ 2.5 万 | 同屏多只，含野外群怪 |
| NPC | ≤ 2 万 | 非战斗实体 |

- 纹理：单张 1024×1024 手绘卡通 base color；引擎为 toon 着色，
  不需要法线/金属度/粗糙度贴图。
- 关节数建议保持 41（复用动画库），硬上限 64。

## 7. Blender 导出设置

- 导出器：glTF 2.0，格式 `.glb`，勾选 Skin + Animation。
- 应用全部修改器；纹理打包进 GLB（Path Mode: Copy 内嵌）。
- 动画名即 clip 名（按第 4 节命名，区分大小写）。
- 骨架骨骼名按第 3 节契约；`handslot.r` 作为手挂点骨骼保留。
- 导出后先跑 `tests/test_model_assets`（清单校验）再入库。

## 8. 入库流程

1. 文件放入 `entry/src/main/resources/rawfile/models/`。
2. 更新 `tests/test_model_assets.cpp` 的 `kManifest`：正式替换三类
   主角时把身份层字段（关节数/clip 数/挂件名）改为新资产的值；
   可选槽位（npc/enemy_<i>）注入后保持契约层校验即可。
3. 更新 `assets/models/LICENSES.md`：作者、许可证、来源、SHA-256
   （AI 生成资产同样需要登记生成工具与商用许可条款）。
4. 宿主测试全绿后打 HAP 真机验收：toon 着色/描边/受击闪白/挂件
   跟随/武器挂载/VFX 锚点（脚下光环、命中火花位置）。
5. 如新模型体量与 KayKit 差异明显，调整 `AssetProfile::forModel`
   的 scale/outlineWidth/specular 分档（`VfxSizeRatio` 会自动按
   基准缩放同步特效尺寸）。

## 9. 注入链路（参考）

`GamePage.ets` 从 rawfile 读取字节 →
`nativeSetModelAssets / nativeSetNpcAsset /
nativeSetEnemyArchetypeAsset` → Surface pending 字节 →
current GL context 下解析上传；解析失败自动回退（静态 Mesh 或
共享模型），不影响其余槽位。
