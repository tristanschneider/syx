#include "Precompile.h"
#include "SyxNoBroadphase.h"

namespace Syx {
  // Since this isn't a real broadphase the results can all point at the same container
  class SingleContext : public BroadphaseContext {
  public:
    SingleContext(const std::vector<ResultNode>& res)
      : mResult(res) {
    }

    const std::vector<ResultNode>& get() const override {
      return mResult;
    }

    size_t getTypeId() const override {
      const static size_t result = std::hash<std::string>()("SingleContext");
      return result;
    }

    const std::vector<ResultNode>& mResult;
  };

  class PairContext : public BroadphasePairContext {
  public:
    PairContext(const std::vector<std::pair<ResultNode, ResultNode>>& res)
      : mResult(res) {
    }

    const std::vector<std::pair<ResultNode, ResultNode>>& get() const override {
      return mResult;
    }

    size_t getTypeId() const override {
      const static size_t result = std::hash<std::string>()("PairContext");
      return result;
    }

  private:
    const std::vector<std::pair<ResultNode, ResultNode>>& mResult;
  };

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

  void NoBroadphase::queryRaycast(const Vec3&, const Vec3&, BroadphaseContext& context) const {
    // Don't need to update, it's already pointing at the container
  }

  void NoBroadphase::queryVolume(const BoundingVolume&, BroadphaseContext& context) const {
    // Don't need to update, it's already pointing at the container
  }

  std::unique_ptr<BroadphaseContext> NoBroadphase::createHitContext() const {
    return std::make_unique<SingleContext>(mHits);
  }

  std::unique_ptr<BroadphasePairContext> NoBroadphase::createPairContext() const {
    return std::make_unique<PairContext>(mPairs);
  }

  bool NoBroadphase::isValid(const BroadphaseContext& context) const {
    return SingleContext(mHits).getTypeId() == context.getTypeId();
  }

  bool NoBroadphase::isValid(const BroadphasePairContext& context) const {
    return PairContext(mPairs).getTypeId() == context.getTypeId();
  }
}
