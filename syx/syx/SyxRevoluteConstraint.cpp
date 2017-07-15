#include "Precompile.h"
#include "SyxRevoluteConstraint.h"
#include "SyxConstraintMath.h"
#include "SyxSConstraintMath.h"
#include "SyxPhysicsObject.h"

namespace Syx {
  float RevoluteBlock::sSlop = 0.01f;

  void RevoluteBlock::GetFixedErrors(const Vec3* worldBasisB, float* errors) const {
    //We want the basis vectors to be aligned but without causing twist on the free axis, so get the vector orthogonal to both, which is the free axis when there is no error
    //Then we align those for A and B
    Vec3 freeA = mAngular[0].Cross(mAngular[1]);
    Vec3 freeB = worldBasisB[0].Cross(worldBasisB[1]);
    Vec3 errorAxis = freeA.Cross(freeB);
    for(int i = 0; i < 2; ++i)
      errors[i] = -errorAxis.Dot(mAngular[i]);
  }

  float RevoluteBlock::GetFreeError(float lastError, const Vec3& referenceB) {
    return Constraints::ComputeCumulativeAngleError(lastError, mAngular[0], referenceB, mFreeAngular);
  }

  void RevoluteConstraint::SetLocalFreeAxis(const Vec3& axis) {
    Vec3 freeAxis = axis.Normalized();
    freeAxis.GetBasis(mBasisA[0], mBasisA[1]);

    // Transform from local a to world to local b
    Mat3 rb = mB->GetTransform().mRot.Inversed().ToMatrix() * mA->GetTransform().mRot.ToMatrix();
    for(int i = 0; i < 2; ++i)
      mBasisB[i] = rb * mBasisA[i];
  }

  void RevoluteConstraint::SetFreeLimits(float minRads, float maxRads) {
    mMinRads = minRads;
    mMaxRads = maxRads;
  }

  void RevoluteConstraint::GetFreeLimits(float& minRads, float& maxRads) const {
    minRads = mMinRads;
    maxRads = mMaxRads;
  }

