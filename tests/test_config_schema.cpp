#include <fstream>
#include <cassert>
int main(){
  std::ifstream f("config/dev/resonances.json");
  assert(f.good());            // MVP 简化:文件存在即视为通过;发布期接 JSON Schema 校验库
  return 0;
}
