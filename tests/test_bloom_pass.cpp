// test_bloom_pass.cpp: bloom 后处理纯函数参数断言。

#include "native/engine/render/bloom_pass.h"

#include <cassert>
#include <cmath>

namespace {

bool nearlyEqual(float left, float right, float epsilon = 0.0001f) {
  return std::fabs(left - right) < epsilon;
}

void testBloomParamsQualityGating() {
  // 高画质启用 bloom：阈值/强度/迭代次数均在有效区间。
  const BloomParams high = BloomParamsFor(0);
  assert(BloomEnabled(high));
  assert(high.threshold > 0.0f && high.threshold < 1.0f);
  assert(high.intensity > 0.0f);
  assert(high.blurIterations >= 1);
  // 低画质整体关闭：intensity=0 且迭代次数为 0。
  const BloomParams low = BloomParamsFor(1);
  assert(!BloomEnabled(low));
  assert(low.intensity == 0.0f);
  assert(low.blurIterations == 0);
}

void testBloomDownsampleSizeHalves() {
  assert(BloomDownsampleSize(1080) == 540);
  assert(BloomDownsampleSize(1920) == 960);
  assert(BloomDownsampleSize(3) == 1);
  // 极小尺寸不下探到 0，避免 0 尺寸纹理。
  assert(BloomDownsampleSize(2) == 1);
  assert(BloomDownsampleSize(1) == 1);
  assert(BloomDownsampleSize(0) == 1);
}

void testBloomGaussianWeightsSymmetricAndNormalized() {
  // 权重全部为正且单调递减（中心最亮）。
  float previous = 1.0f;
  for (int tap = 0; tap <= 4; ++tap) {
    const float weight = BloomGaussianWeight(tap);
    assert(weight > 0.0f);
    assert(weight < previous);
    previous = weight;
  }
  // 对称加权和约等于 1（中心 + 两侧各 4 tap），blur 不整体增亮。
  float sum = BloomGaussianWeight(0);
  for (int tap = 1; tap <= 4; ++tap) sum += 2.0f * BloomGaussianWeight(tap);
  assert(nearlyEqual(sum, 1.0f, 0.01f));
  // 越界 tap 返回 0，不着色器越界采样。
  assert(BloomGaussianWeight(5) == 0.0f);
  assert(BloomGaussianWeight(-1) == 0.0f);
}

}  // namespace

int main() {
  testBloomParamsQualityGating();
  testBloomDownsampleSizeHalves();
  testBloomGaussianWeightsSymmetricAndNormalized();
  return 0;
}
