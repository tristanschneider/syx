#pragma once
#include "SyxHandles.h"

namespace Syx {
  class ConstraintOptions {
  public:
    ConstraintOptions()
      : mObjA(nullptr)
      , mObjB(nullptr)
      , mA(SyxInvalidHandle)
      , mB(SyxInvalidHandle)
      , mSpace(SyxInvalidHandle)
      , mWorldAnchors(false)
      , mCollisionEnabled(false) {
    }

    void Set(Handle a, Handle b, Handle space) {
      mA = a;
      mB = b;
      mSpace = space;
    }

    void SetWorldAnchor(const Vec3& anchor) {
      mAnchorA = mAnchorB = anchor;
      mWorldAnchors = true;
    }

    void SetAnchors(const Vec3& a, const Vec3& b, bool world) {
      mAnchorA = a;
      mAnchorB = b;
      mWorldAnchors = world;
    }

    Handle mA, mB, mSpace;
    //Will be filled in by the physics system
    PhysicsObject* mObjA;
    PhysicsObject* mObjB;
    Vec3 mAnchorA, mAnchorB;
    bool mWorldAnchors;
    //If collision is enabled between the constrained bodies
    bool mCollisionEnabled;
  };

  class DistanceOps : public ConstraintOptions {
  public:
    DistanceOps()
      : mDistance(0.0f) {
    }

    float mDistance;
  };

  class SphericalOps : public ConstraintOptions {
  public:
    SphericalOps()
      : mMaxSwingRadsX(-1.0f)
      , mMaxSwingRadsY(-1.0f)
      , mMinTwistRads(1.0f)
      , mMaxTwistRads(-1.0f)
      , mMaxAngularImpulse(-1.0f)
      , mSwingFrame(Quat::LookAt(Vec3::UnitZ)) {
    }

    //Axis only needs to be supplied if angular limits are desired
    //Z is swing, x is swingx, y is swingY
    Quat mSwingFrame;
    //< 0 means no limit
    float mMaxSwingRadsX;
    float mMaxSwingRadsY;

    //min>max means no limit
    float mMinTwistRads;
    float mMaxTwistRads;

    //<= 0 means no friction. Friction is for both swing and twist, as limiting one without the other doesn't make sense
    float mMaxAngularImpulse;
  };

  class WeldOps : public ConstraintOptions {
  };

  class RevoluteOps : public ConstraintOptions {
  public:
    RevoluteOps()
      : mMinRads(1.0f)
      , mMaxRads(-1.0f)
      , mMaxFreeImpulse( -1.0f)
      , mFreeAxis(Vec3::UnitX) {
    }

    //min > max means no angular limit
    float mMaxRads;
    float mMinRads;
    //<= 0 means no friction is applied
    float mMaxFreeImpulse;
    Vec3 mFreeAxis;
  };
}