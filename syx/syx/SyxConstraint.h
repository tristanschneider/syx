#pragma once
#include "SyxResourceHandle.h"

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
    SAlign Vec3 mLinearA;
    SAlign Vec3 mLinearB;
    SAlign Vec3 mAngularA;
    SAlign Vec3 mAngularB;
  };

  //Same as jacobian but for constraints where the linear component is the other's but flipped
  SAlign struct JacobianSL {
    SAlign Vec3 mLinear;
    SAlign Vec3 mAngularA;
    SAlign Vec3 mAngularB;
  };

  SAlign struct LocalObject {
    LocalObject();
    LocalObject(PhysicsObject& owner);

    Vec3 modelToWorldPoint(const Vec3& p) const;
    Vec3 modelToWorldVec(const Vec3& v) const;
    Vec3 worldToModelPoint(const Vec3& p) const;
    Vec3 worldToModelVec(const Vec3& v) const;

    SAlign Vec3 mPos;
    SAlign Quat mRot;
    SAlign Vec3 mLinVel;
    SAlign Vec3 mAngVel;
    SAlign Mat3 mInertia;
    SAlign float mInvMass;
    PhysicsObject* mOwner;
    char mPadding[8];
  };

  SAlign struct ConstraintObjBlock {
    ConstraintObjBlock(void) {}
    ConstraintObjBlock(const Vec3& pos, const Quat& rot, const Vec3& linVel, const Vec3& angVel)
      : mPos(pos)
      , mRot(rot)
      , mLinVel(linVel)
      , mAngVel(angVel) {
    }

    void set(const LocalObject& obj);
    void loadVelocity(const LocalObject& obj);
    void storeVelocity(LocalObject& obj) const;

    SAlign Vec3 mPos;
    SAlign Quat mRot;
    SAlign Vec3 mLinVel;
    SAlign Vec3 mAngVel;
  };

  class Constraint {
  public:
    using PhysicsObjectHandle = WeakDeferredDeleteResourceHandle<PhysicsObject>;

    // Updated by solve loop and static for easy access wherever it's needed
    static float sDT;

    Constraint(ConstraintType type, PhysicsObject* a = nullptr, PhysicsObject* b = nullptr, Handle handle = SyxInvalidHandle);

    PhysicsObject* getObjA();
    PhysicsObject* getObjB();
    PhysicsObject* getObj(ConstraintObj obj) {
      return obj == ConstraintObj::A ? getObjA() : getObjB();
    }
    ConstraintType getType() {
      return mType;
    };
    bool shouldRemove() {
      return mShouldRemove;
    }
    Handle getHandle() const {
      return mHandle;
    }
    virtual void setLocalAnchor(const Vec3&, ConstraintObj) {}
    virtual const Vec3& getLocalAnchor(ConstraintObj) const {
      return Vec3::Zero;
    }
    bool getBlacklistCollision() {
      return mBlacklistCollision;
    }
    void setBlacklistCollision(bool val) {
      mBlacklistCollision = val;
    }

  protected:
    //These are objects instead of rigidbodies to support rigidbodyless colliders to act as infinite mass
    WeakDeferredDeleteResourceHandle<PhysicsObject> mA;
    WeakDeferredDeleteResourceHandle<PhysicsObject> mB;
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
      , mOwner(nullptr) {
    }

    void set(LocalObject& a, LocalObject& b, Constraint& owner) {
      mA = &a;
      mB = &b;
      mOwner = &owner;
    }

    Constraint* getOwner() {
      return mOwner;
    }

    virtual void draw() {}

  protected:
    LocalObject* mA;
    LocalObject* mB;
    Constraint* mOwner;
  };
};