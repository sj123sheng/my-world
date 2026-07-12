#pragma once
#include "event.h"
struct SourceAuraContainer {
  void apply(const SourceAura& a);
  void decay(Tick now);
  const std::vector<SourceAura>& active() const { return auras_; }
private: std::vector<SourceAura> auras_;
};
