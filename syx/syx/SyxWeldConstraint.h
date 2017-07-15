#pragma once
#include "SyxConstraint.h"
#include "SyxSphericalConstraint.h"

namespace Syx {
  SAlign struct FixedAngleBlock {
    void Setup(const ConstraintObjBlock& a, const ConstraintObjBlock& b, const Vec3* basisA, const Vec3* basisB, const Mat3& ia, const Mat3& ib);
    void ApplyImpulse(const Vec3& lambda, ConstraintObjBlock& a, ConstraintObjBlock& b);
    float Solve(ConstraintObjBlock& a, ConstraintObjBlock& b);
    float SSolve(SFloats& linVelA, SFloats& angVelA, SFloats& linVelB, SFloats& angVelB);

    static float sSlop;

    //Cardinal axes, so columns of the inertia tensor
    SAlign Vec3 mAngularMA[3];
    SAlign Vec3 mAngularMB[3];
    SAlign Vec3 mBias;
    SAlign Vec3 mLambdaSum;
    SAlign Mat3 mConstraintMass;
  };

  SAlign class WeldConstraint: public Constraint {
  public:
    friend class LocalWeldConstraint;

    DeclareIntrusiveNode(WeldConstraint);

    WeldConstraint(PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle)
      : Constraint(ConstraintType::Weld, a, b, handle)
      , mLinearWarmStart{0.0f}
      , mAngularWarmStart{0.0f} {
    }

    //Sets the references such that the relative position and orientation will stay as they are now
    void LockRelativeTransform();

  private:
    SAlign Vec3 mAnchorA;
    SAlign Vec3 mAnchorB;
    //Basis vectors to calculate angular error of off these are cardinal x transformed from world to model (no scale)
    SAlign Vec3 mBasisA[2];
    SAlign Vec3 mBasisB[2];
    SAlign Vec3 mLinearWarmStart;
    SAlign Vec3 mAngularWarmStart;
  };

  SAlign class LocalWeldConstraint: public LocalConstraint {
  public:
    void FirstIteration();
    void LastIteration();
    float Solve();
    float SSolve();
    void Draw();

  private:
    SAlign ConstraintObjBlock mBlockObjA;
    SAlign ConstraintObjBlock mBlockObjB;
    SAlign SphericalBlock mLinearBlock;
    SAlign FixedAngleBlock mAngularBlock;
  };
}