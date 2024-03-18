#include "Precompile.h"
#include "BoxBox.h"

#include "Clip.h"
#include "Geometric.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"

namespace Narrowphase {
  struct SupportPoint {
    Geo::Range1D minMax;
    uint8_t index{};
  };
  struct SupportPair {
    SupportPoint a, b;
  };
  constexpr uint8_t POSITIVE = 1;
  constexpr uint8_t NEGATIVE = 2;
  struct AxisProjection {
    SeparatingAxis toSeparatingAxis(uint8_t axis) const {
      return { overlap, direction, supportA, supportB, axis };
    }

    float overlap{};
    uint8_t direction{};
    uint8_t supportA{};
    uint8_t supportB{};
  };
  constexpr int AXIS_COUNT = 4;
  constexpr int AXIS_STRIDE = 2;
  constexpr glm::vec2 getSupportAxis(int index, const BoxPair& pair) {
    switch(index) {
      case 0: return pair.a.rot;
      case 1: return Geo::orthogonal(pair.a.rot);
      case 2: return pair.b.rot;
      case 3: return Geo::orthogonal(pair.b.rot);
    }
    assert(false);
    return {};
  }

  //Get the edge where the normal of getSupportAxis came from
  Geo::LineSegment getReferenceEdge(uint8_t axisIndex, uint8_t direction, const BoxPair& pair) {
    glm::vec2 primary, secondary, pos;
    switch(axisIndex) {
    case 0:
      primary = pair.a.rot*pair.a.scale.x;
      secondary = Geo::orthogonal(pair.a.rot)*pair.a.scale.y;
      pos = pair.a.pos;
      break;
    case 1:
      secondary = pair.a.rot*pair.a.scale.x;
      primary = Geo::orthogonal(pair.a.rot)*pair.a.scale.y;
      pos = pair.a.pos;
      break;
    case 2:
      primary = pair.b.rot*pair.b.scale.x;
      secondary = Geo::orthogonal(pair.b.rot)*pair.b.scale.y;
      pos = pair.b.pos;
      break;
    case 3:
      secondary = pair.b.rot*pair.b.scale.x;
      primary = Geo::orthogonal(pair.b.rot)*pair.b.scale.y;
      pos = pair.b.pos;
      break;
    }
    const glm::vec2 base = direction == POSITIVE ? pos + primary : pos - primary;
    return { base + secondary, base - secondary };
  }

  //Get the edge connected to supportIndex point that is most orthogonal to the normal, meaning
  //the edge is more parallel with the reference edge
  Geo::LineSegment getIncidentEdge(uint8_t supportIndex, uint8_t direction, const BoxPairElement& element, const glm::vec2& normal) {
    const glm::vec2 l = element.rot*element.scale.x;
    const glm::vec2 u = Geo::orthogonal(element.rot)*element.scale.y;
    //Direction is the one most orthogonal
    const float projL = glm::dot(element.rot, normal);
    const float projR = glm::dot(Geo::orthogonal(element.rot), normal);
    glm::vec2 supportDir = std::abs(projL) < std::abs(projR) ? l : u;
    glm::vec2 support{};
    //Based on the indices from getSupport
    switch(supportIndex) {
      case 1: support = l + u; break;
      case 2: support = l - u; break;
      case 3: support = u - l; break;
      case 4: support = -u - l; break;
    }
    if(direction == POSITIVE) {
      support += element.pos;
    }
    else {
      support -= element.pos;
    }
    //Figure out which way the support goes from here so it goes to the other corner instead of out of the shape
    if(glm::dot(element.pos - support, supportDir) < 0) {
      supportDir = -supportDir;
    }
    return { support, support + supportDir*2.0f };
  }

  constexpr SupportPoint getSupport(const glm::vec2& axis, const BoxPairElement& element) {
    const glm::vec2 l = element.rot*element.scale.x;
    const glm::vec2 u = Geo::orthogonal(element.rot)*element.scale.y;
    const float ldot = glm::dot(l, axis);
    const float udot = glm::dot(u, axis);
    const float posDot = glm::dot(element.pos, axis);
    float supportMax{};
    uint8_t index{};
    if(ldot > 0) {
      if(udot > 0) {
        supportMax = ldot + udot;
        index = 1;
      }
      else {
        supportMax = ldot - udot;
        index = 2;
      }
    }
    else {
      if(udot > 0) {
        supportMax = -ldot + udot;
        index = 3;
      }
      else {
        supportMax = -ldot - udot;
        index = 4;
      }
    }
    return { Geo::Range1D{ posDot - supportMax, posDot + supportMax }, index };
  }

