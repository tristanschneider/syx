#pragma once
#include "SyxConstraint.h"

namespace Syx {
  //Everything needed during solving loop in one compact structure to maximize cache coherence
  SAlign struct DistanceBlock {
    SAlign ConstraintObjBlock mA;
    SAlign ConstraintObjBlock mB;

    //Jacobian, and jacobian multiplied by appropriate masses
    SAlign JacobianSL mJ;
    SAlign Jacobian mJM;

    float mBias;
    float mLambdaSum;
    float mConstraintMass;
  };

  SAlign class DistanceConstraint: public Constraint {
  public:
    friend class LocalDistanceConstraint;

    DeclareIntrusiveNode(DistanceConstraint);

    DistanceConstraint(PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle)
      : Constraint(ConstraintType::Distance, a, b, handle)
      , mWarmStart(0.0f) {
    }

    virtual void SetLocalAnchor(const Vec3& anchor, ConstraintObj obj) override {
      if(obj == ConstraintObj::A)
        mAnchorA = anchor;
      else
        mAnchorB = anchor;
    }
    virtual const Vec3& GetLocalAnchor(ConstraintObj obj) const override {
      return obj == ConstraintObj::A ? mAnchorA : mAnchorB;
    }
    void SetDistance(float dist) {
      mDistance = dist;
    }

  private:
    SAlign Vec3 mAnchorA;
    SAlign Vec3 mAnchorB;
    float mWarmStart;
    float mDistance;
  };

  SAlign class LocalDistanceConstraint: public LocalConstraint {
  public:
    void FirstIteration();
    void LastIteration();
    float Solve();
    float SSolve();
    void Draw();

  private:
    static float sSlop;

    SAlign DistanceBlock mBlock;
  };
}