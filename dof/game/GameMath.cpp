#include "Precompile.h"

#include "GameMath.h"

#include "glm/gtx/norm.hpp"

namespace Math {
  Impulse computeSpringImpulse(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& springAttach, float springConstant) {
    const glm::vec2 objToSpring = springAttach - objectAttach;
    const glm::vec2 linearImpulse = objToSpring * springConstant;

    const glm::vec2 centerToAttach = objectAttach - objectCenter;
    return {
      linearImpulse,
      cross(centerToAttach, linearImpulse)
    };
  }


  ConstraintImpulse solveConstraint(const Constraint& c) {
    const float constraintMass = c.objMass.a.inverseMass*glm::dot(c.jacobian.a.linear, c.jacobian.a.linear)
      + c.objMass.b.inverseMass*glm::dot(c.jacobian.b.linear, c.jacobian.b.linear)
      + c.objMass.a.inverseInertia*c.jacobian.a.angular*c.jacobian.a.angular
      + c.objMass.b.inverseInertia*c.jacobian.b.angular*c.jacobian.b.angular;
    //If mass is zero it would mean both objects have infinite mass or jacobian axes are empty, nothing to apply
    if(constraintMass < 0.001f) {
      return {};
    }
    const float invConstraintMass = 1.0f/constraintMass;

    const float jv = glm::dot(c.jacobian.a.linear, c.velocity.a.linear)
      + glm::dot(c.jacobian.b.linear, c.velocity.b.linear)
      + c.jacobian.a.angular*c.velocity.a.angular
      + c.jacobian.b.angular*c.velocity.b.angular;
    const float b = c.limits.bias;
    const float rawLambda = -(jv + b)*invConstraintMass;
    //With iterative solving this would need to clamp the total sum not the particular impulse
    const float lambda = glm::clamp(rawLambda, c.limits.lambdaMin, c.limits.lambdaMax);

    return {
      Impulse{ c.jacobian.a.linear*lambda, c.jacobian.a.angular*lambda },
      Impulse{ c.jacobian.b.linear*lambda, c.jacobian.b.angular*lambda }
    };
  }

  Impulse computePointConstraint(const glm::vec2& objectAttach, const glm::vec2& objectCenter, const glm::vec2& attachPoint, const HalfVelocityVector& objectVelocity, const glm::vec2& pointVelocity, float bias, const Mass& mass) {
    const glm::vec2 r = objectAttach - objectCenter;
    const glm::vec2 errorVec = objectAttach - attachPoint;
    Impulse result;
    HalfVelocityVector v = objectVelocity;
    for(int i = 0; i < 2; ++i) {
      const float linearVelocity = v.linear[i] - pointVelocity[i];
      glm::vec2 axis{ 0.0f };
      axis[i] = 1.0f;
      const float angularAxis = cross(r, axis);
      const float angularVelocity = v.angular*angularAxis;
      const float angularImpulse = angularAxis*mass.inverseInertia;
      const float constraintMass = 1.0f/(mass.inverseMass + angularAxis*angularImpulse);

      const float jv = linearVelocity + angularVelocity;
      const float b = errorVec[i]*bias;
      const float lambda = -(jv + b)*constraintMass;
      if(lambda > 0.0f) {
        const float resultLinearImpulse = lambda*mass.inverseMass;
        const float resultAngularImpulse = lambda*angularImpulse;

        result.linear[i] = resultLinearImpulse;
        result.angular += resultAngularImpulse;
        v.linear[i] += resultLinearImpulse;
        v.angular += resultAngularImpulse;
      }
    }
    return result;
  }

  bool unitAABBLineIntersect(const glm::vec2& origin, const glm::vec2& dir, float* resultTIn, float* resultTOut) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    //Compute intersect Ts with slabs separated into near and far planes
    for(int i = 0; i < 2; ++i) {
      constexpr float aabbMin = -0.5f;
      constexpr float aabbMax = 0.5f;

      if(std::abs(dir[i]) > 0.00001f) {
        const float recip = 1.0f/dir[i];
        float curMin = (aabbMin - origin[i])*recip;
        float curMax = (aabbMax - origin[i])*recip;
        if(curMin > curMax) {
          std::swap(curMin, curMax);
        }
        if(curMin > tMin) {
          tMin = curMin;
        }
        tMin = std::max(curMin, tMin);
        tMax = std::min(curMax, tMax);
      }
      //No change on this axis, so just need to make sure it is within box on this axis
      else if(!Math::between(origin[i], aabbMin, aabbMax)) {
        return false;
      }
    }

    //if tMax < 0, ray (line) is intersecting AABB, but whole AABB is behind us
    //if tMin > tMax, ray doesn't intersect AABB, as not all axes were overlapping at the same time
    //if tMax > length ray doesn't go far enough to intersect
    //if tMin < 0 start point is inside
    if(tMax < 0 || tMin > tMax) {
      return false;
    }
    if(resultTIn) {
      *resultTIn = std::max(0.0f, tMin);
    }
    if(resultTOut) {
      *resultTOut = tMax;
    }
    return true;
  }

  Impulse computeImpulseAtPoint(const glm::vec2& r, const glm::vec2& impulse, const Mass& mass) {
    return {
      impulse * mass.inverseMass,
      cross(r, impulse) * mass.inverseInertia
    };
  }

  Impulse computeImpulseAtPoint(const glm::vec2& centerOfMass, const glm::vec2& impulsePoint, const glm::vec2& impulse, const Mass& mass) {
    return computeImpulseAtPoint(impulsePoint - centerOfMass, impulse, mass);
  }

}