  AxisProjection classifyAxis(const SupportPair& pair) {
    //Min and max represents support points along the axis in the positive and negative direction
    const float positiveOverlap = pair.a.minMax.max - pair.b.minMax.min;
    const float negativeOverlap = -(pair.a.minMax.min - pair.b.minMax.max);
    if(positiveOverlap < negativeOverlap) {
      return { positiveOverlap, POSITIVE, pair.a.index, pair.b.index };
    }
    return { negativeOverlap, NEGATIVE, pair.a.index, pair.b.index };
  }

  constexpr SupportPair getSupport(int index, const BoxPair& pair) {
    const glm::vec2 axis = getSupportAxis(index, pair);
    return { getSupport(axis, pair.a), getSupport(axis, pair.b) };
  }

  Geo::LineSegment clipIncidentToSides(const Clip::StartAndDir& edge, const Geo::LineSegment& incident) {
    const glm::vec2 leftNormal = -edge.dir;
    const float lProj = glm::dot(edge.start, leftNormal);
    const float rProj = glm::dot(edge.start + edge.dir, leftNormal);
    const float tProj = glm::dot(incident.end, leftNormal);
    const float bProj = glm::dot(incident.start, leftNormal);
    glm::vec2 farPoint = incident.end;
    float farProj = tProj;
    glm::vec2 nearPoint = incident.start;
    float nearProj = bProj;
    if(tProj < bProj) {
      std::swap(farPoint, nearPoint);
      std::swap(farProj, nearProj);
    }

    //Far point is outside of left edge and presumably near is inside, as all axes should be overlapping
    if(farProj > lProj) {
      //Clip far to left normal
      if(const float len = farProj - nearProj; len > Geo::EPSILON) {
        const float t = (farProj - lProj)/len;
        farPoint = farPoint + t*(nearPoint - farPoint);
        farProj = lProj;
      }
    }

    //Near point is outside right edge and presumably far is inside
    if(nearProj < rProj) {
      //Clip near to the right normal
      if(const float len = farProj - nearProj; len > Geo::EPSILON) {
        const float t = (rProj - nearProj)/len;
        nearPoint = nearPoint + t*(farPoint - nearPoint);
      }
    }
    return { farPoint, nearPoint };
  }

  //Clamp point to edge by moving along edge direction
  //This means if the point is not on the edge its distance to the edge will remain the same
  //but it will not go past the beginning or end of the edge
  glm::vec2 clampToEdge(const Clip::StartAndDir& edge, const glm::vec2& point) {
    const glm::vec2 offStart = point - edge.start;
    const float edgeLen2 = glm::dot(edge.dir, edge.dir);
    //Shouldn't happen for boxes of nonzero size
    if(edgeLen2 < Geo::EPSILON) {
      return point;
    }
    const float proj = glm::dot(offStart, edge.dir)/edgeLen2;
    //Point is past the end of the edge, bring it back to the edge
    if(proj > 1.0f) {
      return point - edge.dir*(proj - 1.0f);
    }
    //Point is before the beginning of the edge, bring it back to the beginning
    if(proj < 0.0f) {
      return point - edge.dir*proj;
    }
    //Point is within edge, good as-is
    return point;
  }

  ClipResult getIntersectAndInside(
    float t,
    const Clip::StartAndDir& ref,
    const Geo::LineSegment& incident,
    const glm::vec2& normal
  ) {
    //Within reference and incident, use point inside and intersect
    const glm::vec2 intersect = ref.start + ref.dir*t;
    //Pick the point with a negative dot which would be inside
    const glm::vec2 inside = glm::dot(incident.start - ref.start, normal) > 0 ? incident.end : incident.start;
    //Clamping the intersect is not necessary if it is certain to be within reference already
    ClipResult result;
    result.edge = clipIncidentToSides(ref, { inside, intersect });
    result.overlap.min = -glm::dot(result.edge.start - ref.start, normal);
    result.overlap.max = -glm::dot(result.edge.end - ref.start, normal);
    return result;
  }

