#pragma once
#include <string>
struct SaveState { int campLevel; int relics; int regionProgress; };
struct Save { bool write(const SaveState& s, const char* path); bool read(SaveState& out, const char* path); };
