---
kind: external_dependency
name: Poly Haven 3D 资产库
slug: poly-haven
category: external_dependency
category_hints:
    - vendor_identity
scope:
    - '**'
---

### Poly Haven 资源来源
- 许可证：CC0 1.0（完全免费，无署名要求）
- 使用资产：Modular Fort 01（作者 Rico Cilliers）、Rabdentse Ruins Wall（作者 Amal Kumar）
- 资产格式：GLTF/GLB 模型文件，包含纹理贴图（diffuse、normal、armature）
- 版本管理：通过 assets/environment/manifest.json 记录源文件哈希和下载信息
- 派生资产：项目将原始 GLTF 转换为四个优化后的 GLB 文件（outer_ring、center_rift、backdrop、decoration）
- 完整性校验：所有源文件和派生文件均记录 SHA-256 哈希值用于完整性验证
- 下载地址：https://dl.polyhaven.org/file/ph-assets/Models/gltf/ 和 https://dl.polyhaven.org/file/ph-assets/Textures/gltf/