# Task 1 报告：可测试的 glTF 数据与动画核心

## 改动

- 原样供应商化 cgltf v1.14：`native/third_party/cgltf/cgltf.h`，保留其 MIT 许可头与完整许可文本。
- 新增纯数据 `skinned_model` 核心，不依赖 EGL/GLES：glTF 输入校验、动画时间循环、
  `vec3`/四元数 STEP 与 LINEAR 采样，以及父子节点递推的蒙皮调色板。
- 新增宿主机单元测试，覆盖循环、LINEAR、STEP、父子调色板、缺少属性、65 个关节和
  CUBICSPLINE 拒绝（含资产名）。

## RED

命令：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model
```

输出（预期失败）：

```text
clang++: error: no such file or directory: 'native/engine/render/skinned_model.cpp'
```

## GREEN 与自检

命令：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model
```

输出：无输出，退出码 0。

附加告警检查：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -Wall -Wextra -pedantic -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model_warnings && /tmp/test_skinned_model_warnings
```

输出：无输出，退出码 0；`git diff --check` 与暂存区检查均无空白错误。

## 提交

- `f113d65 feat: 添加 glTF 骨骼动画核心`
- 提交说明包含：`Prompt: M3-2 接入 glTF 模型加载与骨骼动画。`

## 疑虑

无；cgltf 从官方 `jkuhlmann/cgltf` v1.14 下载并逐字供应商化。

---

## 审查修复（P1/P2）

### 根因与改动

- `GltfValidationInput` 原先不能表达容器格式、图元模式、`_0` 属性集、第二组关节/权重，
  或每顶点影响数；现以枚举和显式字段表达，并逐项拒绝超出支持边界的输入。
- 所有 `ValidateGltf` 失败统一经资产名前缀生成；空资产名使用 `unnamed asset`，保证错误信息
  始终带有非空资产标识。
- `SampleVec3`/`SampleQuat` 在访问关键帧前拒绝 `times` 与 `values` 不等长通道，分别返回零向量
  与单位四元数。
- `BuildSkinPalette` 用未访问/访问中/已完成三态检测父子环，并把非法父索引和环安全地返回为空调色板。
- `tests/test_skinned_model.cpp` 增加 GLB、TRIANGLES、`TEXCOORD_0`、`JOINTS_1/WEIGHTS_1`、
  超四影响、非空错误名、不等长 vec3/quat 通道、父子环和非法父索引的回归覆盖。

### RED

命令：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model
```

输出（预期失败，新增 API 尚未实现）：

```text
tests/test_skinned_model.cpp:18:9: error: no member named 'assetFormat' in 'GltfValidationInput'
tests/test_skinned_model.cpp:18:23: error: use of undeclared identifier 'GltfAssetFormat'
tests/test_skinned_model.cpp:19:9: error: no member named 'primitiveMode' in 'GltfValidationInput'
tests/test_skinned_model.cpp:19:25: error: use of undeclared identifier 'GltfPrimitiveMode'
tests/test_skinned_model.cpp:22:9: error: no member named 'hasTexcoord0' in 'GltfValidationInput'
tests/test_skinned_model.cpp:25:9: error: no member named 'maxVertexInfluences' in 'GltfValidationInput'
16 errors generated.
```

### GREEN 与告警检查

命令：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model && /tmp/test_skinned_model
```

输出：无输出，退出码 0。

命令：

```sh
task_sdk=$(xcrun --show-sdk-path); xcrun clang++ -std=c++17 -Wall -Wextra -pedantic -isysroot "$task_sdk" -isystem "$task_sdk/usr/include/c++/v1" -I. -Inative -Inative/engine/math tests/test_skinned_model.cpp native/engine/render/skinned_model.cpp -o /tmp/test_skinned_model_warnings && /tmp/test_skinned_model_warnings
```

输出：无输出，退出码 0；`git diff --check` 无输出。

### 提交

- `66ee428 fix: 完善 glTF 骨骼动画输入校验`
- Prompt: 修复 M3-2 Task 1 审查发现的全部 P1/P2。

### 疑虑

无。
