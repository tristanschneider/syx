#include "Precompile.h"
#include "SyxWeldConstraint.h"
#include "SyxPhysicsObject.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"

namespace Syx {
  float FixedAngleBlock::sSlop = 0.01f;

  void FixedAngleBlock::setup(const ConstraintObjBlock& a, const ConstraintObjBlock& b, const Vec3* basisA, const Vec3* basisB, const Mat3& ia, const Mat3& ib) {
    //Premultiplied angular portion
    for(int i = 0; i < 3; ++i) {
      mAngularMA[i] = ia.getCol(i);
      mAngularMB[i] = -ib.getCol(i);
    }

    //Angular and linear constraint mass
    //Xia to mean (Inertia of A)*(Cardinal X axis)
    //[Xia.X + Xib.X, Xia.Y + Xib.Y, Xia.Z + Xib.Z]
    //[Yia.X + Yib.X, Yia.Y + Yib.Y, Yia.Z + Yib.Z]
    //[Zia.X + Zib.X, Zia.Y + Zib.Y, Zia.Z + Zib.Z]
    mConstraintMass = Mat3(ia.mbx.x + ib.mbx.x, ia.mbx.y + ib.mbx.y, ia.mbx.z + ib.mbx.z,
                           ia.mby.x + ib.mby.x, ia.mby.y + ib.mby.y, ia.mby.z + ib.mby.z,
                           ia.mbz.x + ib.mbz.x, ia.mbz.y + ib.mbz.y, ia.mbz.z + ib.mbz.z);
    mConstraintMass = mConstraintMass.inverse();

    //Bias calculation
    Mat3 rotA = a.mRot.toMatrix();
    Mat3 rotB = b.mRot.toMatrix();
    Vec3 angularError = Vec3::Zero;
    for(int i = 0; i < 2; ++i) {
      Vec3 worldRefA = rotA * basisA[i];
      Vec3 worldRefB = rotB * basisB[i];
      angularError -= worldRefA.cross(worldRefB);
    }

    float halfAngularSlop = sSlop*0.5f;
    for(int i = 0; i < 3; ++i) {
      mBias[i] = Constraints::computeBias(angularError[i], halfAngularSlop, LocalConstraint::sVelBaumgarteTerm, LocalConstraint::sMaxVelCorrection);
    }
    mLambdaSum = Vec3::Zero;
  }

  void FixedAngleBlock::applyImpulse(const Vec3& lambda, ConstraintObjBlock& a, ConstraintObjBlock& b) {
    for(int i = 0; i < 3; ++i)
      Constraints::applyAngularImpulse(lambda[i], mAngularMA[i], mAngularMB[i], a, b);
    mLambdaSum += lambda;
  }

  float FixedAngleBlock::solve(ConstraintObjBlock& a, ConstraintObjBlock& b) {
    // 3 row jacobian with cardinal x,y,z in angular components simplifies to this
    Vec3 jv = a.mAngVel - b.mAngVel;
    Vec3 lambda = Constraints::computeLambda(jv, mBias, mConstraintMass);
    applyImpulse(lambda, a, b);
    return std::abs(lambda.x) + std::abs(lambda.y) + std::abs(lambda.z);
  }

  float FixedAngleBlock::sSolve(SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB) {
    linVelA=linVelA; angVelA=angVelA; linVelB=linVelB; angVelB=angVelB;
    return 0.0f;
  }

  void WeldConstraint::lockRelativeTransform() {
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
      com.safeDivide(massA + massB);
    }
   mAnchorA = a.worldToModelPoint(com);
   mAnchorB = b.worldToModelPoint(com);

    Mat3 rotA = a.mRot.inversed().toMatrix();
    Mat3 rotB = b.mRot.inversed().toMatrix();
    //Store cardinal X and Y axes in model space of both objects. Could be any 2 orthogonal axes, but these are easiest
    for(int i = 0; i < 2; ++i) {
      mBasisA[i] = rotA.getCol(i);
      mBasisB[i] = rotB.getCol(i);
    }
  }

  void LocalWeldConstraint::firstIteration() {
    WeldConstraint* owner = static_cast<WeldConstraint*>(mOwner);
    Constraints::loadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

    Vec3 worldAnchorA = mA->modelToWorldPoint(owner->mAnchorA);
    Vec3 worldAnchorB = mB->modelToWorldPoint(owner->mAnchorB);
    mLinearBlock.setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, mA->mInvMass, mB->mInvMass, mA->mInertia, mB->mInertia);

    mAngularBlock.setup(mBlockObjA, mBlockObjB, owner->mBasisA, owner->mBasisB, mA->mInertia, mB->mInertia);

    //Warm start
    mLinearBlock.applyImpulse(owner->mLinearWarmStart, mBlockObjA, mBlockObjB);
    mAngularBlock.applyImpulse(owner->mAngularWarmStart, mBlockObjA, mBlockObjB);
    Constraints::storeVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
  }

  void LocalWeldConstraint::lastIteration() {
    WeldConstraint& owner = static_cast<WeldConstraint&>(*mOwner);
    owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
    owner.mAngularWarmStart = mAngularBlock.mLambdaSum;
  }

  float LocalWeldConstraint::solve() {
    Constraints::loadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    float result = mLinearBlock.solve(mBlockObjA, mBlockObjB);
    result += mAngularBlock.solve(mBlockObjA, mBlockObjB);
    Constraints::storeVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    return result;
  }

  float LocalWeldConstraint::sSolve() {
    return solve();
  }

  void LocalWeldConstraint::draw() {
    WeldConstraint* owner = static_cast<WeldConstraint*>(mOwner);
    Vec3 worldAnchorA = mA->modelToWorldPoint(owner->mAnchorA);
    Vec3 worldAnchorB = mB->modelToWorldPoint(owner->mAnchorB);
    DebugDrawer& d = DebugDrawer::get();
    d.setColor(1.0f, 0.0f, 0.0f);
    float ps = 0.1f;
    d.drawPoint(worldAnchorA, ps);
    d.setColor(0.0f, 0.0f, 1.0f);
    d.drawPoint(worldAnchorB, ps);
  }
}