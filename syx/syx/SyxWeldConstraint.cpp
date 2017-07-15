#include "Precompile.h"
#include "SyxWeldConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"

namespace Syx {
  float FixedAngleBlock::sSlop = 0.01f;

  void FixedAngleBlock::Setup(const ConstraintObjBlock& a, const ConstraintObjBlock& b, const Vec3* basisA, const Vec3* basisB, const Mat3& ia, const Mat3& ib) {
    //Premultiplied angular portion
    for(int i = 0; i < 3; ++i) {
      mAngularMA[i] = ia.GetCol(i);
      mAngularMB[i] = -ib.GetCol(i);
    }

    //Angular and linear constraint mass
    //Xia to mean (Inertia of A)*(Cardinal X axis)
    //[Xia.X + Xib.X, Xia.Y + Xib.Y, Xia.Z + Xib.Z]
    //[Yia.X + Yib.X, Yia.Y + Yib.Y, Yia.Z + Yib.Z]
    //[Zia.X + Zib.X, Zia.Y + Zib.Y, Zia.Z + Zib.Z]
    mConstraintMass = Mat3(ia.mbx.x + ib.mbx.x, ia.mbx.y + ib.mbx.y, ia.mbx.z + ib.mbx.z,
                           ia.mby.x + ib.mby.x, ia.mby.y + ib.mby.y, ia.mby.z + ib.mby.z,
                           ia.mbz.x + ib.mbz.x, ia.mbz.y + ib.mbz.y, ia.mbz.z + ib.mbz.z);
    mConstraintMass = mConstraintMass.Inverse();

    //Bias calculation
    Mat3 rotA = a.mRot.ToMatrix();
    Mat3 rotB = b.mRot.ToMatrix();
    Vec3 angularError = Vec3::Zero;
    for(int i = 0; i < 2; ++i) {
      Vec3 worldRefA = rotA * basisA[i];
      Vec3 worldRefB = rotB * basisB[i];
      angularError -= worldRefA.Cross(worldRefB);
    }

    float halfAngularSlop = sSlop*0.5f;
    for(int i = 0; i < 3; ++i) {
      mBias[i] = Constraints::ComputeBias(angularError[i], halfAngularSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
    }
    mLambdaSum = Vec3::Zero;
  }

  void FixedAngleBlock::ApplyImpulse(const Vec3& lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
    for(int i = 0; i < 3; ++i)
      Constraints::ApplyAngularImpulse(lambda[i], mAngularMA[i], mAngularMB[i], a, b);
    mLambdaSum += lambda;
  }

  float FixedAngleBlock::Solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
    // 3 row jacobian with cardinal x,y,z in angular components simplifies to this
    Vec3 jv = a.mAngVel - b.mAngVel;
    Vec3 lambda = Constraints::ComputeLambda(jv, mBias, mConstraintMass);
    ApplyImpulse(lambda, a, b);
    return std::abs(lambda.x) + std::abs(lambda.y) + std::abs(lambda.z);
  }

  float FixedAngleBlock::SSolve(SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB) {
    linVelA=linVelA; angVelA=angVelA; linVelB=linVelB; angVelB=angVelB;
    return 0.0f;
  }

  void WeldConstraint::LockRelativeTransform() {
    //Store local translation and orientation differences in local space so we can transform to world to check error
    LocalObject a(*mA);
    LocalObject b(*mB);

    //Use center of mass as the pivot point, as this most accurately simulates a composite object like this
    Vec3 com;
    if(a.mInvMass == 0.0f)
      com = a.mPos;
    else if(b.mInvMass == 0.0f)
      com = b.mPos;
    else {
      float massA = 1.0f/a.mInvMass;
      float massB = 1.0f/b.mInvMass;
      com = a.mPos*massA + b.mPos*massB;
      com.SafeDivide(massA + massB);
    }
   mAnchorA = a.WorldToModelPoint(com);
   mAnchorB = b.WorldToModelPoint(com);

    Mat3 rotA = a.mRot.Inversed().ToMatrix();
    Mat3 rotB = b.mRot.Inversed().ToMatrix();
    //Store cardinal X and Y axes in model space of both objects. Could be any 2 orthogonal axes, but these are easiest
    for(int i = 0; i < 2; ++i) {
      mBasisA[i] = rotA.GetCol(i);
      mBasisB[i] = rotB.GetCol(i);
    }
  }

  void LocalWeldConstraint::FirstIteration() {
    WeldConstraint* owner = static_cast<WeldConstraint*>(mOwner);
    Constraints::LoadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

    Vec3 worldAnchorA = mA->ModelToWorldPoint(owner->mAnchorA);
    Vec3 worldAnchorB = mB->ModelToWorldPoint(owner->mAnchorB);
    mLinearBlock.Setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, mA->mInvMass, mB->mInvMass, mA->mInertia, mB->mInertia);

    mAngularBlock.Setup(mBlockObjA, mBlockObjB, owner->mBasisA, owner->mBasisB, mA->mInertia, mB->mInertia);

    //Warm start
    mLinearBlock.ApplyImpulse(owner->mLinearWarmStart, mBlockObjA, mBlockObjB);
    mAngularBlock.ApplyImpulse(owner->mAngularWarmStart, mBlockObjA, mBlockObjB);
    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
  }

  void LocalWeldConstraint::LastIteration() {
    WeldConstraint& owner = static_cast<WeldConstraint&>(*mOwner);
    owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
    owner.mAngularWarmStart = mAngularBlock.mLambdaSum;
  }

  float LocalWeldConstraint::Solve() {
    Constraints::LoadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    float result = mLinearBlock.Solve(mBlockObjA, mBlockObjB);
    result += mAngularBlock.Solve(mBlockObjA, mBlockObjB);
    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    return result;
  }

  float LocalWeldConstraint::SSolve() {
    return Solve();
  }

  void LocalWeldConstraint::Draw() {
    WeldConstraint* owner = static_cast<WeldConstraint*>(mOwner);
    Vec3 worldAnchorA = mA->ModelToWorldPoint(owner->mAnchorA);
    Vec3 worldAnchorB = mB->ModelToWorldPoint(owner->mAnchorB);
    DebugDrawer& d = DebugDrawer::Get();
    d.SetColor(1.0f, 0.0f, 0.0f);
    float ps = 0.1f;
    d.DrawPoint(worldAnchorA, ps);
    d.SetColor(0.0f, 0.0f, 1.0f);
    d.DrawPoint(worldAnchorB, ps);
  }
}