  void LocalRevoluteConstraint::FirstIteration() {
    Constraints::LoadObjects(mBlockObjA, mBlockObjB, *mA, *mB);

    const Mat3& ia = mA->mInertia;
    const Mat3& ib = mB->mInertia;
    float ma = mA->mInvMass;
    float mb = mB->mInvMass;

    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    const Transformer& toWorldA = mA->mOwner->GetTransform().GetModelToWorld();
    const Transformer& toWorldB = mB->mOwner->GetTransform().GetModelToWorld();
    Vec3 worldAnchorA = toWorldA.TransformPoint(owner.GetLocalAnchor(ConstraintObj::A));
    Vec3 worldAnchorB = toWorldB.TransformPoint(owner.GetLocalAnchor(ConstraintObj::B));
    mLinearBlock.Setup(mBlockObjA.mPos, mBlockObjB.mPos, worldAnchorA, worldAnchorB, ma, mb, ia, ib);

    Vec3 worldBasisB[2];
    for(int i = 0; i < 2; ++i) {
      Vec3 axis = mBlockObjA.mRot*owner.mBasisA[i];
      mAngularBlock.mAngular[i] = axis;
      mAngularBlock.mAngularMA[i] = ia*axis;
      mAngularBlock.mAngularMB[i] = -(ib*axis);
      worldBasisB[i] = mBlockObjB.mRot*owner.mBasisB[i];
    }

    float error[2];
    mAngularBlock.GetFixedErrors(worldBasisB, error);
    float halfSlop = RevoluteBlock::sSlop*0.5f;
    for(int i = 0; i < 2; ++i)
      mAngularBlock.mBias[i] = Constraints::ComputeBias(error[i], halfSlop, sVelBaumgarteTerm, sMaxVelCorrection);

    const Vec3& x = mAngularBlock.mAngular[0]; const Vec3& y = mAngularBlock.mAngular[1];
    Vec3 xia = ia*x; Vec3 xib = ib*x;
    Vec3 yia = ia*y; Vec3 yib = ib*y;
    mAngularBlock.mConstraintMass = Vec3(xia.Dot(x) + xib.Dot(x), xia.Dot(y) + xib.Dot(y),
                                      yia.Dot(x) + yib.Dot(x), yia.Dot(y) + yib.Dot(y));
    mAngularBlock.mConstraintMass = mAngularBlock.mConstraintMass.Mat2Inversed();

    mLinearBlock.mLambdaSum = Vec3::Zero;
    mLinearBlock.ApplyImpulse(owner.mLinearWarmStart, mBlockObjA, mBlockObjB);

    mAngularBlock.mLambdaSum = owner.mAngularWarmStart;
    for(int i = 0; i < 2; ++i)
      Constraints::ApplyAngularImpulse(mAngularBlock.mLambdaSum[i], mAngularBlock.mAngularMA[i], mAngularBlock.mAngularMB[i], mBlockObjA, mBlockObjB);

    bool enforceFree = owner.mMaxFreeImpulse > 0.0f;
    bool enforceFreeLimits = owner.mMaxRads > owner.mMinRads;
    if(enforceFree || enforceFreeLimits) {
      mAngularBlock.mFreeAngular = mAngularBlock.mAngular[0].Cross(mAngularBlock.mAngular[1]);
      mAngularBlock.mFreeEnforceDir = LocalConstraint::EnforceBoth;

      mAngularBlock.mBias[2] = 0.0f;
      if(enforceFreeLimits) {
        float freeError = mAngularBlock.GetFreeError(owner.GetLastFreeError(), worldBasisB[0]);
        owner.SetLastFreeError(freeError);
        Constraints::ComputeAngularLimitError(owner.mMinRads, owner.mMaxRads, enforceFree, freeError, mAngularBlock.mFreeEnforceDir);

        if(mAngularBlock.mFreeEnforceDir == LocalConstraint::EnforcePos || mAngularBlock.mFreeEnforceDir == LocalConstraint::EnforceNeg)
          mAngularBlock.mBias[2] = -Constraints::ComputeBias(freeError, halfSlop, sVelBaumgarteTerm, sMaxVelCorrection);
      }

      if(mAngularBlock.mFreeEnforceDir != LocalConstraint::NoEnforce) {
        //If user didn't specify a max sum, have no max, meaning max is max float
        mAngularBlock.mFreeMaxSum = owner.mMaxFreeImpulse <= 0.0f ? std::numeric_limits<float>::max() : owner.mMaxFreeImpulse;

        mAngularBlock.mFreeAngularMA = ia*mAngularBlock.mFreeAngular;
        Vec3 freeAngularB = -mAngularBlock.mFreeAngular;
        mAngularBlock.mFreeAngularMB = ib*freeAngularB;

        mAngularBlock.mFreeMass = SafeDivide(1.0f, mAngularBlock.mFreeAngularMA.Dot(mAngularBlock.mFreeAngular) + mAngularBlock.mFreeAngularMB.Dot(freeAngularB), SYX_EPSILON);
        //Lambda is ultimately compared against mFreeMaxSum after being multiplied by mass, so scale our cap by mass so it's consistent
        mAngularBlock.mFreeMaxSum *= mAngularBlock.mFreeMass;

        Constraints::ApplyAngularImpulse(mAngularBlock.mLambdaSum[2], mAngularBlock.mFreeAngularMA, mAngularBlock.mFreeAngularMB, mBlockObjA, mBlockObjB);
      }
    }
    else
      mAngularBlock.mFreeEnforceDir = LocalConstraint::NoEnforce;

    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
  }

  void LocalRevoluteConstraint::LastIteration() {
    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    owner.mAngularWarmStart = mAngularBlock.mLambdaSum;
    owner.mLinearWarmStart = mLinearBlock.mLambdaSum;
  }

