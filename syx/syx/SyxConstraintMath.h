#pragma once
#include "SyxVec3.h"
#include "SyxConstraint.h"
#include "SyxSIMD.h"

namespace Syx {
  namespace Constraints {
    FInline void LoadObjects(ConstraintObjBlock& ba, ConstraintObjBlock& bb, const LocalObject& a, const LocalObject& b) {
      ba.Set(a);
      bb.Set(b);
    }

    FInline void LoadVelocity(ConstraintObjBlock& ba, ConstraintObjBlock& bb, const LocalObject& a, const LocalObject& b) {
      ba.LoadVelocity(a);
      bb.LoadVelocity(b);
    }

    FInline void StoreVelocity(const ConstraintObjBlock& ba, const ConstraintObjBlock& bb, LocalObject& a, LocalObject& b) {
      ba.StoreVelocity(a);
      bb.StoreVelocity(b);
    }

    //Solve towards middle of slop region with half slop, otherwise there's instability on slop boundary
    FInline float ComputeBiasPos(float error, float halfSlop, float baumgarteTerm, float maxCorrection) {
      if(error < halfSlop)
        return 0.0f;
      return std::min(maxCorrection, (error - halfSlop)*baumgarteTerm);
    }

    FInline float ComputeBiasNeg(float error, float halfSlop, float baumgarteTerm, float maxCorrection) {
      if(error > -halfSlop)
        return 0.0f;
      return std::max(-maxCorrection, (error + halfSlop)*baumgarteTerm);
    }

    FInline float ComputeBias(float error, float halfSlop, float baumgarteTerm, float maxCorrection) {
      if(error > 0.0f) {
        if(error < halfSlop)
          return 0.0f;
        return std::min(maxCorrection, (error - halfSlop)*baumgarteTerm);
      }
      if(error > -halfSlop)
        return 0.0f;
      return std::max(-maxCorrection, (error + halfSlop)*baumgarteTerm);
    }

    FInline float ComputeJV(const Vec3& jal, const Vec3& jaa, const Vec3& jbl, const Vec3& jba, const ConstraintObjBlock& a, const ConstraintObjBlock& b) {
      return jal.Dot(a.mLinVel) + jaa.Dot(a.mAngVel) + jbl.Dot(b.mLinVel) + jba.Dot(b.mAngVel);
    }

    //1 constraint
    FInline float ComputeLambda(float jv, float bias, float constraintMass) {
      return -(jv + bias)*constraintMass;
    }

    //3 constraints
    FInline Vec3 ComputeLambda(const Vec3& jv, const Vec3& bias, const Mat3& constraintMass) {
      //Constraint mass is a symmetric matrix, so might be able to shave of a few computations from a straight multiplication like this
      return constraintMass*(-(jv + bias));
    }

    //2 constraints
    FInline Vec3 ComputeLambda(const Vec3& jv, const Vec3& bias, const Vec3& constraintMass) {
      Vec3 jvb(-(jv.x + bias.x), -(jv.y + bias.y), 0.0f);
      return constraintMass.Mat2Mul(jvb);
    }

    FInline float ComputeLambda(float jv, float constraintMass) {
      return -jv*constraintMass;
    }

    FInline void ClampLambdaMin(float& lambda, float& lambdaSum, float minBound) {
      float oldSum = lambdaSum;
      lambdaSum += lambda;
      lambdaSum = std::max(lambdaSum, minBound);
      lambda = lambdaSum - oldSum;
    }

    FInline void ClampLambdaMax(float& lambda, float& lambdaSum, float maxBound) {
      float oldSum = lambdaSum;
      lambdaSum += lambda;
      lambdaSum = std::min(lambdaSum, maxBound);
      lambda = lambdaSum - oldSum;
    }

    FInline void ClampLambda(float& lambda, float& lambdaSum, float minBound, float maxBound) {
      float oldSum = lambdaSum;
      lambdaSum += lambda;
      if(lambdaSum < minBound)
        lambdaSum = minBound;
      else if(lambdaSum > maxBound)
        lambdaSum = maxBound;
      lambda = lambdaSum - oldSum;
    }

    FInline void ComputeLambdaBounds(float maxSum, char state, float& resultMin, float& resultMax) {
      if(state == LocalConstraint::EnforcePos) {
        resultMin = 0.0f;
        resultMax = std::numeric_limits<float>::max();
      }
      else if(state == LocalConstraint::EnforceNeg) {
        resultMin = std::numeric_limits<float>::lowest();
        resultMax = 0.0f;
      }
      else {
        resultMin = -maxSum;
        resultMax = maxSum;
      }
    }

    FInline float ComputeCumulativeAngleError(float lastError, const Vec3& refA, const Vec3& refB, const Vec3& normal) {
      Vec3 accumRefA = Mat3::AxisAngle(normal, lastError)*refA;
      float error = lastError + std::asin(accumRefA.Cross(refB).Dot(normal));
      //To prevent rounding error from accumulating, reset to the absolute value when we're within 90 degrees in either direction
      if(error > -SYX_PI_2 && error < SYX_PI_2)
        error = std::asin(refA.Cross(refB).Dot(normal));
      return error;
    }

    FInline void ComputeAngularLimitError(float minError, float maxError, bool enforceInBounds, float& error, char& enforceDir) {
      if(error < minError) {
        error -= minError;
        enforceDir = LocalConstraint::EnforceNeg;
      }
      else if(error > maxError) {
        error -= maxError;
        enforceDir = LocalConstraint::EnforcePos;
      }
      //If there's no angular error, we only still want to enforce if we're supposed to apply resistance
      else if(enforceInBounds)
        enforceDir = LocalConstraint::EnforceBoth;
      else
        enforceDir = LocalConstraint::NoEnforce;
    }

    FInline void ApplyImpulse(float lambda, const Vec3& jalm, const Vec3& jaam, const Vec3& jblm, const Vec3& jbam, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      a.mLinVel += lambda*jalm;
      a.mAngVel += lambda*jaam;
      b.mLinVel += lambda*jblm;
      b.mAngVel += lambda*jbam;
    }

    FInline void ApplyAngularImpulse(float lambda, const Vec3& jaam, const Vec3& jbam, ConstraintObjBlock& a, ConstraintObjBlock& b) {
      a.mAngVel += lambda*jaam;
      b.mAngVel += lambda*jbam;
    }
  }
}