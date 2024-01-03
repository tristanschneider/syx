#pragma once

#include "glm/vec2.hpp"
#include "glm/mat3x3.hpp"

namespace Math {
  template<class T>
  auto enumCast(T e) {
    return static_cast<std::underlying_type_t<T>>(e);
  }

  struct Impulse {
    glm::vec2 linear{ 0.0f };
    float angular{};
  };

  struct ConstraintImpulse {
    Impulse a, b;
  };

  struct HalfVelocityVector {
    glm::vec2 linear{ 0.0f };
    float angular{};
  };

  struct VelocityVector {
    HalfVelocityVector a, b;
  };

  struct HalfJacobian {
    glm::vec2 linear{};
    float angular{};
  };

  struct Jacobian {
    HalfJacobian a, b;
  };

  struct ConstraintLimits {
    float lambdaMin = std::numeric_limits<float>::min();
    float lambdaMax = std::numeric_limits<float>::max();
    float bias{};
  };

  struct Mass {
    float inverseMass{};
    float inverseInertia{};
  };

  struct MassPair {
    Mass a, b;
  };

  struct Constraint {
    VelocityVector velocity;
    Jacobian jacobian;
    ConstraintLimits limits;
    MassPair objMass;
  };

  //TODO: store this somewhere
  constexpr Mass computeQuadMass(float w, float h, float density) {
    Mass result;
    result.inverseMass = w*h*density;
    result.inverseInertia = result.inverseMass*(h*h + w*w)/12.0f;
    if(result.inverseMass > 0.0f) {
      result.inverseMass = 1.0f/result.inverseMass;
      result.inverseInertia = 1.0f/result.inverseInertia;
    }
    return result;
  }

  constexpr Mass computePlayerMass() {
    return computeQuadMass(1.0f, 1.0f, 1.0f);
  }

  constexpr Mass computeFragmentMass() {
    return computeQuadMass(1.0f, 1.0f, 1.0f);
  }

  constexpr glm::vec2 getFragmentExtents() {
    return glm::vec2{ 0.5f, 0.5f };
  }

  constexpr glm::vec2 getFragmentScale() {
    return getFragmentExtents() * glm::vec2(2.0f);
  }

  constexpr float cross(const glm::vec2& a, const glm::vec2& b) {
    return a.x*b.y - a.y*b.x;
  }

  //Cross a with z axis vector of given length
  constexpr glm::vec2 crossZ(const glm::vec2& a, float length) {
    return { a.y*length, -a.x*length };
  }

  //3d cross product with z axis
  // [v.x]   [0]
  // [v.y] x [0]
  // [0  ]   [1]
  constexpr glm::vec2 orthogonal(const glm::vec2& v) {
    return { v.y, -v.x };
  }

  //2d rotation matrix multiplication with 2d vector
  //[cos, -sin][x]
  //[sin,  cos][y]
  inline glm::vec2 rotate(const glm::vec2& v, float cosAngle, float sinAngle) {
    return {
      v.x*cosAngle - v.y*sinAngle,
      v.x*sinAngle + v.y*cosAngle
    };
  }

  inline glm::vec2 rotate(const glm::vec2& v, float a) {
    return rotate(v, std::cos(a), std::sin(a));
  }

  inline glm::mat3 buildTranslate(const glm::vec2& t, float z = 1.0f) {
    glm::mat3 result;
    result[0] = glm::vec3(1, 0, 0);
    result[1] = glm::vec3(0, 1, 0);
    result[2] = glm::vec3(t.x, t.y, z);
    return result;
  }

  inline glm::mat3 buildScale(const glm::vec2& s) {
    return {
      s.x, 0.0f, 0.0f,
      0.0f, s.y, 0.0f,
      0.0f, 0.0f, 1.0f
    };
  }

  inline glm::mat3 buildRotate(const glm::vec2& r) {
    glm::mat3 result;
    const glm::vec2& basisX = r;
    const glm::vec2 basisY = orthogonal(basisX);
    result[0] = glm::vec3(basisX.x, basisX.y, 0.0f);
    result[1] = glm::vec3(basisY.x, basisY.y, 0.0f);
    result[2] = glm::vec3(0, 0, 1);
    return result;
  }

  inline glm::mat3 buildTransform(const glm::vec2& translate, const glm::vec2& rotate, const glm::vec2& scale) {
    //Could be done in less operations if built by hand
    return buildTranslate(translate) * buildRotate(rotate) * buildScale(scale);
  }

  //Transform vector, not point, meaning last row is ignored, as in [x, y, 0]
  inline glm::vec2 transformVector(const glm::mat3& transform, const glm::vec2& v) {
    return {
      transform[0].x*v.x + transform[1].x*v.y,
      transform[0].y*v.x + transform[1].y*v.y
    };
  }

  inline glm::vec2 transformPoint(const glm::mat3& transform, const glm::vec2& p) {
    //Could optimize out some of the zeroes from the last row
    const glm::vec3 result = transform*glm::vec3{ p.x, p.y, 1.0f };
    return { result.x, result.y };
  }

  inline bool between(float v, float min, float max) {
    return v >= min && v <= max;
  }

  inline bool between(const glm::vec2& v, const glm::vec2& min, const glm::vec2& max) {
    return between(v.x, min.x, max.x) && between(v.y, min.y, max.y);
  }

  Impulse computeImpulseAtPoint(const glm::vec2& r, const glm::vec2& impulse, const Mass& mass);
  Impulse computeImpulseAtPoint(const glm::vec2& centerOfMass, const glm::vec2& impulsePoint, const glm::vec2& impulse, const Mass& mass);

  Impulse computeSpringImpulse(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& springAttach, float springConstant);

  ConstraintImpulse solveConstraint(const Constraint& c);
  Impulse computePointConstraint(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& attachPoint, const HalfVelocityVector& objectVelocity, const glm::vec2& pointVelocity, float bias, const Mass& mass);


  bool unitAABBLineIntersect(const glm::vec2& origin, const glm::vec2& dir, float* resultTIn, float* resultTOut);
  glm::vec2 getNormalFromUnitAABBIntersect(const glm::vec2& intersect);
}
