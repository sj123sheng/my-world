#include "loader.h"
#include <fstream>
bool ResourceLoader::loadManifest(const char* path){
  // MVP 简化:读取 JSON manifest 文件并解析 entry 列表
  // 发布期使用 JSON 解析库;此处留接口
  std::ifstream f(path);
  return f.good();
}
bool ResourceLoader::verify(const ManifestEntry& e){
  std::ifstream f(e.id, std::ios::binary);
  if(!f) return false;
  f.seekg(0, std::ios::end);
  size_t sz = (size_t)f.tellg();
  return sz == e.size; // MVP 简化:仅校验大小;发布期补哈希
}
