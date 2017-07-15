#include "Precompile.h"
#include "SyxContactConstraint.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"

namespace Syx {
  float LocalContactConstraint::sPositionSlop = 0.01f;
  float LocalContactConstraint::sTimeToRemove = 2.0f;

  LocalContactConstraint::LocalContactConstraint(ContactConstraint& owner)
    : mManifold(&owner.mManifold)
    , mShouldRemove(&owner.mShouldRemove)
    , mInactiveTime(&owner.mInactiveTime) {}

  void LocalContactConstraint::SetupContactJacobian(float massA, const Matrix3& inertiaA, float massB, const Matrix3& inertiaB) {
    //The equation expects a normal going away from a, but manifold holds the opposite
    Vector3 normalB = -mManifold->mNormal;
    Vector3 normalA = mContactBlock.mNormal = mManifold->mNormal;
    //Set linear and angular terms of the jacobian and pre-multiply in the appropriate masses
    mContactBlock.mNormalTMass[0] = normalA*massA;
    mContactBlock.mNormalTMass[1] = normalB*massB;

    //Mass of the linear portion of the Jacobian, which is the same for all contacts since they share the same normal
    float linearMass = massA + massB;

    for(size_t i = 0; i < mManifold->mSize; ++i) {
      mContactBlock.mLambdaSum[i] = 0.0f;

      const ContactPoint& c = mManifold->mContacts[i];
      mContactBlock.mRCrossNA[i] = -((c.mObjA.mCurrentWorld - mBlockObjA.mPos).Cross(normalB));
      mContactBlock.mRCrossNB[i] = (c.mObjB.mCurrentWorld - mBlockObjB.mPos).Cross(normalB);
      mContactBlock.mRCrossNATInertia[i] = inertiaA*mContactBlock.mRCrossNA[i];
      mContactBlock.mRCrossNBTInertia[i] = inertiaB*mContactBlock.mRCrossNB[i];

      //Mass of the angular portion of the jacobian
      float angularMass = mContactBlock.mRCrossNATInertia[i].Dot(mContactBlock.mRCrossNA[i]) + mContactBlock.mRCrossNBTInertia[i].Dot(mContactBlock.mRCrossNB[i]);
      mContactBlock.mContactMass[i] = SafeDivide(1.0f, linearMass + angularMass, SYX_EPSILON);

      mContactBlock.mEnforce[i] = c.mPenetration > 0.0f;
      float posError = -std::max(0.0f, c.mPenetration);
      //Position error is always negative
      mContactBlock.mPenetrationBias[i] = Constraints::ComputeBiasNeg(posError, sPositionSlop*0.5f, sVelBaumgarteTerm, sMaxVelCorrection);

      if(mContactBlock.mEnforce[i]) {
        *mInactiveTime = 0.0f;
        if(c.mWarmContact) {
          Constraints::ApplyImpulse(c.mWarmContact, mContactBlock.mNormalTMass[0], mContactBlock.mRCrossNATInertia[i], mContactBlock.mNormalTMass[1], mContactBlock.mRCrossNBTInertia[i], mBlockObjA, mBlockObjB);
          mContactBlock.mLambdaSum[i] = c.mWarmContact;
        }
      }
    }

    //Don't enforce contacts that don't exist
    for(size_t i = mManifold->mSize; i < 4; ++i)
      mContactBlock.mEnforce[i] = false;

    *mInactiveTime += Constraint::sDT;
    if(*mInactiveTime > sTimeToRemove) {
      *mShouldRemove = true;
    }

    SetupFrictionJacobian(massA, inertiaA, massB, inertiaB);
  }

