---
kind: external_dependency
name: HarmonyOS 平台与 SDK
slug: harmonyos
category: external_dependency
category_hints:
    - vendor_identity
scope:
    - '**'
---

### HarmonyOS 平台
- 构建工具链：Hvigor + CMake + Ninja，使用 BiSheng 编译器
- 设备类型：phone、tablet
- 签名配置：通过 build-profile.json5 管理，调试签名不得提交到仓库
- 图形栈：EGL 1.4 + OpenGL ES 3.0，通过 libGLESv3.so 链接
- 原生桥接：N-API (libace_napi.z.so, libace_ndk.z.so)
- 窗口系统：NativeWindow + NativeBuffer
- 日志系统：HiLog NDK (libhilog_ndk.z.so)
- 真机验证：已在 Pura 70 Pro 模拟器完成性能验收
- 已知限制：模拟器 GLES 支持取决于 DevEco Studio、SDK 与系统镜像匹配情况