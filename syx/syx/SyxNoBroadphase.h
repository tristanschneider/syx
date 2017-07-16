#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  class NoBroadphase: public Broadphase {
  public:
    Handle Insert(const BoundingVolume& obj, void* userdata) override;
    void Remove(Handle handle) override;
    void Clear(void) override;

    Handle Update(const BoundingVolume& newVol, Handle handle) override;

    void QueryPairs(BroadphaseContext& context) const override;
    void QueryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const override;
    void QueryVolume(const BoundingVolume& volume, BroadphaseContext& context) const override;

  private:
    void RebuildPairs(void);

    BroadResults mResults;
  };
}