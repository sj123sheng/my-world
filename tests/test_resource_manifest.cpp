#include "../native/engine/resource/loader.h"
#include <cassert>
int main(){
  ResourceLoader r;
  ManifestEntry bad{"missing", 999, "x", {}};
  assert(!r.verify(bad));   // 不存在资源应失败
  return 0;
}
