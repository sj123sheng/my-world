#pragma once
#include <cstdint>
using Tick = int64_t;
using FixedPoint = int64_t; // 1.0 == 1<<16, 定点数
constexpr FixedPoint FP_ONE = 1 << 16;
inline FixedPoint fp(double v){ return static_cast<FixedPoint>(v * FP_ONE); }
