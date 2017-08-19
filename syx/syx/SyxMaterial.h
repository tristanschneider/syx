#pragma once

namespace Syx {
  class PhysicsSystem;

  struct Material {
    friend class PhysicsSystem;
    DeclareHandleMapNode(Material);

    Material(float density, float restitution, float friction, Handle handle = SyxInvalidHandle) :
      mDensity(density), mRestitution(restitution), mFriction(friction), mHandle(handle) {
    }

    //Arbitrary default values that probably don't matter because they'll probably be set after the constructor if this is used
    Material(Handle handle) : mHandle(handle), mDensity(1.0f), mRestitution(0.0f), mFriction(0.9f) {}
    Material() : mHandle(SyxInvalidHandle), mDensity(1.0f), mRestitution(0.0f), mFriction(0.9f) {}

    bool operator==(Handle handle) { return mHandle == handle; }
    bool operator<(Handle handle) { return mHandle < handle; }

    float mDensity;
    float mRestitution;
    float mFriction;
    Handle mHandle;
  };
}