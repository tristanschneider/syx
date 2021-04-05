#pragma once

namespace Syx {
  class PhysicsObject;
  class Space;

  namespace RigidbodyFlags {
    enum {
      LockLinX = 1 << 0,
      LockLinY = 1 << 1,
      LockLinZ = 1 << 2,
      LockAngX = 1 << 3,
      LockAngY = 1 << 4,
      LockAngZ = 1 << 5,
      Kinematic = 1 << 6,
      Disabled = 1 << 7
    };
  }

  SAlign class Rigidbody {
  public:
    friend class PhysicsObject;
    friend class Space;

    Rigidbody(PhysicsObject* owner = nullptr): mOwner(owner), mFlags(0), mLinVel(Vec3::Zero), mAngVel(Vec3::Zero) {}
    // Don't copy because of owner*
    Rigidbody(const Rigidbody&) = delete;
    Rigidbody& operator=(const Rigidbody&) = delete;

    void sCalculateMass(void);

    void updateInertia(void);
    void calculateMass(void);

    void integratePosition(float dt);
    void integrateVelocity(float dt);

    PhysicsObject* getOwner(void);

    void setFlag(int flag, bool value) { setBits(mFlags, flag, value); }
    bool getFlag(int flag) { return (mFlags & flag) != 0; }

    const Mat3& getInertia(void) { return mInvInertia; }
    float getMass(void) { return mInvMass; }

    SAlign Vec3 mLinVel;
    SAlign Vec3 mAngVel;

    Vec3 getGravity();
    Vec3 getUnintegratedLinearVelocity();
    Vec3 getUnintegratedAngularVelocity();

  private:
    SAlign Vec3 mLocalInertia;
    SAlign Mat3 mInvInertia;
    float mInvMass = 1.f;
    int mFlags = 0;

    PhysicsObject* mOwner;
  };
}