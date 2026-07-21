# 角色模型资产许可清单

下载日期：2026-07-20（Asia/Shanghai）

## KayKit Adventurers Character Pack 1.0

- 作者：Kay Lousberg（KayKit）
- 资产包版本：1.0
- 资产包来源：https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0
- 资产包许可证文件：https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/blob/main/LICENSE.txt
- 许可证：Creative Commons Zero v1.0 Universal（CC0-1.0）
- 许可证 URL：https://creativecommons.org/publicdomain/zero/1.0/
- 使用范围：原许可文件明确允许个人、教育及商业项目使用，且不强制署名。

### 项目内文件

| 项目文件 | 上游角色 | 上游文件 URL | SHA-256 |
| --- | --- | --- | --- |
| `entry/src/main/resources/rawfile/models/player.glb` | Knight | https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/blob/main/addons/kaykit_character_pack_adventures/Characters/gltf/Knight.glb | `ed721763df8d2fcbe8cfd9f6a16da863750ccc50d6979a231eb09e38250c8f21` |
| `entry/src/main/resources/rawfile/models/enemy.glb` | Mage | https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/blob/main/addons/kaykit_character_pack_adventures/Characters/gltf/Mage.glb | `ba1577e3602c93a9a862e7faacb63cf0bf9cf7ee579dd1a884fcf7551242fee1` |
| `entry/src/main/resources/rawfile/models/boss.glb` | Barbarian | https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/blob/main/addons/kaykit_character_pack_adventures/Characters/gltf/Barbarian.glb | `e39aeb8917e9ff166271cddc5adaa44479c3783a76dc41eb1fad957bb787ca8d` |

### 项目适配

上游文件本身已是 GLB 2.0，模型、骨骼、关键帧和内嵌纹理数据均保持不变。为匹配项目
`idle/run/attack/hit/death` 动作约定，仅修改每份 GLB JSON chunk 中五个已有动作的名称：

| 上游动作名 | 项目动作名 |
| --- | --- |
| `Idle` | `idle` |
| `Running_A` | `run` |
| `1H_Melee_Attack_Chop` | `attack` |
| `Hit_A` | `hit` |
| `Death_A` | `death` |

项目的真实 `SkinnedModel` 解析审计确认三份文件各有 41 个关节、6 个骨骼图元、76 个
动画 clip 和 1 张内嵌纹理；均未使用外部 URI，且通过最多 64 关节、每顶点最多 4 个影响
及项目已确认 glTF 子集的校验。
