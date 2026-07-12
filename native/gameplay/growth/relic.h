#pragma once
#include <string>
struct Character;
struct Relic { std::string id; std::string modifiesAbility; float mult;
  void apply(Character& c) const; };