  void LocalContactConstraint::SetupFrictionJacobian(float massA, const Matrix3& inertiaA, float massB, const Matrix3& inertiaB) {
    for(int axis = 0; axis < 2; ++axis) {
      Vector3 dir = !axis ? mManifold->mTangentA : mManifold->mTangentB;
      FrictionAxisBlock& block = mFrictionBlock.mAxes[axis];

      Vector3 axisA = block.mAxis = -dir;
      Vector3 axisB = dir;
      block.mLinearA = axisA*massA;
      block.mLinearB = axisB*massB;

      float linearMass = axisA.Dot(block.mLinearA) + axisB.Dot(block.mLinearB);

      for(size_t i = 0; i < mManifold->mSize; ++i) {
        const ContactPoint& c = mManifold->mContacts[i];
        block.mRCrossAxisA[i] = -((c.mObjA.mCurrentWorld - mBlockObjA.mPos).Cross(dir));
        block.mRCrossAxisB[i] = (c.mObjB.mCurrentWorld - mBlockObjB.mPos).Cross(dir);
        block.mAngularA[i] = inertiaA*block.mRCrossAxisA[i];
        block.mAngularB[i] = inertiaB*block.mRCrossAxisB[i];

        float angularMass = block.mRCrossAxisA[i].Dot(block.mAngularA[i]) + block.mRCrossAxisB[i].Dot(block.mAngularB[i]);
        block.mConstraintMass[i] = SafeDivide(1.0f, linearMass + angularMass, SYX_EPSILON);

        block.mLambdaSum[i] = 0.0f;
        float warmStart = c.mWarmFriction[axis];
        if(warmStart && mContactBlock.mEnforce[i]) {
          Constraints::ApplyImpulse(warmStart, block.mLinearA, block.mAngularA[i], block.mLinearB, block.mAngularB[i], mBlockObjA, mBlockObjB);
          mFrictionBlock.mAxes[axis].mLambdaSum[i] = warmStart;
        }
      }
    }

    for(int i = 0; i < 4; ++i) {
      mFrictionBlock.mEnforce[i] = mContactBlock.mEnforce[i];
      //Zero now, but could be different when I add warm starting
      mFrictionBlock.mContactLambdaSum[i] = mContactBlock.mLambdaSum[i];
    }

    //Store result of warm starts from contact and friction
    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
  }

  void LocalContactConstraint::FirstIteration(void) {
    Constraints::LoadObjects(mBlockObjA, mBlockObjB, *mA, *mB);
    SetupContactJacobian(mA->mInvMass, mA->mInertia, mB->mInvMass, mB->mInertia);
  }

  void LocalContactConstraint::LastIteration(void) {
    for(int i = 0; i < 4; ++i) {
      ContactPoint& c = mManifold->mContacts[i];
      if(!mContactBlock.mEnforce[i])
        c.mWarmContact = c.mWarmFriction[0] = c.mWarmFriction[1] = 0.0f;
      else {
        c.mWarmContact = mContactBlock.mLambdaSum[i];
        c.mWarmFriction[0] = mFrictionBlock.mAxes[0].mLambdaSum[i];
        c.mWarmFriction[1] = mFrictionBlock.mAxes[1].mLambdaSum[i];
      }
    }
  }

  float LocalContactConstraint::Solve(void) {
    mFrictionBlock.mContactLambdaSum = mContactBlock.mLambdaSum;
    Constraints::LoadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    float result = 0.0f;
    //Solve friction first because it's less important, and the last constraint wins
    for(int i = 0; i < 4; ++i)
      if(!mFrictionBlock.mEnforce[i])
        continue;
      else
        result += SolveFriction(i);

    for(int i = 0; i < 4; ++i)
      if(!mContactBlock.mEnforce[i])
        continue;
      else
        result += SolveContact(i);

    //Store velocities back for other constraints to access
    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    return result;
  }

  float LocalContactConstraint::SolveContact(int i) {
    //J = jacobian, V = velocity vector, b = bias term, M = Jacobian mass
    //Calculate lambda = -(JV + b)*M^1
    float jv = Constraints::ComputeJV(mContactBlock.mNormal, mContactBlock.mRCrossNA[i], -mContactBlock.mNormal, mContactBlock.mRCrossNB[i], mBlockObjA, mBlockObjB);
    float lambda = Constraints::ComputeLambda(jv, mContactBlock.mPenetrationBias[i], mContactBlock.mContactMass[i]);
    //Clamp lambda term so contacts don't pull objects back together
    Constraints::ClampLambdaMin(lambda, mContactBlock.mLambdaSum[i], 0.0f);
    Constraints::ApplyImpulse(lambda, mContactBlock.mNormalTMass[0], mContactBlock.mRCrossNATInertia[i], mContactBlock.mNormalTMass[1], mContactBlock.mRCrossNBTInertia[i], mBlockObjA, mBlockObjB);
    return std::abs(lambda);
  }

  float LocalContactConstraint::SolveFriction(int i) {
    //This is supposed to come from the material. I'll do that later
    float frictionCoeff = 0.9f;
    float lambdaLimit = mFrictionBlock.mContactLambdaSum[i]*frictionCoeff;
    float lowerBound = -lambdaLimit;
    float upperBound = lambdaLimit;
    float result = 0.0f;
    if(upperBound < 0.0f)
      std::swap(lowerBound, upperBound);

    //Both axes
    for(int j = 0; j < 2; ++j) {
      FrictionAxisBlock& block = mFrictionBlock.mAxes[j];
      float jv = Constraints::ComputeJV(block.mAxis, block.mRCrossAxisA[i], -block.mAxis, block.mRCrossAxisB[i], mBlockObjA, mBlockObjB);
      //No bias term for friction
      float lambda = Constraints::ComputeLambda(jv, block.mConstraintMass[i]);
      //Clamp lambda term so friction doesn't work harder than normal force
      Constraints::ClampLambda(lambda, block.mLambdaSum[i], lowerBound, upperBound);
      Constraints::ApplyImpulse(lambda, block.mLinearA, block.mAngularA[i], block.mLinearB, block.mAngularB[i], mBlockObjA, mBlockObjB);
      result += std::abs(lambda);
    }
    return result;
  }