  ClipResult getIncident(
    const Clip::StartAndDir& ref,
    const Geo::LineSegment& incident,
    const glm::vec2& normal
  ) {
    ClipResult result;
    result.edge = clipIncidentToSides(ref, incident);
    result.overlap.min = -glm::dot(result.edge.start - ref.start, normal);
    result.overlap.max = -glm::dot(result.edge.end - ref.start, normal);
    return result;
  }

  ClipResult clipEdgeToEdge(const glm::vec2& normal, const Geo::LineSegment& reference, const Geo::LineSegment& incident) {
    const Clip::StartAndDir ref = Clip::StartAndDir::fromStartEnd(reference.start, reference.end);
    const Clip::StartAndDir inc = Clip::StartAndDir::fromStartEnd(incident.start, incident.end);
    const auto times = Clip::getIntersectTimes(ref, inc);
    //Edge and incident are parallel. Since it's a collision, assume the incident is inside
    if(!times.tA) {
      return getIncident(ref, incident, normal);
    }
    //If incident edge intersects within reference line use the intersect and the corner inside the reference
    const bool withinReferenceSegment = Geo::between(*times.tA, { 0, 1 });
    const bool withinIncidentSegment = Geo::between(*times.tB, { 0, 1 });
    if(withinReferenceSegment) {
      if(withinIncidentSegment) {
        //Within reference and incident, use point inside and intersect
        return getIntersectAndInside(*times.tA, ref, incident, normal);
      }
      //Within reference but not incident
      //Incident is either entirely outside or entirely inside reference
      //Presumably inside since this is a collision
      //Use both inside points
      return getIncident(ref, incident, normal);
    }
    if(withinIncidentSegment) {
      //Within incident but not reference
      //Likely incident is very far overlapping or almost parallel to reference
      //as it intersects eventually but not within the length of the reference
      //Either way it's the same as incident entirely inside case, clamp to sides
      return getIncident(ref, incident, normal);
    }
    //Within neither, incident is entierly outside or entirely inside
    //Assume inside given that this is a collision
    return getIncident(ref, incident, normal);
  }

  SeparatingAxis getLeastOverlappingAxis(const BoxPair& pair) {
    std::array<AxisProjection, AXIS_COUNT> projections;
    uint8_t bestAxis{};
    float bestOverlap = std::numeric_limits<float>::max();
    //Get projections along each axis to find the least overlapping axis and exit if separation is found
    for(int i = 0; i < AXIS_COUNT; ++i) {
      const AxisProjection& proj = projections[i] = classifyAxis(getSupport(i, pair));
      //Negative overlap would be separation, exit
      if(proj.overlap < 0) {
        return proj.toSeparatingAxis(static_cast<uint8_t>(i));
      }
      if(proj.overlap < bestOverlap) {
        bestOverlap = proj.overlap;
        bestAxis = static_cast<uint8_t>(i);
      }
    }
    return projections[bestAxis].toSeparatingAxis(static_cast<uint8_t>(bestAxis));
  }

  void boxBox(SP::ContactManifold& manifold, const BoxPair& pair) {
    const SeparatingAxis bestAxis = getLeastOverlappingAxis(pair);
    if(bestAxis.overlap < 0) {
      return;
    }

    //If we made it here there is a collision and the separating axis is stored by bestAxis
    const glm::vec2 normal = bestAxis.direction == POSITIVE ? getSupportAxis(bestAxis.axis, pair) : -getSupportAxis(bestAxis.axis, pair);
    const bool isOnA = bestAxis.axis < AXIS_STRIDE;
    Geo::LineSegment reference, incident;
    if(isOnA) {
      reference = getReferenceEdge(bestAxis.axis, bestAxis.direction, pair);
      incident = getIncidentEdge(bestAxis.supportB, bestAxis.direction, pair.b, normal);
    }
    else {
      reference = getReferenceEdge(bestAxis.axis, bestAxis.direction, pair);
      incident = getIncidentEdge(bestAxis.supportA, bestAxis.direction, pair.a, normal);
    }

    ClipResult result = clipEdgeToEdge(normal, reference, incident);
    if(result.overlap.max >= 0.0f || result.overlap.min >= 0.0f) {
      manifold[0].centerToContactA = result.edge.start - pair.a.pos;
      manifold[0].centerToContactB = result.edge.start - pair.b.pos;
      manifold[0].overlap = result.overlap.min;
      manifold[1].centerToContactA = result.edge.end - pair.a.pos;
      manifold[1].centerToContactB = result.edge.end - pair.b.pos;
      manifold[1].overlap = result.overlap.max;
    }
  }
}
