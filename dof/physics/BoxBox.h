#pragma once

#include <glm/vec2.hpp>
#include "Geometric.h"

namespace SP {
  struct ContactManifold;
};

namespace Narrowphase {
  struct BoxPairElement {
    glm::vec2 pos{};
    glm::vec2 rot{};
    //Assuming half-scale
    glm::vec2 scale{};
  };
  struct BoxPair {
    BoxPairElement a, b;
  };
  struct ClipResult {
    Geo::LineSegment edge;
    Geo::Range1D overlap;
  };
  struct SeparatingAxis {
    float overlap{};
    uint8_t direction{};
    uint8_t supportA{};
    uint8_t supportB{};
    uint8_t axis{};
  };

  SeparatingAxis getLeastOverlappingAxis(const BoxPair& pair);
  ClipResult clipEdgeToEdge(const glm::vec2& normal, const Geo::LineSegment& reference, const Geo::LineSegment& incident);

  void boxBox(SP::ContactManifold& manifold, const BoxPair& pair);
};