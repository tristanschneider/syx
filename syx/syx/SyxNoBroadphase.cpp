#include "Precompile.h"
#include "SyxNoBroadphase.h"

namespace Syx {
  class ContextBase {
  public:
    ContextBase(const NoBroadphase& broadphase, std::weak_ptr<bool> existenceTracker)
      : mBroadphase(broadphase)
      , mExistenceTracker(std::move(existenceTracker)) {
    }

    const NoBroadphase& mBroadphase;
    std::weak_ptr<bool> mExistenceTracker;
  };

  // Since this isn't a real broadphase the results can all point at the same container
  class SingleContext : public BroadphaseContext, public ContextBase {
  public:
    SingleContext(const NoBroadphase& broadphase, std::weak_ptr<bool> existenceTracker, const std::vector<ResultNode>& res)
      : ContextBase(broadphase, std::move(existenceTracker))
      , mResult(res) {
    }

    void queryRaycast(const Vec3& start, const Vec3& end) override {
      if(auto e = mExistenceTracker.lock()) {
        mBroadphase.queryRaycast(start, end, *this);
      }
    }

    void queryVolume(const BoundingVolume& volume) override {
      if(auto e = mExistenceTracker.lock()) {
        mBroadphase.queryVolume(volume, *this);
      }
    }

    const std::vector<ResultNode>& get() const override {
      return mResult;
    }

    const std::vector<ResultNode>& mResult;
  };

  class PairContext : public BroadphasePairContext, public ContextBase {
  public:
    PairContext(const NoBroadphase& broadphase, std::weak_ptr<bool> existenceTracker, const std::vector<std::pair<ResultNode, ResultNode>>& res)
      : ContextBase(broadphase, std::move(existenceTracker))
      , mResult(res) {
    }

    void queryPairs() override {
      if(auto e = mExistenceTracker.lock()) {
        mBroadphase.queryPairs(*this);
      }
    }

    const std::vector<std::pair<ResultNode, ResultNode>>& get() const override {
      return mResult;
    }

  private:
    const std::vector<std::pair<ResultNode, ResultNode>>& mResult;
  };

  NoBroadphase::NoBroadphase()
    : mExistenceTracker(std::make_shared<bool>()) {
  }

  NoBroadphase::~NoBroadphase() {
    while(mExistenceTracker.use_count() > 1) {
      std::this_thread::yield();
    }
  }

  Handle NoBroadphase::insert(const BoundingVolume&, void* userdata) {
    Handle result = _getNewHandle();
    mHits.push_back(ResultNode(result, userdata));
    return result;
  }

  void NoBroadphase::remove(Handle handle) {
    if (auto it = std::find_if(mHits.begin(), mHits.end(), [handle](const ResultNode& n) { return n.mHandle == handle; }); it != mHits.end()) {
      mHits.erase(it);
    }
  }

  void NoBroadphase::clear() {
    mHits.clear();
    mPairs.clear();
  }

  //This isn't a real broadphase, so there's nothing to update
  Handle NoBroadphase::update(const BoundingVolume&, Handle handle) {
    return handle;
  }

  //Contexts are all just pointing at the internal container so just need to update it
  void NoBroadphase::queryPairs(BroadphasePairContext&) const {
    mPairs.clear();
    for(size_t i = 0; i + 1 < mHits.size(); ++i)
      for(size_t j = i + 1; j < mHits.size(); ++j)
        mPairs.push_back({mHits[i], mHits[j]});
  }

  void NoBroadphase::queryRaycast(const Vec3&, const Vec3&, BroadphaseContext&) const {
    // Don't need to update, it's already pointing at the container
  }

  void NoBroadphase::queryVolume(const BoundingVolume&, BroadphaseContext&) const {
    // Don't need to update, it's already pointing at the container
  }

  std::unique_ptr<BroadphaseContext> NoBroadphase::createHitContext() const {
    return std::make_unique<SingleContext>(*this, mExistenceTracker, mHits);
  }

  std::unique_ptr<BroadphasePairContext> NoBroadphase::createPairContext() const {
    return std::make_unique<PairContext>(*this, mExistenceTracker, mPairs);
  }
}
