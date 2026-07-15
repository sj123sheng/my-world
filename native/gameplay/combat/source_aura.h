#pragma once
#include "event.h"
struct SourceAuraContainer {
  void apply(const SourceAura& a);
  void decay(Tick now);
  bool consume(SourceType type);
  void clear() { auras_.clear(); }
  const std::vector<SourceAura>& active() const { return auras_; }
private: std::vector<SourceAura> auras_;
};
