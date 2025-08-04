#pragma once

#include <math/Constants.h>
#include <glm/vec2.hpp>
#include <glm/gtx/norm.hpp>

namespace Mass {
  constexpr float DEFAULT_DENSITY = 1.f;
  struct OriginMass {
    float inverseMass{};
    float inverseInertia{};
  };
  struct MassProps {
    OriginMass body;
    glm::vec2 centerOfMass{};
  };

  struct Quad {
    glm::vec2 fullSize{};
    float density{ DEFAULT_DENSITY };
  };

  struct Circle {
    glm::vec2 center{};
    float radius{};
    float density{ DEFAULT_DENSITY };
  };

  struct Capsule {
    glm::vec2 top{};
    glm::vec2 bottom{};
    float radius{};
    float density{ DEFAULT_DENSITY };
  };

  struct Mesh {
    //Convex hull of mesh wrapping in counter-clockwise order
    const glm::vec2* ccwPoints{};
    //Temporary buffer needed for the computation, same size as ccwPoints
    glm::vec2* temp{};
    uint32_t count{};
    float radius{};
    float density{ DEFAULT_DENSITY };
  };

  struct Triangle {
    //Counterclockwise winding in the direction a b c
    glm::vec2 a, b, c;
    float density{ DEFAULT_DENSITY };
  };

  constexpr MassProps invert(MassProps props) {
    //Assumes that if inverse mass is nonzero inertia is as well, since inertia is based on mass
    if(props.body.inverseMass > 0.f) {
      props.body.inverseMass = 1.f/props.body.inverseMass;
      props.body.inverseInertia = 1.f/props.body.inverseInertia;
    }
    return props;
  }

  //Takes size from one size to the other, not one side to the center
  constexpr MassProps computeQuadMass(const Quad& q) {
    MassProps result;
    result.body.inverseMass = q.fullSize.x*q.fullSize.y*q.density;
    result.body.inverseInertia = result.body.inverseMass*(q.fullSize.x*q.fullSize.x + q.fullSize.y*q.fullSize.y)/12.0f;
    return invert(result);
  }

  constexpr MassProps computeCircleMass(const Circle& c) {
    MassProps result;
    const float r2 = c.radius*c.radius;
    result.body.inverseMass = r2*Constants::PI*c.density;
    result.body.inverseInertia = result.body.inverseMass*r2*0.5f;
    result.centerOfMass = c.center;
    return invert(result);
  }

  MassProps computeCapsuleMass(const Capsule& c);
  MassProps computeMeshMass(const Mesh& mesh);
  MassProps computeTriangleMass(const Triangle& tri);

  inline MassProps compute(const Capsule& c) { return computeCapsuleMass(c); }
  inline MassProps compute(const Mesh& m) { return computeMeshMass(m); }
  inline MassProps compute(const Circle& c) { return computeCircleMass(c); }
  inline MassProps compute(const Quad& q) { return computeQuadMass(q); }
  inline MassProps compute(const Triangle& t) { return computeTriangleMass(t); }
}