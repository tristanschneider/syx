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

    Rigidbody(PhysicsObject* owner = nullptr): mOwner(owner), mFlags(0), mLinVel(Vector3::Zero), mAngVel(Vector3::Zero) {}
    // Don't copy because of owner*
    Rigidbody(const Rigidbody&) = delete;
    Rigidbody& operator=(const Rigidbody&) = delete;

    void SCalculateMass(void);

    void UpdateInertia(void);
    void CalculateMass(void);

    void IntegratePosition(float dt);
    void IntegrateVelocity(float dt);

    PhysicsObject* GetOwner(void);

    void SetFlag(int flag, bool value) { SetBits(mFlags, flag, value); }
    bool GetFlag(int flag) { return (mFlags & flag) != 0; }

    const Matrix3& GetInertia(void) { return mInvInertia; }
    float GetMass(void) { return mInvMass; }

    SAlign Vector3 mLinVel;
    SAlign Vector3 mAngVel;

    Vector3 GetGravity();
    Vector3 GetUnintegratedLinearVelocity();
    Vector3 GetUnintegratedAngularVelocity();

  private:
    SAlign Vector3 mLocalInertia;
    SAlign Matrix3 mInvInertia;
    float mInvMass;
    int mFlags;

    PhysicsObject* mOwner;
  };
}