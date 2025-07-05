#include <Precompile.h>
#include <Mass.h>

#include <Geometric.h>

namespace Mass {
  MassProps computeCapsuleMass(const Capsule& c) {
    MassProps result;
    const float r2 = c.radius*c.radius;
    const glm::vec2& top = c.top;
    const glm::vec2& bottom = c.bottom;
    const glm::vec2 topToBottom = bottom - top;
    const float length = glm::length(topToBottom);
    const float l2 = length*length;
    //Mass of the circles on the end of the capsule
    const float circleMass = Constants::PI*r2*c.density;
    //Mass of the middle section of the capsule, which in 2d is a box
    //Radius is half length so multiply by 2 for width * height
    const float boxMass = 2.f*c.radius*length*c.density;

    //Mass is the middle section plus the two half-circles on each end, which is a full circle
    result.body.inverseMass = circleMass + boxMass;
    result.centerOfMass = c.top + (topToBottom * 0.5f);

    //Box2d magic:
    //two offset half circles, both halves add up to full circle and each half is offset by half length
    //semi-circle centroid = 4 r / 3 pi
    //Need to apply parallel-axis theorem twice:
    //1. shift semi-circle centroid to origin
    //2. shift semi-circle to box end
    //m * ((h + lc)^2 - lc^2) = m * (h^2 + 2 * h * lc)
    //See: https://en.wikipedia.org/wiki/Parallel_axis_theorem
    const float halfCircleCentroid = (4.f * c.radius)/(3.f * Constants::PI);
    const float halfLength = length * 0.5f;
    const float circleInertia = circleMass * (0.5f*r2 + halfLength*halfLength + 2.f*halfLength*halfCircleCentroid);
    const float boxInertia = boxMass * (4.f*r2 + l2) / 12.f;

    result.body.inverseInertia = circleInertia + boxInertia;

    return invert(result);
  }

  MassProps computeTriangleMass(const Triangle& tri) {
    MassProps result;
    const glm::vec2 edgeA = tri.b - tri.a;
    const glm::vec2 edgeB = tri.c - tri.a;
    const float determinant = Geo::cross(edgeA, edgeB);
    const float area = determinant*0.5f;
    constexpr float third = 1.f / 3.f;
    result.centerOfMass = (tri.a + tri.b + tri.c) * third;
    result.body.inverseMass = area*tri.density;

    //Magic: https://stackoverflow.com/questions/41592034/computing-tensor-of-inertia-in-2d
    const glm::vec2 ca = tri.a - result.centerOfMass;
    const glm::vec2 cb = tri.b - result.centerOfMass;
    const glm::vec2 cc = tri.c - result.centerOfMass;
    constexpr float sixth = 1.f/6.f;
    result.body.inverseInertia = result.body.inverseMass * sixth * (glm::dot(ca, ca) + glm::dot(cb, cb) + glm::dot(cc, cc) + glm::dot(ca, cb) + glm::dot(cb, cc) + glm::dot(cc, ca));

    return invert(result);
  }

  MassProps computeMeshMass(const Mesh& mesh) {
    switch(mesh.count) {
    //No points, arbitrarily return infinite mass
    case 0:
      return {};
    case 1:
      return computeCircleMass(Circle{
        .center = mesh.ccwPoints[0],
        .radius = mesh.radius,
        .density = mesh.density
      });
    case 2:
      return computeCapsuleMass(Capsule{
        .top = mesh.ccwPoints[0],
        .bottom = mesh.ccwPoints[1],
        .radius = mesh.radius,
        .density = mesh.density
      });
    default:
      break;
    };

    const glm::vec2* points = mesh.ccwPoints;
    //If mesh has a radius, approximate it by pushing out the points by their normals using the temporary buffer
    if(mesh.radius > 0) {
      points = mesh.temp;
      glm::vec2 lastPoint = mesh.ccwPoints[mesh.count - 1];
      glm::vec2 lastNormal = glm::normalize(Geo::crossZ(mesh.ccwPoints[0] - lastPoint));
      for(uint32_t i = 0; i < mesh.count; ++i) {
        const glm::vec2& thisPoint = mesh.ccwPoints[i];
        const glm::vec2& nextPoint = mesh.ccwPoints[(i + 1) % mesh.count];
        const glm::vec2 thisNormal = glm::normalize(Geo::crossZ(nextPoint - thisPoint));
        //Box2d multiplies by sqrt(2) which seems like the two normals being treated as adjacent and opposite sides and the final
        //combined normal being the hypotenuse, but I don't know why, as expanding by the radius itself would be more intuitive.
        //Perhaps this accounts for the shape still not being round even if the vertex is near the edge of the rounded shape.
        mesh.temp[i] = thisPoint + Geo::normalizedOrZero(thisNormal + lastNormal)*mesh.radius;

        lastPoint = thisPoint;
        lastNormal = thisNormal;
      }
    }

    const glm::vec2& reference = points[0];
    constexpr float third = 1.f/3.f;

    MassProps result;
    glm::vec2 center{};
    for(uint32_t i = 1; i < mesh.count - 1; ++i) {
      //Form a triangle from reference to these two points
      const glm::vec2 edgeA = points[i] - reference;
      const glm::vec2 edgeB = points[i + 1] - reference;
      const float determinant = Geo::cross(edgeA, edgeB);

      const float triangleArea = determinant * 0.5f;
      result.body.inverseMass += triangleArea;

      //Area weighted centroid
      center += (edgeA + edgeB) * (triangleArea * third);

      //Box2d integration magic
      auto integral = [](float a, float b) { return a*a + a*b + b*b; };
      const float integralX = integral(edgeA.x, edgeB.x);
      const float integralY = integral(edgeA.y, edgeB.y);

      result.body.inverseInertia += (0.25f * third * determinant) * (integralX + integralY);
    }

    //Shift center of mass as computed from the reference point back to the origin
    const float inverseArea = Geo::inverseOrZero(result.body.inverseMass);
    center *= inverseArea;
    result.centerOfMass = center + reference;

    result.body.inverseMass *= mesh.density;
    result.body.inverseInertia *= mesh.density;
    result.body.inverseInertia -= result.body.inverseMass * glm::dot(center, center);

    return invert(result);
  }
}