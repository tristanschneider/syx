#pragma once
#include "SyxConstraint.h"
#include "SyxSphericalConstraint.h"

namespace Syx {
  //Everything needed during solving loop in one compact structure to maximize cache coherence
  SAlign struct RevoluteBlock {
    //Get errors on the two fixed axes
    void getFixedErrors(const Vec3* worldBasisB, float* errors) const;
    float getFreeError(float lastError, const Vec3& referenceB);

    static float sSlop;

    SAlign Vec3 mAngular[2];
    SAlign Vec3 mFreeAngular;
    SAlign Vec3 mFreeAngularMA;
    SAlign Vec3 mFreeAngularMB;
    SAlign Vec3 mAngularMA[2];
    SAlign Vec3 mAngularMB[2];
    SAlign Vec3 mBias;
    SAlign Vec3 mLambdaSum;
    //2x2 mass matrix stored in vector
    SAlign Vec3 mConstraintMass;
    float mFreeMaxSum;
    float mFreeMass;
    char mFreeEnforceDir;
    char mPadding[7];
  };

  SAlign class RevoluteConstraint: public Constraint {
  public:
    friend class LocalRevoluteConstraint;

    DeclareIntrusiveNode(RevoluteConstraint);

    RevoluteConstraint(PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle)
      : Constraint(ConstraintType::Revolute, a, b, handle)
      , mLastFreeError(0.0f) {
    }

    virtual void setLocalAnchor(const Vec3& anchor, ConstraintObj obj) override {
      if(obj == ConstraintObj::A)
        mAnchorA = anchor;
      else
        mAnchorB = anchor;
    }
    virtual const Vec3& getLocalAnchor(ConstraintObj obj) const override {
      return obj == ConstraintObj::A ? mAnchorA : mAnchorB;
    }
    void setMaxFreeImpulse(float max) {
      mMaxFreeImpulse = max;
    }
    void setLastFreeError(float error) {
      mLastFreeError = error;
    }
    float getLastFreeError() {
      return mLastFreeError;
    }
    void setFreeLimits(float minRads, float maxRads);
    void getFreeLimits(float& minRads, float& maxRads) const;
    void setLocalFreeAxis(const Vec3& axis);

  private:
    SAlign Vec3 mAnchorA;
    SAlign Vec3 mAnchorB;
    //Basis vectors to calculate angular error of off these are cardinal x transformed from world to model (no scale)
    SAlign Vec3 mBasisA[2];
    SAlign Vec3 mBasisB[2];
    SAlign Vec3 mLinearWarmStart;
    SAlign Vec3 mAngularWarmStart;
    float mMinRads;
    float mMaxRads;
    float mMaxFreeImpulse;
    float mLastFreeError;
  };

  SAlign class LocalRevoluteConstraint: public LocalConstraint {
  public:
    void firstIteration();
    void lastIteration();
    float solve();
    float sSolve();
    void draw();

  private:
    SAlign ConstraintObjBlock mBlockObjA;
    SAlign ConstraintObjBlock mBlockObjB;
    SAlign SphericalBlock mLinearBlock;
    SAlign RevoluteBlock mAngularBlock;
  };
}