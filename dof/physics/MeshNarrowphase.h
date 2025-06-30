#pragma once

namespace SP {
  struct ContactManifold;
}
namespace ShapeRegistry {
  struct Mesh;
}

namespace Narrowphase {
  struct MeshOptions {
    //Model space threshold to ignore edge length as being too small.
    float edgeEpsilon{ 0.00001f };
    //Distance over which contacts will not be generated and pair ignored as not colliding.
    //Use positive values to get the nearest point when objects are close.
    //Use zero to maximize early-out potential for speed at the cost of stability.
    float noCollisionDistance{ 0.f };
  };

  void generateContactsConvex(const ShapeRegistry::Mesh& a, const ShapeRegistry::Mesh& b, const MeshOptions& ops, SP::ContactManifold& result);
}