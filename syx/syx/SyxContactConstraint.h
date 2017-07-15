#pragma once
#include "SyxConstraint.h"
#include "SyxManifold.h"
#include "SyxTransform.h"
#include "SyxPhysicsObject.h"

namespace Syx {
  //Everything needed during solving loop in one compact structure to maximize cache coherence
  SAlign struct ContactBlock {
    //These are the linear and angular terms of the Jacobian that are pre-multiplied with the appropriate masses
    SAlign Vector3 mNormal;
    //Center to contact crossed with normal for object a and b
    SAlign Vector3 mRCrossNA[4];
    SAlign Vector3 mRCrossNB[4];

    //Same terms as above multiplied by the appropriate masses
    SAlign Vector3 mNormalTMass[2];
    SAlign Vector3 mRCrossNATInertia[4];
    SAlign Vector3 mRCrossNBTInertia[4];

    //Penetration bias term ready to be applied straight to the lambda
    SAlign Vector3 mPenetrationBias;
    //Inverse mass of the Jacobians of the contact constraints
    SAlign Vector3 mContactMass;
    //Sum of lambda terms over all iterations this frame
    SAlign Vector3 mLambdaSum;

    //If false this contact is ignored during solving
    bool mEnforce[4];
  };

  SAlign struct FrictionAxisBlock {
    SAlign Vector3 mConstraintMass;
    SAlign Vector3 mLambdaSum;
    SAlign Vector3 mAxis;
    SAlign Vector3 mRCrossAxisA[4];
    SAlign Vector3 mRCrossAxisB[4];

    //Premultiplied linear and angular terms of Jacobian for a and b
    SAlign Vector3 mLinearA;
    SAlign Vector3 mLinearB;
    SAlign Vector3 mAngularA[4];
    SAlign Vector3 mAngularB[4];
  };

  SAlign struct FrictionBlock {
    //Contact constraint's sum is used to determine strength of friction
    SAlign Vector3 mContactLambdaSum;
    SAlign FrictionAxisBlock mAxes[2];
    //If false this contact is ignored during solving
    bool mEnforce[4];
  };

  SAlign class ContactConstraint: public Constraint {
  public:
    friend class LocalContactConstraint;

    DeclareIntrusiveNode(ContactConstraint);

    ContactConstraint(PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle, Handle instA = SyxInvalidHandle, Handle instB = SyxInvalidHandle)
      : Constraint(ConstraintType::Contact, a, b, handle)
      , mManifold(a ? a->GetCollider() : nullptr, b ? b->GetCollider() : nullptr)
      , mInactiveTime(0.0f)
      , mInstA(instA) 
      , mInstB(instB) {
    }

    Handle GetModelInstanceA() { return mInstA; }
    Handle GetModelInstanceB() { return mInstB; }

    SAlign Manifold mManifold;
  private:
    float mInactiveTime;
    //Not used internally, but needed to identify the contact pair this constraint came from
    Handle mInstA;
    Handle mInstB;
  };

  SAlign class LocalContactConstraint: public LocalConstraint {
  public:
    static float sPositionSlop;
    static float sTimeToRemove;

    LocalContactConstraint() {}
    LocalContactConstraint(ContactConstraint& owner);

    void FirstIteration();
    void LastIteration();
    float Solve();
    float SSolve();
    void Draw();

    //Pointer because warm starts are ultimately stored here. Only used in first and last iteration, so cache misses shouldn't hit too hard
    Manifold* mManifold;

  private:
    void SetupContactJacobian(float massA, const Matrix3& inertiaA, float massB, const Matrix3& inertiaB);
    void SetupFrictionJacobian(float massA, const Matrix3& inertiaA, float massB, const Matrix3& inertiaB);
    float SolveContact(int i);
    float SolveFriction(int i);

    SAlign ContactBlock mContactBlock;
    SAlign FrictionBlock mFrictionBlock;
    SAlign ConstraintObjBlock mBlockObjA;
    SAlign ConstraintObjBlock mBlockObjB;
    float* mInactiveTime;
    bool* mShouldRemove;
  };
}