# M3-2 glTF 模型加载与骨骼动画设计

日期：2026-07-20

## 目标

在不改动现有 2D 游戏逻辑、HUD 和相机操作的前提下，使用三个可再分发的低多边形
骨骼角色模型，替代 M3-1 的玩家、敌人和 Boss 静态占位网格。角色应能依据既有快照状态
播放待机、移动、攻击、受击和死亡动画。

## 方案选择

采用 `cgltf` 作为 MIT 许可的单文件 C99 glTF 2.0 读取器，并在 Native 渲染层实现精简的
运行时骨骼系统。资产统一采用二进制 `.glb`，避免分散的 JSON、buffer 与纹理文件带来的
打包和路径复杂度。

相比 `tinygltf`，该方案引入的代码和依赖更少；相比自行解析 GLB，能可靠覆盖 glTF 的
访问器、节点、皮肤和动画结构，且不把解析协议维护成本带入项目。

## 资产与许可

- 新增 `assets/models/`，放置玩家、普通敌人和 Boss 三个独立的低多边形骨骼 `.glb` 模型。
- 仅接纳 CC0 或等效的可商用、可再分发许可资产。
- `assets/models/LICENSES.md` 必须记录每份资源的来源 URL、版本、作者/署名要求和授权文本或
  授权链接。
- 资源加载前验证骨骼数、顶点属性、索引与动画访问器；错误日志须包含资产名和拒绝原因。

## 支持的 glTF 子集

M3-2 支持：

- 三角形图元；`POSITION`、`NORMAL`、`TEXCOORD_0`、`JOINTS_0` 和 `WEIGHTS_0`。
- 单套 skin、基础色纹理、节点 TRS 变换。
- 一个顶点最多四个关节影响；一个模型最多 64 根关节。
- `LINEAR` 与 `STEP` 动画插值，以及平移、旋转、缩放通道。

M3-2 明确不支持 PBR 材质、形态键、Draco 压缩、扩展、`CUBICSPLINE` 插值、多 skin 和超过
上述骨骼限制的模型。遇到不支持的数据应加载失败并启用占位网格，不能静默产生错误画面。

## Native 运行时结构

`SkinnedModel` 拥有并释放以下资源：

- CPU 数据：顶点、索引、材质、节点层级、关节索引、逆绑定矩阵和动画 clip。
- GPU 数据：顶点缓冲、索引缓冲、纹理和蒙皮所需顶点属性绑定。
- 动画状态：当前/上一个 clip、播放时间、循环方式和可选的过渡计时。

每帧的动画路径为：采样当前动画的局部 TRS，沿节点树计算全局矩阵，再以
`globalJoint * inverseBindMatrix` 形成关节调色板，上传给 GLES 顶点着色器。着色器根据
`JOINTS_0` 和 `WEIGHTS_0` 进行四权重线性混合蒙皮。

## 游戏状态映射

现有渲染快照映射为：

| 快照状态 | 目标动画 |
| --- | --- |
| 默认 | `idle` |
| 移动 | `run` |
| 攻击 | `attack` |
| 受击 | `hit` |
| 死亡 | `death` |

`idle` 与 `run` 做短时交叉淡入淡出；`attack`、`hit` 和 `death` 立即切换。资源缺失某个动作时，
回退至 `idle` 并记录一次可诊断日志。该映射不改变已有的战斗、AI、输入和数据流。

## 渲染整合与降级

- 扩展现有 `Shader3D`，为同一条着色器路径加入关节/权重属性和关节矩阵数组；地面走非蒙皮分支。
- 启用深度测试和背面剔除，绘制顺序为地面后不透明角色模型。
- M3-1 的玩家、敌人、Boss 静态网格继续保留为加载失败时的可见占位，确保资产问题不会阻止游戏
  启动、移动或战斗。
- 保持既有 Camera3D 控制、2D UI 和 XComponent 生命周期逻辑不变。

## 验收标准

- 本地测试覆盖访问器解析、动画循环时间、LINEAR/STEP 插值、骨骼调色板计算及非法资产拒绝。
- HarmonyOS HAP 构建成功；真机冷启动日志确认 GLB 加载、动画更新和 GLES 3D 渲染链路。
- 真机截图或录屏可辨识三种不同角色模型，且可看到移动、攻击、受击、死亡动画。
- 训练和战斗场景 FPS 不低于 30，无 Native 崩溃、EGL 初始化或着色器失败日志。

## 参考

- Khronos, [glTF 2.0 Specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [cgltf](https://github.com/jkuhlmann/cgltf)（MIT License）
- [Kenney Blocky Characters](https://kenney.nl/assets/blocky-characters)（CC0，作为候选资源来源）
