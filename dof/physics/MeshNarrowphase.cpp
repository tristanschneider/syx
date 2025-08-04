#include <MeshNarrowphase.h>

#include <BoxBox.h>
#include <math/Geometric.h>
#include <generics/IntMath.h>
#include <shapes/ShapeRegistry.h>
#include <SpatialPairsStorage.h>

namespace Narrowphase {
  struct SupportPoint {
    float minDistance{};
    uint32_t index{};
  };

  struct FurthestEdge {
    uint32_t referenceEdge{};
    uint32_t incidentPoint{};
    //Unit length normal of the chosen reference edge in world space
    glm::vec2 normalWorld{};
    //World space distance between reference and incident. Positive if separating
    float distanceAlongNormal{};
  };

  //Get the point furthest in the opposite direction on the mesh. Direction should be transformed into the mesh's space
  SupportPoint getSupportPoint(const glm::vec2& direction, const ShapeRegistry::Mesh& mesh) {
    SupportPoint result{ .minDistance = std::numeric_limits<float>::max() };
    for(size_t i = 0; i < mesh.points.size(); ++i) {
      const float currentDistance = glm::dot(direction, mesh.points[i]);
      if(currentDistance < result.minDistance) {
        result.minDistance = currentDistance;
        result.index = i;
      }
    }

    return result;
  }

  FurthestEdge findFurthestEdgeOnA(const ShapeRegistry::Mesh& a, const ShapeRegistry::Mesh& b, const MeshOptions& ops) {
    FurthestEdge result{ .distanceAlongNormal = std::numeric_limits<float>::lowest() };

    //Transform edges on A into space B to search for closest along the edge normals in B space
    const Transform::PackedTransform aToB = b.worldToModel * a.modelToWorld;
    const size_t aPointCount = a.points.size();
    //All the points are wound around the center in counterclockwise order.
    //Subtracting two neighboring points produces an edge on the boundary.
    glm::vec2 beginInB = aToB.transformPoint(a.points[aPointCount - 1]);
    for(size_t e = 0; e < aPointCount; ++e) {
      glm::vec2 endInB = aToB.transformPoint(a.points[e]);
      //Cross edge with z, since edge is counterclockwise that's the outward facing normal.
      glm::vec2 normalInB = Geo::orthogonal(endInB - beginInB);

      const float length = glm::length(normalInB);
      //If the normal is nonsense count this is no-collision with this edge
      if(length > ops.edgeEpsilon) {
        //Resulting distance must be computed against a normalized normal because the distances
        //of different normals will be compared with each-other for the overall best.
        normalInB /= length;

        const SupportPoint supportB = getSupportPoint(normalInB, b);
        //Either begin or end are valid support points, as they are on the boundary of the shape
        const float supportA = glm::dot(normalInB, endInB);
        //Since the supports are projected onto an outward facing normal, the distance is positive (separating) if B > A
        const float aToBDistance = supportB.minDistance - supportA;
        if(aToBDistance > result.distanceAlongNormal) {
          result.distanceAlongNormal = aToBDistance;
          result.referenceEdge = e;
          result.incidentPoint = supportB.index;
          //Store the local normal here while searching, it'll be transformed at the end if it's the best.
          result.normalWorld = normalInB;
        }
      }

      beginInB = endInB;
    }

    //Take the unit length normal, and scale it to the length of a to b
    result.normalWorld *= std::abs(result.distanceAlongNormal);
    //Transform to world. This could change the length of the vector (distance between points) if either object has non-unit scale
    result.normalWorld = b.modelToWorld.transformVector(result.normalWorld);
    //Normalize and compute world space distance, preserving sign so caller knows if it's separating or not
    const float normalLength = glm::length(result.normalWorld);
    result.distanceAlongNormal = Geo::makeSameSign(normalLength, result.distanceAlongNormal);
    //Normalize or arbitrary vector. Zero shouldn't be possible as edge epsilon should prevent selecting an edge with a zero normal.
    //Still better to choose an arbitrary direction to try to resolve than to divide by zero or miss the collision entirely.
    result.normalWorld = Geo::divideOr(result.normalWorld, normalLength, glm::vec2{ 1, 0 });

    return result;
  }

  //Reconstructs the reference edge similar to how it was when traversing in findFurthestEdgeOnA
  Geo::LineSegment getReferenceSegment(uint32_t index, const ShapeRegistry::Mesh& mesh) {
    return Geo::LineSegment{
      .start = mesh.modelToWorld.transformPoint(mesh.points[index]),
      .end = mesh.modelToWorld.transformPoint(mesh.points[gnx::IntMath::wrappedDecrement(index, gnx::IntMath::Nonzero{ static_cast<uint32_t>(mesh.points.size()) })]),
    };
  }

  //Computes incident edge from incident point, finding the connected edge that is more orthogonal to the reference edge normal
  Geo::LineSegment getIncidentSegment(uint32_t index, const glm::vec2& referenceNormal, const ShapeRegistry::Mesh& mesh) {
    const gnx::IntMath::Nonzero size{ static_cast<uint32_t>(mesh.points.size()) };
    const glm::vec2 root = mesh.modelToWorld.transformPoint(mesh.points[index]);
    const glm::vec2 a = mesh.modelToWorld.transformPoint(mesh.points[gnx::IntMath::wrappedIncrement(index, size)]);
    const glm::vec2 b = mesh.modelToWorld.transformPoint(mesh.points[gnx::IntMath::wrappedDecrement(index, size)]);
    const glm::vec2 edgeA = glm::normalize(root - a);
    const glm::vec2 edgeB = glm::normalize(root - b);
    return std::abs(glm::dot(referenceNormal, edgeA)) < std::abs(glm::dot(referenceNormal, edgeB))
      ? Geo::LineSegment{ .start = a, .end = root }
      : Geo::LineSegment{ .start = b, .end = root };
  }

  void generateContactsConvex(
    const ShapeRegistry::Mesh& a,
    const ShapeRegistry::Mesh& b,
    const MeshOptions& ops,
    SP::ContactManifold& result
  ) {
    //Can't collide with an empty mesh
    if(a.points.empty() || b.points.empty()) {
      return;
    }

    //Find the most-separating edges relative to the normals on both bodies
    const FurthestEdge bestA = findFurthestEdgeOnA(a, b, ops);
    const FurthestEdge bestB = findFurthestEdgeOnA(b, a, ops);
    //Figure out which has the better edge, that'll be the reference
    Geo::LineSegment reference, incident;
    bool isOnA{};
    glm::vec2 normal{};
    //Bias towards one arbitrarily to avoid numerical instability flipping between near-equivalent results.
    if(bestA.distanceAlongNormal + ops.edgeEpsilon > bestB.distanceAlongNormal) {
      normal = bestA.normalWorld;
      reference = getReferenceSegment(bestA.referenceEdge, a);
      incident = getIncidentSegment(bestA.incidentPoint, normal, b);
      isOnA = true;
    }
    else {
      normal = bestB.normalWorld;
      reference = getReferenceSegment(bestB.referenceEdge, b);
      incident = getIncidentSegment(bestB.incidentPoint, normal, a);
      isOnA = false;
    }

    //Clip incident edge against reference and store in manifold
    Narrowphase::storeResult(Narrowphase::clipEdgeToEdge(normal, reference, incident), normal, a.modelToWorld.pos2(), b.modelToWorld.pos2(), isOnA, result);

    //TODO: adjust contacts for mesh radius
  }
}