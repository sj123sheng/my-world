// texture.h: PNG 纹理加载接口。
//
// loadTexture 用 stb_image 加载 PNG 文件，返回 GL 纹理句柄，失败返回 0。
// 在 HarmonyOS 平台执行真实 GL 上传，非平台侧为空操作（返回 0），便于单元测试。

#pragma once

// 返回加载得到的 GL 纹理句柄；路径不存在或加载失败时返回 0。
// 非 HarmonyOS 平台始终返回 0。
unsigned int loadTexture(const char* path);
