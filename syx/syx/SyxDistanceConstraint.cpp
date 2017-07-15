#include "Precompile.h"
#include "SyxDistanceConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"

namespace Syx {
  float LocalDistanceConstraint::sSlop = 0.05f;

  void LocalDistanceConstraint::FirstIteration(void) {
    DistanceConstraint* owner = static_cast<DistanceConstraint*>(mOwner);
    Constraints::LoadObjects(mBlock.mA, mBlock.mB, *mA, *mB);

    //Compute anchor points and R vector
    SAlign Vector3 worldA = mA->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::A));
    SAlign Vector3 worldB = mB->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::B));
    SAlign Vector3 rA = worldA - mA->mPos;
    SAlign Vector3 rB = worldB - mB->mPos;

    //Compute jacobian
    JacobianSL& j = mBlock.mJ;
    Jacobian& jm = mBlock.mJM;
    j.mLinear = worldA - worldB;
    float linLen = mBlock.mJ.mLinear.Length();
    //If points are on top of each other, resolve in arbitrary direction
    if(linLen < SYX_EPSILON) {
      j.mLinear = Vector3::UnitY;
    }
    else {
      j.mLinear *= 1.0f/linLen;
    }

    mBlock.mBias = Constraints::ComputeBias(linLen - owner->mDistance, sSlop*0.5f, sVelBaumgarteTerm, sMaxVelCorrection);

    //Premultiply jacobian and compute mass
    Vector3 linearB = -j.mLinear;
    j.mAngularA = rA.Cross(j.mLinear);
    j.mAngularB = rB.Cross(linearB);

    jm.mLinearA = mA->mInvMass*j.mLinear;
    jm.mLinearB = -mB->mInvMass*j.mLinear;
    jm.mAngularA = mA->mInertia*j.mAngularA;
    jm.mAngularB = mB->mInertia*j.mAngularB;
    mBlock.mConstraintMass = SafeDivide(1.0f, mA->mInvMass + mB->mInvMass + 
      j.mAngularA.Dot(jm.mAngularA) + j.mAngularB.Dot(jm.mAngularB), SYX_EPSILON);

    //Warm start and update objects
    mBlock.mLambdaSum = owner->mWarmStart;
    Constraints::ApplyImpulse(mBlock.mLambdaSum, jm.mLinearA, jm.mAngularA, jm.mLinearB, jm.mAngularB, mBlock.mA, mBlock.mB);
    Constraints::StoreVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
  }

  void LocalDistanceConstraint::LastIteration(void) {
    static_cast<DistanceConstraint*>(mOwner)->mWarmStart = mBlock.mLambdaSum;
  }

  float LocalDistanceConstraint::Solve(void) {
    const JacobianSL& j = mBlock.mJ;
    const Jacobian& jm = mBlock.mJM;
    Constraints::LoadVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
    float jv = Constraints::ComputeJV(j.mLinear, j.mAngularA, -j.mLinear, j.mAngularB, mBlock.mA, mBlock.mB);
    float lambda = Constraints::ComputeLambda(jv, mBlock.mBias, mBlock.mConstraintMass);
    mBlock.mLambdaSum += lambda;
    Constraints::ApplyImpulse(lambda, jm.mLinearA, jm.mAngularA, jm.mLinearB, jm.mAngularB, mBlock.mA, mBlock.mB);
    Constraints::StoreVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
    return std::abs(lambda);
  }

  float LocalDistanceConstraint::SSolve(void) {
    const JacobianSL& j = mBlock.mJ;
    const Jacobian& jm = mBlock.mJM;
    SFloats linVelA, angVelA, linVelB, angVelB;
    SConstraints::LoadVelocity(*mA, *mB, linVelA, angVelA, linVelB, angVelB);
    SFloats linear = SLoadAll(&j.mLinear.x);
    SFloats lambda = SConstraints::ComputeJV(linear, SLoadAll(&j.mAngularA.x), SVector3::Neg(linear), SLoadAll(&j.mAngularB.x), linVelA, angVelA, linVelB, angVelB);

    //Load once, shuffle to get the appropriate value
    SAlign Vector3 store(mBlock.mBias, mBlock.mLambdaSum, mBlock.mConstraintMass);
    SFloats floats = SLoadAll(&store.x);

    lambda = SConstraints::ComputeLambda(lambda, SShuffle(floats, 0, 0, 0, 0), SShuffle(floats, 2, 2, 2, 2));

    SFloats lambdaSum = SAddAll(SShuffle(floats, 1, 1, 1, 1), lambda);
    //Store return value and new lambda sum in store
    SStoreAll(&store.x, SCombine(SAbsAll(lambda), lambdaSum));

    SConstraints::ApplyImpulse(lambda, linVelA, angVelA, linVelB, angVelB, SLoadAll(&jm.mLinearA.x), SLoadAll(&jm.mAngularA.x), SLoadAll(&jm.mLinearB.x), SLoadAll(&jm.mAngularB.x));
    SConstraints::StoreVelocity(linVelA, angVelA, linVelB, angVelB, *mA, *mB);

    mBlock.mLambdaSum = store.z;
    return store.x;
  }

  void LocalDistanceConstraint::Draw() {
    SAlign Vector3 worldA = mA->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::A));
    SAlign Vector3 worldB = mB->mOwner->GetTransform().ModelToWorld(mOwner->GetLocalAnchor(ConstraintObj::B));
    DebugDrawer& d = DebugDrawer::Get();
    float s = 0.1f;
    d.SetColor(1.0f, 0.0f, 0.0f);
    d.DrawPoint(worldA, s);
    d.SetColor(0.0f, 0.0f, 1.0f);
    d.DrawPoint(worldB, s);
    d.SetColor(0.0f, 1.0f, 0.0f);
    d.DrawLine(worldA, worldB);
  }
}