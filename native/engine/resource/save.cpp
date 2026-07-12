#include "save.h"
#include <fstream>
bool Save::write(const SaveState& s, const char* path){
  std::ofstream tmp(std::string(path)+".tmp");
  tmp << s.campLevel << " " << s.relics << " " << s.regionProgress << "\n";
  tmp.flush();
  std::rename((std::string(path)+".tmp").c_str(), path); // 原子替换
  return true;
}
bool Save::read(SaveState& o, const char* path){
  std::ifstream f(path); if(!f) return false;
  f >> o.campLevel >> o.relics >> o.regionProgress; return !f.fail();
}
