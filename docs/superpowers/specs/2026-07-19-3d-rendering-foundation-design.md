# M3-1 3D 渲染基础设计

**日期：** 2026-07-19
**状态：** 待确认
**上游依赖：** 垂直切片六个阶段全部收口（阶段 1-6 自动化、构建和真机验收通过）
**范围：** 3D 数学库、透视相机、网格渲染、纹理加载、基础光照和 3D 着色器

## 1. 目标与边界

M3-1 在现有 GLES3 渲染层上搭建 3D 透视渲染管线，替换当前的 2D 屏幕空间渲染。2D 游戏逻辑
（位置、移动、碰撞、AI、战斗）完全不变，只升级渲染层。玩家、敌人、首领的 2D 位置
`(x, y)` 映射到 3D 世界坐标 `(x, 0, y)`，即站在 y=0 的地面上。相机从斜上方看向玩家，
用透视投影产生纵深感。

本阶段交付：

- glm 3D 数学库集成（矩阵、向量、四元数、变换函数）。
- 3D 透视相机（第三人称跟随、触摸手势控制 yaw/pitch、透视投影）。
- 硬编码网格渲染（立方体代表角色、平面代表地面、四边形代表 VFX 叠加）。
- 纹理加载（stb_image 加载 PNG，生成 GL 纹理）。
- 3D 着色器（MVP 变换 + 方向光照 + 纹理采样）。
- 与现有 2D 渲染的共存（2D 灰盒保留为调试回退路径）。

明确不属于 M3-1：

- glTF 模型加载和骨骼动画（M3-2）。
- 卡通着色、边缘光和后处理（M3-3）。
- 3D 碰撞、3D 寻路和 z 轴玩法（后续里程碑）。
- 真实美术资源（用硬编码几何体验证管线）。
- 新增 N-API 接口或 ArkTS 改动（纯 Native 渲染层改动）。

## 2. 架构原则

3D 渲染层遵循"只读消费 2D 逻辑"原则。数据流为：

```text
2D 游戏逻辑 (不变)
  -> 位置映射 (x, y) -> (x, 0, y) 世界坐标
  -> 3D 透视相机 (glm::lookAt + glm::perspective)
  -> MVP 矩阵 (model * view * projection)
  -> 3D 网格着色器 (顶点/法线/UV + 光照 + 纹理)
  -> GLES3 提交
```

必须保持以下边界：

- 3D 渲染层只读取 2D 位置和相机手势状态，不反向修改游戏逻辑。
- 2D 灰盒渲染保留为调试回退，不删除。
- 3D 着色器与现有 2D 单色着色器独立，不共享 Program。
- 纹理加载失败时回退到纯色渲染，不阻塞游戏循环。
- 网格 GPU 资源在 Surface 销毁时释放，不泄漏。

## 3. 核心组件

### 3.1 glm 集成

glm 是头文件库，加入 CMakeLists include path 即可，无需链接。macOS 测试和 HarmonyOS
NDK 均兼容。使用 `glm::mat4`、`glm::vec3`、`glm::vec2`、`glm::radians`、`glm::perspective`、
`glm::lookAt`、`glm::translate`、`glm::rotate`、`glm::scale` 等标准函数。

glm 头文件放在 `native/engine/math/glm/` 目录下（随仓库分发），或通过 CMakeLists 指向
系统安装路径。M3-1 采用随仓库分发方式，避免环境依赖。

### 3.2 3D 透视相机

```text
struct Camera3D {
  glm::vec3 position;
  glm::vec3 target;
  float fov = 60.0f;
  float nearPlane = 0.1f;
  float farPlane = 100.0f;
  float aspectRatio = 1.0f;

  void follow(glm::vec3 playerPos, float yaw, float pitch, float distance);
  glm::mat4 viewMatrix() const;
  glm::mat4 projectionMatrix() const;
  glm::mat4 viewProjection() const;
};
```

`follow()` 根据玩家 3D 位置、yaw、pitch 和 distance 计算相机 position 和 target。
yaw 控制水平旋转，pitch 控制俯仰角，distance 控制相机与玩家的距离。计算方式为球坐标
转笛卡尔坐标。`viewMatrix()` 返回 `glm::lookAt(position, target, up)`，`projectionMatrix()`
返回 `glm::perspective(radians(fov), aspectRatio, nearPlane, farPlane)`。

相机在 `Loop::tickOnce` 中每帧更新，输入来自现有 `camera.yaw()`、`camera.pitch()` 和
玩家位置。现有 `ThirdPersonCamera` 的触摸手势链路不变，只是输出从 2D 变换变成 3D 矩阵。

### 3.3 网格

```text
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  GLuint vbo = 0;
  GLuint ibo = 0;
  GLuint texture = 0;

  void upload();   // 上传 VBO/IBO 到 GPU
  void draw() const; // 绘制
  void destroy();  // 释放 GPU 资源
};

Mesh createCube(float size);
Mesh createPlane(float width, float depth);
```

`createCube()` 生成 24 个顶点（每面 4 个，确保法线独立）和 36 个索引（12 个三角形）。
`createPlane()` 生成 4 个顶点和 6 个索引。`upload()` 创建 VBO 和 IBO，`draw()` 绑定缓冲区
并调用 `glDrawElements`。`texture` 为 0 时使用纯色回退。

### 3.4 纹理加载

