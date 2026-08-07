#pragma once

// bloom_pass.h: bloom 后处理（原神式技能发光）的纯函数参数决策。
//
// 与 combat_vfx.h 同样的约定：阈值/强度/降采样/卷积权重都是纯函数，
// Surface 只负责 FBO 资源与 pass 编排，行为由 tests/test_bloom_pass.cpp
// 断言锁定。bloom 让加法混合的刀光/冲击波/火花/光环产生溢出光晕，
// 是技能释放效果向原神靠拢的关键一步。

struct BloomParams {
  float threshold = 0.62f;  // 亮度阈值（软膝），低于此不参与发光
  float intensity = 0.85f;  // 合成时 bloom 叠加强度
  int blurIterations = 2;   // 半分辨率 ping-pong 迭代次数
};

// 画质分档：高画质（qualityPreset=0）启用 bloom；低画质返回
// intensity=0/blurIterations=0，调用方据此整体跳过 bloom 管线。
inline BloomParams BloomParamsFor(int qualityPreset) {
  if (qualityPreset != 0) return {0.0f, 0.0f, 0};
  return {0.62f, 0.85f, 2};
}

inline bool BloomEnabled(const BloomParams& params) {
  return params.intensity > 0.0f && params.blurIterations > 0;
}

// 半分辨率降采样尺寸：blur pass 在 1/2 分辨率上做，至少 1x1。
inline int BloomDownsampleSize(int size) {
  return size > 2 ? size / 2 : 1;
}

// 9-tap 对称高斯卷积权重（tap=0 中心，tap=1..4 两侧），
// 与着色器内常量一致；权重和约等于 1，保证 blur 不整体增亮。
inline float BloomGaussianWeight(int tap) {
  switch (tap) {
    case 0: return 0.227027f;
    case 1: return 0.1945946f;
    case 2: return 0.1216216f;
    case 3: return 0.054054f;
    case 4: return 0.016216f;
    default: return 0.0f;
  }
}
