# M4 Task 2 / Task 1 实施报告

## 实现内容

- 通过 Poly Haven 公共 `files/{assetId}` API 获取
  `modular_fort_01` 与 `rabdentse_ruins_wall` 的真实下载索引。
- 确定性选择两个资源的 2K glTF 下载，所有源文件和依赖只写入系统临时目录，
  无源包进入 Git；脚本结束时清理临时目录。
- 实现并导出简报要求的全部公共函数：
  `fetchPolyHavenFileIndex`、`chooseGltfDownload`、`sha256`、`readGlb`、
  `bakeNodeTransforms`、`partitionNodes`、`mergePrimitivesByMaterial`、
  `embedTextureLevels`、`writeGlb`。
- `layout.json` 使用真实源节点名、唯一 placement ID、显式 TRS 四元数和四个合法区域；
  获取脚本校验未知区域、重复 ID、非有限变换和不存在的源节点。
- 烘焙完整节点矩阵到 POSITION，以逆转置 3×3 矩阵处理并归一化 NORMAL；
  按源材质名合并各区域 primitive，重建并重基索引。
- 每个派生 GLB 嵌入 Rabdentse 2K JPEG 漫反射图，以及通过固定 `sips`
  参数生成的 1K PNG；图片记录命名为 `diffuse_full`、`diffuse_half`。
- GLB 写入使用递归排序 JSON key、无多余空白、JSON 空格填充和 BIN 零填充；
  转换后拒绝任意外部 buffer/image URI。
- 生成四个 GLB 2.0 批次、来源/派生 SHA-256 manifest 和 CC0 来源说明。
- 增加清单契约测试，以及内存 GLB 对全部转换接口的聚焦测试。

## 文件清单

- `.superpowers/sdd/task-1-report.md`
- `automation/assets/fetch_environment_assets.mjs`
- `automation/assets/validate_environment_assets.mjs`
- `assets/environment/manifest.json`
- `assets/environment/LICENSES.md`
- `assets/environment/layout.json`
- `entry/src/main/resources/rawfile/environment/outer_ring.glb`
- `entry/src/main/resources/rawfile/environment/center_rift.glb`
- `entry/src/main/resources/rawfile/environment/backdrop.glb`
- `entry/src/main/resources/rawfile/environment/decoration.glb`
- `tests/test_environment_assets.mjs`

## RED

命令：

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_environment_assets.mjs
```

预期且实际的首次失败：

```text
Error: ENOENT: no such file or directory, open
'/Users/xiling/Documents/project/game/my-world/.worktrees/m4-task2-environment-impl/assets/environment/manifest.json'
code: 'ENOENT'
exit code: 1
```

补充转换接口测试后，在创建实现前再次运行，得到预期失败：

```text
Error [ERR_MODULE_NOT_FOUND]: Cannot find module
'.../automation/assets/fetch_environment_assets.mjs'
exit code: 1
```

实现后的聚焦测试还发现单位矩阵第三个对角元素位置错误；断言显示实际矩阵与标准
4×4 单位矩阵不同。修正常量后，接口测试通过并仅剩 manifest 未生成的预期 ENOENT。

## GREEN

最终按简报原样运行：

```bash
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  automation/assets/fetch_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  automation/assets/validate_environment_assets.mjs
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
  tests/test_environment_assets.mjs
```

结果：

```text
fetch_environment_assets.mjs: exit code 0
validate_environment_assets.mjs: exit code 0
test_environment_assets.mjs: exit code 0
stdout/stderr: empty
```

脚本完整重复运行后四个 SHA-256 保持逐字节一致：

```text
55376fc70f8dc2f354c2edcdce242d14d0edf11f98de07a9d42f6285e7b410cc  backdrop.glb
600703fc588eff24301031df0f99460316acc944bd54155c58169687635b07ae  center_rift.glb
2c1bf9f689a0fa2452f1a2c7c67e4eccea5fbb91e32105165b3c7e481ae99879  decoration.glb
ee8df4e5ee951a7f28fb86dd274f19394b153e2e2231a66d86fd5d53c68e4d14  outer_ring.glb
```

附加验证：

```text
node --check automation/assets/fetch_environment_assets.mjs: exit code 0
node --check automation/assets/validate_environment_assets.mjs: exit code 0
git diff --check: exit code 0
```

## 自审

- 逐项重读简报，未修改 C++、渲染代码或计划文档。
- 两个 source asset 的 ID、作者、资源页、下载 URL、下载日期和 SHA-256 均已登记。
- 四个 derived asset 均声明两个精确 source dependency，文件哈希与 manifest 一致。
- 四个输出均为 GLB 2.0，每个含一个 JSON 和一个 BIN chunk。
- 四个输出均无外部 URI；`diffuse_full` 为嵌入 JPEG，
  `diffuse_half` 为嵌入 PNG，二者均由 bufferView 承载。
- 每个区域只保留一个合并 mesh，primitive 材质边界仍保留，材质按源名称排序。
- placement 和材质顺序、JSON key、纹理编码参数、chunk 填充均固定；
  多次完整获取/派生的哈希一致。
- 工作区改动仅包含任务简报要求的实现、资产、测试和本报告。

## 问题 / 担忧

- 无阻塞问题。
- 1K PNG 派生依赖 macOS `/usr/bin/sips`；当前 DevEco Studio/HarmonyOS 开发环境为
  macOS，已用固定尺寸、格式和 formatOptions 验证多次输出一致。若未来在非 macOS
  CI 重新派生，需要提供等价的固定 PNG 编码器。
- manifest 的 source SHA-256 对应所登记 downloadUrl 的主 glTF 文件；
  下载依赖只在临时目录中用于派生，不单独作为 sourceAssets 条目登记。
