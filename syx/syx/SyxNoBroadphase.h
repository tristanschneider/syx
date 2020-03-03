#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  class NoBroadphase: public Broadphase {
  public:
    Handle insert(const BoundingVolume& obj, void* userdata) override;
    void remove(Handle handle) override;
    void clear() override;

    Handle update(const BoundingVolume& newVol, Handle handle) override;

    void queryPairs(BroadphasePairContext& context) const override;
    void queryRaycast(const Vec3& start, const Vec3& end, BroadphaseContext& context) const override;
    void queryVolume(const BoundingVolume& volume, BroadphaseContext& context) const override;

    std::unique_ptr<BroadphaseContext> createHitContext() const override;
    std::unique_ptr<BroadphasePairContext> createPairContext() const override;
    bool isValid(const BroadphaseContext& conetxt) const override;
    bool isValid(const BroadphasePairContext& context) const override;

  private:
    std::vector<ResultNode> mHits;
    mutable std::vector<std::pair<ResultNode, ResultNode>> mPairs;
  };
}