  float LocalRevoluteConstraint::Solve() {
    Constraints::LoadVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    float result = mLinearBlock.Solve(mBlockObjA, mBlockObjB);

    if(mAngularBlock.mBias.Length() > 0.1f)
      mAngularBlock.mBias.LeastSignificantAxis();

    Vec3 jv;
    Vec3 angVelDiff = mBlockObjA.mAngVel - mBlockObjB.mAngVel;
    for(int i = 0; i < 2; ++i)
      jv[i] = mAngularBlock.mAngular[i].Dot(angVelDiff);
    Vec3 lambda = Constraints::ComputeLambda(jv, mAngularBlock.mBias*2.0f, mAngularBlock.mConstraintMass);

    for(int i = 0; i < 2; ++i) {
      float l = lambda[i];
      result += std::abs(l);
      mAngularBlock.mLambdaSum[i] += l;
      Constraints::ApplyAngularImpulse(l, mAngularBlock.mAngularMA[i], mAngularBlock.mAngularMB[i], mBlockObjA, mBlockObjB);
    }

    if(mAngularBlock.mFreeEnforceDir != LocalConstraint::NoEnforce) {
      float freeJV = mAngularBlock.mFreeAngular.Dot(mBlockObjA.mAngVel - mBlockObjB.mAngVel);
      float freeLambda = Constraints::ComputeLambda(freeJV, mAngularBlock.mBias[2], mAngularBlock.mFreeMass);
      result += std::abs(freeLambda);

      float minSum, maxSum;
      Constraints::ComputeLambdaBounds(mAngularBlock.mFreeMaxSum, mAngularBlock.mFreeEnforceDir, minSum, maxSum);
      Constraints::ClampLambda(freeLambda, mAngularBlock.mLambdaSum[2], minSum, maxSum);
      Constraints::ApplyAngularImpulse(freeLambda, mAngularBlock.mFreeAngularMA, mAngularBlock.mFreeAngularMB, mBlockObjA, mBlockObjB);
    }

    Constraints::StoreVelocity(mBlockObjA, mBlockObjB, *mA, *mB);
    return result;
  }

  float LocalRevoluteConstraint::SSolve() {
    return Solve();
  }

  void LocalRevoluteConstraint::Draw() {
    RevoluteConstraint& owner = static_cast<RevoluteConstraint&>(*mOwner);
    const Transformer& toWorldA = mA->mOwner->GetTransform().GetModelToWorld();
    const Transformer& toWorldB = mB->mOwner->GetTransform().GetModelToWorld();
    Vec3 worldAnchorA = toWorldA.TransformPoint(owner.GetLocalAnchor(ConstraintObj::A));
    Vec3 worldAnchorB = toWorldB.TransformPoint(owner.GetLocalAnchor(ConstraintObj::B));

    Vec3 worldBasisB[2];
    for(int i = 0; i < 2; ++i)
      worldBasisB[i] = mBlockObjB.mRot*owner.mBasisB[i];

    DebugDrawer& d = DebugDrawer::Get();
    float bs = 0.1f;
    float vs = 0.25f;
    d.SetColor(1.0f, 0.0f, 0.0f);
    DrawCube(worldAnchorA, bs);
    for(int i = 0; i < 2; ++i)
      d.DrawVector(worldAnchorA, mAngularBlock.mAngular[i]*vs);

    d.SetColor(0.0f, 0.0f, 1.0f);
    DrawCube(worldAnchorB, bs);
    for(int i = 0; i < 2; ++i)
      d.DrawVector(worldAnchorB, worldBasisB[i]*vs);

    if(owner.mMinRads < owner.mMaxRads) {
      d.SetColor(0.0f, 1.0f, 0.0f);
      Vec3 free = mAngularBlock.mFreeAngular;
      float minRads, maxRads;
      owner.GetFreeLimits(minRads, maxRads);
      float spiral = 0.05f;
      d.DrawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, owner.mMaxRads, spiral);
      d.DrawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, owner.mMinRads, spiral);

      d.SetColor(1.0f, 1.0f, 1.0f);
      float freeError = mAngularBlock.GetFreeError(owner.GetLastFreeError(), worldBasisB[0]);
      d.DrawArc(worldAnchorB, mAngularBlock.mAngular[0]*vs, free, freeError, spiral);
    }
  }
}