#pragma once
#include "SyxIntrusive.h"
#include "SyxVector3.h"
#include "SyxQuaternion.h"
#include "SyxSIMD.h"
#include "SyxHandles.h"
#include "SyxMatrix3.h"

namespace Syx {
  class PhysicsObject;

  enum ConstraintType {
    Invalid,
    Contact,
    Distance,
    Spherical,
    Revolute,
    Weld,
  };

  enum ConstraintObj {
    A,
    B
  };

  SAlign struct Jacobian {
    SAlign Vector3 mLinearA;
    SAlign Vector3 mLinearB;
    SAlign Vector3 mAngularA;
    SAlign Vector3 mAngularB;
  };

  //Same as jacobian but for constraints where the linear component is the other's but flipped
  SAlign struct JacobianSL {
    SAlign Vector3 mLinear;
    SAlign Vector3 mAngularA;
    SAlign Vector3 mAngularB;
  };

  SAlign struct LocalObject {
    LocalObject();
    LocalObject(PhysicsObject& owner);

    Vec3 ModelToWorldPoint(const Vec3& p) const;
    Vec3 ModelToWorldVec(const Vec3& v) const;
    Vec3 WorldToModelPoint(const Vec3& p) const;
    Vec3 WorldToModelVec(const Vec3& v) const;

    SAlign Vector3 mPos;
    SAlign Quat mRot;
    SAlign Vector3 mLinVel;
    SAlign Vector3 mAngVel;
    SAlign Matrix3 mInertia;
    SAlign float mInvMass;
    PhysicsObject* mOwner;
    char mPadding[8];
  };

  SAlign struct ConstraintObjBlock {
    ConstraintObjBlock(void) {}
    ConstraintObjBlock(const Vector3& pos, const Quat& rot, const Vector3& linVel, const Vector3& angVel):
      mPos(pos), mRot(rot), mLinVel(linVel), mAngVel(angVel) {
    }

    void Set(const LocalObject& obj);
    void LoadVelocity(const LocalObject& obj);
    void StoreVelocity(LocalObject& obj) const;

    SAlign Vector3 mPos;
    SAlign Quat mRot;
    SAlign Vector3 mLinVel;
    SAlign Vector3 mAngVel;
  };

  class Constraint {
  public:

    // Updated by solve loop and static for easy access wherever it's needed
    static float sDT;

    Constraint(ConstraintType type, PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle)
      : mA(a)
      , mB(b)
      , mShouldRemove(false)
      , mHandle(handle)
      , mType(type)
      , mBlacklistCollision(true) {
    }

    PhysicsObject* GetObjA() {
      return mA;
    }
    PhysicsObject* GetObjB() {
      return mB;
    }
    PhysicsObject* GetObj(ConstraintObj obj) {
      return obj == ConstraintObj::A ? GetObjA() : GetObjB();
    }
    ConstraintType GetType() {
      return mType;
    };
    bool ShouldRemove() {
      return mShouldRemove;
    }
    Handle GetHandle() const {
      return mHandle;
    }
    virtual void SetLocalAnchor(const Vector3&, ConstraintObj) {}
    virtual const Vector3& GetLocalAnchor(ConstraintObj) const {
      return Vector3::Zero;
    }
    bool GetBlacklistCollision() {
      return mBlacklistCollision;
    }
    void SetBlacklistCollision(bool val) {
      mBlacklistCollision = val;
    }

  protected:
    //These are objects instead of rigidbodies to support rigidbodyless colliders to act as infinite mass
    PhysicsObject* mA;
    PhysicsObject* mB;
    bool mShouldRemove;
  private:
    Handle mHandle;
    //Should be const but that deletes copy, which is a drag
    ConstraintType mType;
    bool mBlacklistCollision;
    char mPadding[4];
  };

  //Local copy of Constraint used for cache coherence during solves
  class LocalConstraint {
  public:
    enum EnforceState : char {
      NoEnforce,
      EnforceBoth,
      EnforcePos,
      EnforceNeg
    };

    static const float sVelBaumgarteTerm;
    static const float sMaxVelCorrection;

    LocalConstraint()
      : mA(nullptr)
      , mB(nullptr)
      , mOwner(nullptr) {}

    void Set(LocalObject& a, LocalObject& b, Constraint& owner) {
      mA = &a;
      mB = &b;
      mOwner = &owner;
    }

    Constraint* GetOwner() {
      return mOwner;
    }

    virtual void Draw() {}

  protected:
    LocalObject* mA;
    LocalObject* mB;
    Constraint* mOwner;
  };
};