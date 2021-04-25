#pragma once
#include "SyxResourceHandle.h"

namespace Syx {
  struct Material;
  class PhysicsSystem;

  struct IMaterialHandle {
    virtual ~IMaterialHandle() = default;
    virtual const Material& get() const = 0;
  };

  struct Material {
    Material() = default;
    Material(float density, float restitution, float friction)
      : mDensity(density)
      , mRestitution(restitution)
      , mFriction(friction) {
    }
    Material(const Material&) = default;
    Material& operator=(const Material&) = default;

    float mDensity = 1.f;
    float mRestitution = 0.f;
    float mFriction = 0.9f;
  };

  struct OwnedMaterial : public EnableDeferredDeletion<OwnedMaterial>, public Material {
    OwnedMaterial(const Material& material, DeferredDeleteResourceHandle<OwnedMaterial>& handle)
      : Material(material)
      , EnableDeferredDeletion(handle) {
    }
  };

  struct MaterialHandle : public IMaterialHandle, public DeferredDeleteResourceHandle<OwnedMaterial> {
    const Material& get() const override {
      return _get();
    }
  };
}