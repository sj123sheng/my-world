# Lost Ruins 环境资产来源

本目录记录 Lost Ruins 环境批次的来源和派生关系。Poly Haven 资产采用
[CC0 1.0](https://polyhaven.com/license)；CC0 不要求署名，但本项目自愿保留来源证据。

## 源资产

- **Modular Fort 01** — 作者 Rico Cilliers；
  [资源页](https://polyhaven.com/a/modular_fort_01)；
  [Poly Haven 许可页](https://polyhaven.com/license)。
- **Rabdentse Ruins Wall** — 作者 Amal Kumar；
  [资源页](https://polyhaven.com/a/rabdentse_ruins_wall)；
  [Poly Haven 许可页](https://polyhaven.com/license)。

## 派生资产

以下四个 GLB 均使用 **Modular Fort 01** 的网格几何，并使用
**Rabdentse Ruins Wall** 的 2K 漫反射纹理及其确定性生成的 1K 纹理层：

- `outer_ring.glb`：外围墙、拱门、立柱和可行走环带。
- `center_rift.glb`：中央平台和石质框架；红色裂隙平面不在资产内，由运行时程序生成。
- `backdrop.glb`：不可到达的远景塔楼和墙体剪影。
- `decoration.glb`：瓦砾和不参与碰撞的小型细节。

精确下载 URL、下载日期、源文件 SHA-256、派生文件 SHA-256 和逐文件依赖记录在
`manifest.json`。
