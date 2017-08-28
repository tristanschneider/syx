#include "Precompile.h"
#include "SyxNoBroadphase.h"

namespace Syx {
  Handle NoBroadphase::insert(const BoundingVolume&, void* userdata) {
    Handle result = _getNewHandle();
    mResults.push_back(ResultNode(result, userdata));
    return result;
  }

  void NoBroadphase::remove(Handle handle) {
    for(auto it = mResults.begin(); it != mResults.end(); ++it)
      if(it->mHandle == handle) {
        mResults.erase(it);
        return;
      }
  }

  void NoBroadphase::clear(void) {
    mResults.clear();
  }

  //This isn't a real broadphase, so there's nothing to update
  Handle NoBroadphase::update(const BoundingVolume&, Handle handle) {
    return handle;
  }

  void NoBroadphase::queryPairs(BroadphaseContext& context) const {
    context.mQueryPairResults.clear();
    for(size_t i = 0; i + 1 < mResults.size(); ++i)
      for(size_t j = i + 1; j < mResults.size(); ++j)
        context.mQueryPairResults.push_back({mResults[i], mResults[j]});
  }

  void NoBroadphase::queryRaycast(const Vec3&, const Vec3&, BroadphaseContext& context) const {
    context.mQueryResults = mResults;
  }

  void NoBroadphase::queryVolume(const BoundingVolume&, BroadphaseContext& context) const {
    context.mQueryResults = mResults;
  }
}
