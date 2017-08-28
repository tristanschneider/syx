#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  class NoBroadphase: public Broadphase {
  public:
    Handle insert(const BoundingVolume& obj, void* userdata) override;
    void remove(Handle handle) override;
    void clear(void) override;

    Handle update(const BoundingVolume& newVol, Handle handle) override;

    void queryPairs(BroadphaseContext& context) const override;
    void queryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const override;
    void queryVolume(const BoundingVolume& volume, BroadphaseContext& context) const override;

  private:

    BroadResults mResults;
  };
}