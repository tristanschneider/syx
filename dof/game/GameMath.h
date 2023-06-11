#pragma once

#include "glm/vec2.hpp"

namespace Math {
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

  Impulse computeSpringImpulse(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& springAttach, float springConstant);

  ConstraintImpulse solveConstraint(const Constraint& c);
  Impulse computePointConstraint(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& attachPoint, const HalfVelocityVector& objectVelocity, const glm::vec2& pointVelocity, float bias, const Mass& mass);
}
