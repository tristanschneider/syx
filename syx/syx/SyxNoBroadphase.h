#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  class NoBroadphase: public Broadphase {
  public:
    NoBroadphase();
    ~NoBroadphase();

    Handle insert(const BoundingVolume& obj, void* userdata) override;
    void remove(Handle handle) override;
    void clear() override;

    Handle update(const BoundingVolume& newVol, Handle handle) override;

    void queryPairs(BroadphasePairContext& context) const;
    void queryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const;
    void queryVolume(const BoundingVolume& volume, BroadphaseContext& context) const;

    std::unique_ptr<BroadphaseContext> createHitContext() const override;
    std::unique_ptr<BroadphasePairContext> createPairContext() const override;

  private:
    std::vector<ResultNode> mHits;
    mutable std::vector<std::pair<ResultNode, ResultNode>> mPairs;
    std::shared_ptr<bool> mExistenceTracker;
  };
}