#include "Precompile.h"
#include "SyxRevoluteConstraint.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"
#include "SyxPhysicsObject.h"

namespace Syx {
  float RevoluteBlock::sSlop = 0.01f;

  void RevoluteBlock::getFixedErrors(const Vec3* worldBasisB, float* errors) const {
    //We want the basis vectors to be aligned but without causing twist on the free axis, so get the vector orthogonal to both, which is the free axis when there is no error
    //Then we align those for A and B
    Vec3 freeA = mAngular[0].cross(mAngular[1]);
    Vec3 freeB = worldBasisB[0].cross(worldBasisB[1]);
    Vec3 errorAxis = freeA.cross(freeB);
    for(int i = 0; i < 2; ++i)
      errors[i] = -errorAxis.dot(mAngular[i]);
  }

  float RevoluteBlock::getFreeError(float lastError, const Vec3& referenceB) {
    return Constraints::computeCumulativeAngleError(lastError, mAngular[0], referenceB, mFreeAngular);
  }

  void RevoluteConstraint::setLocalFreeAxis(const Vec3& axis) {
    Vec3 freeAxis = axis.normalized();
    freeAxis.getBasis(mBasisA[0], mBasisA[1]);

    // Transform from local a to world to local b
    Mat3 rb = mB->getTransform().mRot.inversed().toMatrix() * mA->getTransform().mRot.toMatrix();
    for(int i = 0; i < 2; ++i)
      mBasisB[i] = rb * mBasisA[i];
  }

  void RevoluteConstraint::setFreeLimits(float minRads, float maxRads) {
    mMinRads = minRads;
    mMaxRads = maxRads;
  }

  void RevoluteConstraint::getFreeLimits(float& minRads, float& maxRads) const {
    minRads = mMinRads;
    maxRads = mMaxRads;
  }

  void LocalRevoluteConstraint::firstIteration() {
    Constraints::loadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

    const Mat3& ia = mA->mInertia;
    const Mat3& ib = mB->mInertia;
    float ma = mA->mInvMass;
    float mb = mB->mInvMass;

    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    const Transformer& toWorldA = mA->mOwner->getTransform().getModelToWorld();
    const Transformer& toWorldB = mB->mOwner->getTransform().getModelToWorld();
    Vec3 worldAnchorA = toWorldA.transformPoint(owner.getLocalAnchor(ConstraintObj::A));
    Vec3 worldAnchorB = toWorldB.transformPoint(owner.getLocalAnchor(ConstraintObj::B));
    mLinearBlock.setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, ma, mb, ia, ib);

    Vec3 worldBasisB[2];
    for(int i = 0; i < 2; ++i) {
      Vec3 axis = mBlockObjA.mRot*owner.mBasisA[i];
      mAngularBlock.mAngular[i] = axis;
      mAngularBlock.mAngularMA[i] = ia*axis;
      mAngularBlock.mAngularMB[i] = -(ib*axis);
      worldBasisB[i] = mBlockObjB.mRot*owner.mBasisB[i];
    }

    float error[2];
    mAngularBlock.getFixedErrors(worldBasisB, error);
    float halfSlop = RevoluteBlock::sSlop*0.5f;
    for(int i = 0; i < 2; ++i)
      mAngularBlock.mBias[i] = Constraints::computeBias(error[i], halfSlop, sVelBaumgarteTerm, sMaxVelCorrection);

    const Vec3& x = mAngularBlock.mAngular[0]; const Vec3& y = mAngularBlock.mAngular[1];
    Vec3 xia = ia*x; Vec3 xib = ib*x;
    Vec3 yia = ia*y; Vec3 yib = ib*y;
    mAngularBlock.mConstraintMass = Vec3(xia.dot(x) + xib.dot(x), xia.dot(y) + xib.dot(y),
                                      yia.dot(x) + yib.dot(x), yia.dot(y) + yib.dot(y));
    mAngularBlock.mConstraintMass = mAngularBlock.mConstraintMass.mat2Inversed();

    mLinearBlock.mLambdaSum = Vec3::Zero;
    mLinearBlock.applyImpulse(owner.mLinearWarmStart, mBlockObjA, mBlockObjB);

    mAngularBlock.mLambdaSum = owner.mAngularWarmStart;
    for(int i = 0; i < 2; ++i)
      Constraints::applyAngularImpulse(mAngularBlock.mLambdaSum[i], mAngularBlock.mAngularMA[i], mAngularBlock.mAngularMB[i], mBlockObjA, mBlockObjB);