  void LocalContactConstraint::Draw() {
    mManifold->Draw();
  }

  float LocalContactConstraint::SSolve(void) {
    SFloats lambdaSum = SLoadAll(&mContactBlock.mLambdaSum.x);
    SFloats linVelA, angVelA, linVelB, angVelB;
    SConstraints::LoadVelocity(*mA, *mB, linVelA, angVelA, linVelB, angVelB);
    SFloats result = SVector3::Zero;

    //Solve friction first because it's less important, and the last constraint wins
    for(int i = 0; i < 4; ++i) {
      if(!mFrictionBlock.mEnforce[i])
        continue;

      //This is supposed to come from the material. I'll do that later
      SAlign float frictionCoeff = 0.9f;
      SFloats fcoeff = SLoadSplat(&frictionCoeff);
      SFloats upperBound = SAbsAll(SMulAll(lambdaSum, fcoeff));

      //Both axes
      for(int j = 0; j < 2; ++j) {
        FrictionAxisBlock& block = mFrictionBlock.mAxes[j];
        SFloats blockAxis = SLoadAll(&block.mAxis.x);
        SFloats jv = SConstraints::ComputeJV(blockAxis, SLoadAll(&block.mRCrossAxisA[i].x), SVector3::Neg(blockAxis), SLoadAll(&block.mRCrossAxisB[i].x), linVelA, angVelA, linVelB, angVelB);
        //No bias term for friction
        SFloats lambda = SConstraints::ComputeLambda(jv, SLoadSplat(&block.mConstraintMass[i]));

        //Clamp lambda term so friction doesn't work harder than normal force
        SFloats sum = SLoadSplat(&block.mLambdaSum[i]);
        SFloats oldSum = sum;
        //Upper bound is all of them, splat the one we care about
        SFloats ubound = SSplatIndex(upperBound, i);
        SConstraints::ClampLambda(lambda, sum, SVector3::Neg(ubound), ubound);

        SAlign Vector3 lamStore;
        SStoreAll(&lamStore.x, sum);
        block.mLambdaSum[i] = lamStore.x;

        SConstraints::ApplyImpulse(lambda, linVelA, angVelA, linVelB, angVelB, SLoadAll(&block.mLinearA.x), SLoadAll(&block.mAngularA[i].x), SLoadAll(&block.mLinearB.x), SLoadAll(&block.mAngularB[i].x));
        result = SAddAll(result, SAbsAll(lambda));
      }
    }

    SFloats normal = SLoadAll(&mContactBlock.mNormal.x);
    for(int i = 0; i < 4; ++i) {
      if(!mContactBlock.mEnforce[i])
        continue;

      //J = jacobian, V = velocity vector, b = bias term, M = Jacobian mass
      //Calculate lambda = -(JV + b)*M^1
      SFloats jv = SConstraints::ComputeJV(normal, SLoadAll(&mContactBlock.mRCrossNA[i].x), SVector3::Neg(normal), SLoadAll(&mContactBlock.mRCrossNB[i].x), linVelA, angVelA, linVelB, angVelB);
      SFloats lambda = SConstraints::ComputeLambda(jv, SLoadSplat(&mContactBlock.mPenetrationBias[i]), SLoadSplat(&mContactBlock.mContactMass[i]));

      //Clamp lambda term so contacts don't pull objects back together
      SFloats sum = SSplatIndex(lambdaSum, i);
      SConstraints::ClampLambdaMin(lambda, sum, SVector3::Zero);

      SAlign Vector3 lamstore;
      SStoreAll(&lamstore.x, sum);
      mContactBlock.mLambdaSum[i] = lamstore.x;

      const ContactBlock& cb = mContactBlock;
      SConstraints::ApplyImpulse(lambda, linVelA, angVelA, linVelB, angVelB,
        SLoadAll(&cb.mNormalTMass[0].x), SLoadAll(&cb.mRCrossNATInertia[i].x), SLoadAll(&cb.mNormalTMass[1].x), SLoadAll(&cb.mRCrossNBTInertia[i].x));

      result = SAddAll(result, SAbsAll(lambda));
    }

    //Store velocities back for other constraints to access
    SConstraints::StoreVelocity(linVelA, angVelA, linVelB, angVelB, *mA, *mB);
    SAlign Vector3 resultStore;
    SStoreAll(&resultStore.x, result);
    return resultStore.x;
  }
}