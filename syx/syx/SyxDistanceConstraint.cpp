#include "Precompile.h"
#include "SyxDistanceConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"

namespace Syx {
  float LocalDistanceConstraint::sSlop = 0.05f;

  void LocalDistanceConstraint::firstIteration(void) {
    DistanceConstraint* owner = static_cast<DistanceConstraint*>(mOwner);
    Constraints::loadObjects(mBlock.mA, mBlock.mB, *mA, *mB);

    //Compute anchor points and R vector
    SAlign Vec3 worldA = mA->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::A));
    SAlign Vec3 worldB = mB->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::B));
    SAlign Vec3 rA = worldA - mA->mPos;
    SAlign Vec3 rB = worldB - mB->mPos;

    //Compute jacobian
    JacobianSL& j = mBlock.mJ;
    Jacobian& jm = mBlock.mJM;
    j.mLinear = worldA - worldB;
    float linLen = mBlock.mJ.mLinear.length();
    //If points are on top of each other, resolve in arbitrary direction
    if(linLen < SYX_EPSILON) {
      j.mLinear = Vec3::UnitY;
    }
    else {
      j.mLinear *= 1.0f/linLen;
    }

    mBlock.mBias = Constraints::computeBias(linLen - owner->mDistance, sSlop*0.5f, sVelBaumgarteTerm, sMaxVelCorrection);

    //Premultiply jacobian and compute mass
    Vec3 linearB = -j.mLinear;
    j.mAngularA = rA.cross(j.mLinear);
    j.mAngularB = rB.cross(linearB);

    jm.mLinearA = mA->mInvMass*j.mLinear;
    jm.mLinearB = -mB->mInvMass*j.mLinear;
    jm.mAngularA = mA->mInertia*j.mAngularA;
    jm.mAngularB = mB->mInertia*j.mAngularB;
    mBlock.mConstraintMass = safeDivide(1.0f, mA->mInvMass + mB->mInvMass + 
      j.mAngularA.dot(jm.mAngularA) + j.mAngularB.dot(jm.mAngularB), SYX_EPSILON);

    //Warm start and update objects
    mBlock.mLambdaSum = owner->mWarmStart;
    Constraints::applyImpulse(mBlock.mLambdaSum, jm.mLinearA, jm.mAngularA, jm.mLinearB, jm.mAngularB, mBlock.mA, mBlock.mB);
    Constraints::storeVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
  }

  void LocalDistanceConstraint::lastIteration(void) {
    static_cast<DistanceConstraint*>(mOwner)->mWarmStart = mBlock.mLambdaSum;
  }

  float LocalDistanceConstraint::solve(void) {
    const JacobianSL& j = mBlock.mJ;
    const Jacobian& jm = mBlock.mJM;
    Constraints::loadVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
    float jv = Constraints::computeJV(j.mLinear, j.mAngularA, -j.mLinear, j.mAngularB, mBlock.mA, mBlock.mB);
    float lambda = Constraints::computeLambda(jv, mBlock.mBias, mBlock.mConstraintMass);
    mBlock.mLambdaSum += lambda;
    Constraints::applyImpulse(lambda, jm.mLinearA, jm.mAngularA, jm.mLinearB, jm.mAngularB, mBlock.mA, mBlock.mB);
    Constraints::storeVelocity(mBlock.mA, mBlock.mB, *mA, *mB);
    return std::abs(lambda);
  }

  float LocalDistanceConstraint::sSolve(void) {
    const JacobianSL& j = mBlock.mJ;
    const Jacobian& jm = mBlock.mJM;
    SFloats linVelA, angVelA, linVelB, angVelB;
    SConstraints::loadVelocity(*mA, *mB, linVelA, angVelA, linVelB, angVelB);
    SFloats linear = SLoadAll(&j.mLinear.x);
    SFloats lambda = SConstraints::computeJV(linear, SLoadAll(&j.mAngularA.x), SVec3::neg(linear), SLoadAll(&j.mAngularB.x), linVelA, angVelA, linVelB, angVelB);

    //Load once, shuffle to get the appropriate value
    SAlign Vec3 store(mBlock.mBias, mBlock.mLambdaSum, mBlock.mConstraintMass);
    SFloats floats = SLoadAll(&store.x);

    lambda = SConstraints::computeLambda(lambda, SShuffle(floats, 0, 0, 0, 0), SShuffle(floats, 2, 2, 2, 2));

    SFloats lambdaSum = SAddAll(SShuffle(floats, 1, 1, 1, 1), lambda);
    //Store return value and new lambda sum in store
    SStoreAll(&store.x, sCombine(sAbsAll(lambda), lambdaSum));

    SConstraints::applyImpulse(lambda, linVelA, angVelA, linVelB, angVelB, SLoadAll(&jm.mLinearA.x), SLoadAll(&jm.mAngularA.x), SLoadAll(&jm.mLinearB.x), SLoadAll(&jm.mAngularB.x));
    SConstraints::storeVelocity(linVelA, angVelA, linVelB, angVelB, *mA, *mB);

    mBlock.mLambdaSum = store.z;
    return store.x;
  }

  void LocalDistanceConstraint::draw() {
    SAlign Vec3 worldA = mA->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::A));
    SAlign Vec3 worldB = mB->mOwner->getTransform().modelToWorld(mOwner->getLocalAnchor(ConstraintObj::B));
    DebugDrawer& d = DebugDrawer::get();
    float s = 0.1f;
    d.setColor(1.0f, 0.0f, 0.0f);
    d.drawPoint(worldA, s);
    d.setColor(0.0f, 0.0f, 1.0f);
    d.drawPoint(worldB, s);
    d.setColor(0.0f, 1.0f, 0.0f);
    d.drawLine(worldA, worldB);
  }
}