    bool enforceFree = owner.mMaxFreeImpulse > 0.0f;
    bool enforceFreeLimits = owner.mMaxRads > owner.mMinRads;
    if(enforceFree || enforceFreeLimits) {
      mAngularBlock.mFreeAngular = mAngularBlock.mAngular[0].cross(mAngularBlock.mAngular[1]);
      mAngularBlock.mFreeEnforceDir = LocalConstraint::EnforceBoth;

      mAngularBlock.mBias[2] = 0.0f;
      if(enforceFreeLimits) {
        float freeError = mAngularBlock.getFreeError(owner.getLastFreeError(), worldBasisB[0]);
        owner.setLastFreeError(freeError);
        Constraints::computeAngularLimitError(owner.mMinRads, owner.mMaxRads, enforceFree, freeError, mAngularBlock.mFreeEnforceDir);

        if(mAngularBlock.mFreeEnforceDir == LocalConstraint::EnforcePos || mAngularBlock.mFreeEnforceDir == LocalConstraint::EnforceNeg)
          mAngularBlock.mBias[2] = -Constraints::computeBias(freeError, halfSlop, sVelBaumgarteTerm, sMaxVelCorrection);
      }

      if(mAngularBlock.mFreeEnforceDir != LocalConstraint::NoEnforce) {
        //If user didn't specify a max sum, have no max, meaning max is max float
        mAngularBlock.mFreeMaxSum = owner.mMaxFreeImpulse <= 0.0f ? std::numeric_limits<float>::max() : owner.mMaxFreeImpulse;

        mAngularBlock.mFreeAngularMA = ia*mAngularBlock.mFreeAngular;
        Vec3 freeAngularB = -mAngularBlock.mFreeAngular;
        mAngularBlock.mFreeAngularMB = ib*freeAngularB;

        mAngularBlock.mFreeMass = safeDivide(1.0f, mAngularBlock.mFreeAngularMA.dot(mAngularBlock.mFreeAngular) + mAngularBlock.mFreeAngularMB.dot(freeAngularB), SYX_EPSILON);
        //Lambda is ultimately compared against mFreeMaxSum after being multiplied by mass, so scale our cap by mass so it's consistent
        mAngularBlock.mFreeMaxSum *= mAngularBlock.mFreeMass;

        Constraints::applyAngularImpulse(mAngularBlock.mLambdaSum[2], mAngularBlock.mFreeAngularMA, mAngularBlock.mFreeAngularMB, mBlockObjA, mBlockObjB);
      }
    }
    else
      mAngularBlock.mFreeEnforceDir = LocalConstraint::NoEnforce;

    Constraints::storeVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
  }

  void LocalRevoluteConstraint::lastIteration() {
    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    owner.mAngularWarmStart = mAngularBlock.mLambdaSum;
    owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
  }

  float LocalRevoluteConstraint::solve() {
    Constraints::loadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    float result = mLinearBlock.solve(mBlockObjA, mBlockObjB);

    if(mAngularBlock.mBias.length() > 0.1f)
      mAngularBlock.mBias.leastSignificantAxis();

    Vec3 jv;
    Vec3 angVelDiff = mBlockObjA.mAngVel - mBlockObjB.mAngVel;
    for(int i = 0; i < 2; ++i)
      jv[i] = mAngularBlock.mAngular[i].dot(angVelDiff);
    Vec3 lambda = Constraints::computeLambda(jv, mAngularBlock.mBias*2.0f, mAngularBlock.mConstraintMass);

    for(int i = 0; i < 2; ++i) {
      float l = lambda[i];
      result += std::abs(l);
      mAngularBlock.mLambdaSum[i] += l;
      Constraints::applyAngularImpulse(l, mAngularBlock.mAngularMA[i], mAngularBlock.mAngularMB[i], mBlockObjA, mBlockObjB);
    }

    if(mAngularBlock.mFreeEnforceDir != LocalConstraint::NoEnforce) {
      float freeJV = mAngularBlock.mFreeAngular.dot(mBlockObjA.mAngVel - mBlockObjB.mAngVel);
      float freeLambda = Constraints::computeLambda(freeJV, mAngularBlock.mBias[2], mAngularBlock.mFreeMass);
      result += std::abs(freeLambda);

      float minSum, maxSum;
      Constraints::computeLambdaBounds(mAngularBlock.mFreeMaxSum, mAngularBlock.mFreeEnforceDir, minSum, maxSum);
      Constraints::clampLambda(freeLambda, mAngularBlock.mLambdaSum[2], minSum, maxSum);
      Constraints::applyAngularImpulse(freeLambda, mAngularBlock.mFreeAngularMA, mAngularBlock.mFreeAngularMB, mBlockObjA, mBlockObjB);
    }

    Constraints::storeVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    return result;
  }

  float LocalRevoluteConstraint::sSolve() {
    return solve();
  }

  void LocalRevoluteConstraint::draw() {
    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    const Transformer& toWorldA = mA->mOwner->getTransform().getModelToWorld();
    const Transformer& toWorldB = mB->mOwner->getTransform().getModelToWorld();
    Vec3 worldAnchorA = toWorldA.transformPoint(owner.getLocalAnchor(ConstraintObj::A));
    Vec3 worldAnchorB = toWorldB.transformPoint(owner.getLocalAnchor(ConstraintObj::B));

    Vec3 worldBasisB[2];
    for(int i = 0; i < 2; ++i)
      worldBasisB[i] = mBlockObjB.mRot*owner.mBasisB[i];

    DebugDrawer& d = DebugDrawer::get();
    float bs = 0.1f;
    float vs = 0.25f;
    d.setColor(1.0f, 0.0f, 0.0f);
    drawCube(worldAnchorA, bs);
    for(int i = 0; i < 2; ++i)
      d.drawVector(worldAnchorA, mAngularBlock.mAngular[i]*vs);

    d.setColor(0.0f, 0.0f, 1.0f);
    drawCube(worldAnchorB, bs);
    for(int i = 0; i < 2; ++i)
      d.drawVector(worldAnchorB, worldBasisB[i]*vs);

    if(owner.mMinRads < owner.mMaxRads) {
      d.setColor(0.0f, 1.0f, 0.0f);
      Vec3 free = mAngularBlock.mFreeAngular;
      float minRads, maxRads;
      owner.getFreeLimits(minRads, maxRads);
      float spiral = 0.05f;
      d.drawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, owner.mMaxRads, spiral);
      d.drawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, owner.mMinRads, spiral);

      d.setColor(1.0f, 1.0f, 1.0f);
      float freeError = mAngularBlock.getFreeError(owner.getLastFreeError(), worldBasisB[0]);
      d.drawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, freeError, spiral);
    }
  }
}