使用 stb_image（头文件库）加载 PNG 文件。`loadTexture(path)` 返回 GL 纹理句柄，失败返回 0。
M3-1 使用一张简单测试贴图（如纯色或棋盘格 PNG）验证管线。纹理加载在 Surface 初始化时
执行一次，不在帧循环中重复加载。

stb_image 头文件放在 `native/engine/render/stb_image.h`（随仓库分发）。

### 3.5 3D 着色器

顶点着色器：

```glsl
uniform mat4 uMVP;
uniform mat4 uModel;
attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aUV;
varying vec3 vNormal;
varying vec2 vUV;
void main() {
  gl_Position = uMVP * vec4(aPosition, 1.0);
  vNormal = mat3(uModel) * aNormal;
  vUV = aUV;
}
```

片段着色器：

```glsl
uniform sampler2D uTexture;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform bool uHasTexture;
varying vec3 vNormal;
varying vec2 vUV;
void main() {
  vec3 N = normalize(vNormal);
  float diff = max(dot(N, normalize(uLightDir)), 0.0);
  vec3 color = uAmbient + diff * uLightColor;
  if (uHasTexture) {
    gl_FragColor = vec4(color, 1.0) * texture2D(uTexture, vUV);
  } else {
    gl_FragColor = vec4(color, 1.0);
  }
}
```

一个方向光（`uLightDir` + `uLightColor`）加环境光（`uAmbient`），足够让 3D 几何体有
明暗立体感。M3-3 在此基础上替换为卡通着色。

### 3.6 Surface 扩展

`Surface` 增加以下字段：

```text
Camera3D camera3d;
Mesh playerMesh;
Mesh groundMesh;
Mesh enemyMesh;
Mesh bossMesh;
GLuint shader3d = 0;
glm::vec3 lightDir;
glm::vec3 lightColor;
glm::vec3 ambient;
```

`surface_draw` 在现有 2D 绘制后增加 3D 绘制阶段：

1. 使用 `shader3d` 着色器程序。
2. 设置相机 VP 矩阵和光照 uniform。
3. 画地面平面（大平面，纹理或纯色）。
4. 画玩家立方体（在玩家 3D 位置，缩放为合理大小）。
5. 画敌人立方体（在敌人位置，按存活状态跳过死亡敌人）。
6. 画首领立方体（在首领位置，按阶段配色）。
7. 2D VFX 叠加保留在现有 2D 绘制阶段。

## 4. 数据与接口

### 4.1 位置映射

2D 位置 `(x, y)`（0-1 范围）映射到 3D 世界坐标 `(x, 0, y)`。地面平面在 y=0，大小覆盖
整个可玩区域。角色立方体在 y=0.5（半高，使其站在地面上）。

### 4.2 相机输入

相机 yaw/pitch 来自现有 `ThirdPersonCamera`，distance 固定。`Camera3D::follow()` 接收
这些值并计算 3D 位置。触摸手势链路不变。

### 4.3 敌人和首领位置

`Loop::tickOnce` 在 `surface_draw` 前把 `EncounterSnapshot::enemies` 和 `boss` 的 2D 位置
写入 `Surface` 的 3D 渲染字段。渲染层只读这些字段。

## 5. 测试策略

### 5.1 单元测试（macOS 纯逻辑）

- `Camera3D`：`follow()` 后 position 和 target 正确、`viewMatrix()` 和
  `projectionMatrix()` 维度正确、FOV 和宽高比变化时投影矩阵响应。
- `Mesh`：`createCube()` 顶点数 24、索引数 36、法线方向朝外；`createPlane()` 顶点数 4、
  索引数 6。

### 5.2 真机验证

- 3D 立方体可见且有透视纵深。
- 方向光照让几何体有明暗面。
- 2D 玩法（移动、战斗、AI、首领）不回归。
- 帧率不低于 30 FPS。

### 5.3 不新增合约测试

M3-1 不新增 N-API 接口或 ArkTS 改动，现有 Node 合约测试保持通过即可。

## 6. 验收出口

| 出口 | 标准 |
|---|---|
| 自动化 | macOS 纯逻辑测试全通过（含新增 Camera3D/Mesh 测试），现有合约测试不回归 |
| 生产构建 | Hvigor `assembleHap` 返回 `BUILD SUCCESSFUL`，signed HAP 可安装 |
| 真机渲染 | 3D 立方体可见、透视正确、光照有明暗、2D 玩法不回归、帧率 >= 30 FPS |

## 7. 约束与风险

- glm 和 stb_image 随仓库分发，避免环境依赖。
- 3D 着色器与 2D 着色器独立，不共享 Program，避免属性冲突。
- 纹理加载失败回退纯色，不阻塞渲染。
- 网格 GPU 资源在 `surface_destroy` 中释放。
- 3D 绘制在 2D 绘制后执行，避免深度缓冲冲突（M3-1 暂不启用深度测试，用绘制顺序保证
  正确性；M3-2 接入模型后启用深度缓冲）。

## 8. 实施约束

本设计批准并保存后，编写分阶段实施计划。实施计划按 glm 集成 → Camera3D → Mesh →
纹理加载 → 3D 着色器 → Surface 集成 → 真机验证的顺序拆分。每个 task 包含失败测试、
实现和提交三步。不允许在 Camera3D 未通过单元测试前接入 Surface。
