#include "native/engine/render/mesh.h"

#include <cassert>
#include <cmath>

int main() {
  Mesh ring = createRing(0.42f, 0.055f, 24);
  assert(ring.vertices.size() == 96);
  assert(ring.indices.size() == 144);
  for (const Vertex& vertex : ring.vertices) {
    const float radius = std::sqrt(vertex.position.x * vertex.position.x +
                                   vertex.position.z * vertex.position.z);
    assert(radius > 0.35f && radius < 0.50f);
    assert(vertex.position.y == 0.0f);
  }
  return 0;
}
