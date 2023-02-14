#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Profile.h"

namespace {
  template<class RowT>
  auto _unwrapWithOffset(RowT& r, size_t offset) {
    return r.mElements.data() + offset;
  }

  void _copyRange(const float* from, float* to, size_t count) {
    std::memcpy(to, from, sizeof(float)*count);
  };
}

bool SweepNPruneBroadphase::recomputeBoundaries(const float* oldMinAxis, const float* oldMaxAxis, float* newMinAxis, float* newMaxAxis, const float* pos, const SweepNPruneBroadphase::BoundariesConfig& cfg, SweepNPruneBroadphase::NeedsReinsert& needsReinsert) {
  PROFILE_SCOPE("physics", "recomputeBoundaries");
  bool anyNeedsRecompute = false;
  for(size_t i = 0; i < needsReinsert.size(); ++i) {
    //TODO: ispc
    const float minExtent = pos[i] - cfg.mHalfSize;
    const float maxExtent = pos[i] + cfg.mHalfSize;
    //If this is within the threshold of its boundary, compute the new boundary and flag it for update
    if(minExtent - cfg.mResizeThreshold < oldMinAxis[i] || maxExtent + cfg.mResizeThreshold > oldMaxAxis[i]) {
      newMinAxis[i] = minExtent - cfg.mPadding;
      newMaxAxis[i] = maxExtent + cfg.mPadding;
      needsReinsert.mElements[i] = true;
      anyNeedsRecompute = true;
    }
  }
  return anyNeedsRecompute;
}

void SweepNPruneBroadphase::insertRange(size_t begin, size_t count,
  BroadphaseTable& broadphase,
  OldMinX& oldMinX,
  OldMinY& oldMinY,
  OldMaxX& oldMaxX,
  OldMaxY& oldMaxY,
  NewMinX& newMinX,
  NewMinY& newMinY,
  NewMaxX& newMaxX,
  NewMaxY& newMaxY,
  Key& key) {
  PairChanges& changes = std::get<SharedRow<PairChanges>>(broadphase.mRows).at();
  Sweep2D& sweep = std::get<SharedRow<Sweep2D>>(broadphase.mRows).at();

  //Use new boundaries to insert
  SweepNPrune::insertRange(sweep,
    _unwrapWithOffset(newMinX, begin),
    _unwrapWithOffset(newMinY, begin),
    _unwrapWithOffset(newMaxX, begin),
    _unwrapWithOffset(newMaxY, begin),
    _unwrapWithOffset(key, begin),
    changes.mGained,
    count);

  //Store old boundaries
  _copyRange(_unwrapWithOffset(newMinX, begin), _unwrapWithOffset(oldMinX, begin), count);
  _copyRange(_unwrapWithOffset(newMaxX, begin), _unwrapWithOffset(oldMaxX, begin), count);
  _copyRange(_unwrapWithOffset(newMinY, begin), _unwrapWithOffset(oldMinY, begin), count);
  _copyRange(_unwrapWithOffset(newMaxY, begin), _unwrapWithOffset(oldMaxY, begin), count);
}

void SweepNPruneBroadphase::reinsertRange(size_t begin, size_t count,
  BroadphaseTable& broadphase,
  OldMinX& oldMinX,
  OldMinY& oldMinY,
  OldMaxX& oldMaxX,
  OldMaxY& oldMaxY,
  NewMinX& newMinX,
  NewMinY& newMinY,
  NewMaxX& newMaxX,
  NewMaxY& newMaxY,
  Key& key) {
  PairChanges& changes = std::get<SharedRow<PairChanges>>(broadphase.mRows).at();
  Sweep2D& sweep = std::get<SharedRow<Sweep2D>>(broadphase.mRows).at();
  SweepNPrune::reinsertRange(sweep,
    _unwrapWithOffset(oldMinX, begin),
    _unwrapWithOffset(oldMinY, begin),
    _unwrapWithOffset(newMinX, begin),
    _unwrapWithOffset(newMinY, begin),
    _unwrapWithOffset(newMaxX, begin),
    _unwrapWithOffset(newMaxY, begin),
    _unwrapWithOffset(key, begin),
    changes.mGained,
    changes.mLost,
    count);

  _copyRange(_unwrapWithOffset(newMinX, begin), _unwrapWithOffset(oldMinX, begin), count);
  _copyRange(_unwrapWithOffset(newMinY, begin), _unwrapWithOffset(oldMinY, begin), count);
  _copyRange(_unwrapWithOffset(newMaxX, begin), _unwrapWithOffset(oldMaxX, begin), count);
  _copyRange(_unwrapWithOffset(newMaxY, begin), _unwrapWithOffset(oldMaxY, begin), count);
}

void SweepNPruneBroadphase::reinsertRangeAsNeeded(NeedsReinsert& needsReinsert,
  BroadphaseTable& broadphase,
  OldMinX& oldMinX,
  OldMinY& oldMinY,
  OldMaxX& oldMaxX,
  OldMaxY& oldMaxY,
  NewMinX& newMinX,
  NewMinY& newMinY,
  NewMaxX& newMaxX,
  NewMaxY& newMaxY,
  Key& key) {
  PROFILE_SCOPE("physics", "reinsertAsNeeded");
  for(size_t i = 0; i < needsReinsert.size(); ++i) {
    if(needsReinsert.mElements[i]) {
      //TODO: more clever about combining neighbors
      reinsertRange(i, size_t(1), broadphase, oldMinX, oldMinY, oldMaxX, oldMaxY, newMinX, newMinY, newMaxX, newMaxY, key);
      needsReinsert.mElements[i] = false;
    }
  }
}

void _gatherPairsOnAxis(const std::vector<SweepElement>& axis, std::vector<SweepCollisionPair>& gather) {
  //Brute force iterate over each boundary start end pair and find all other overlapping boundaries in it
  //This could be more efficiently done in straight iteration tracking traversed elements but speed is not the objective of this method
  for(size_t i = 0; i < axis.size(); ++i) {
    if(axis[i].isEnd()) {
      continue;
    }
    for(size_t j = i + 1; j < axis.size() && axis[j].getValue() != axis[i].getValue(); ++j) {
      if(axis[j].isStart()) {
        size_t min = axis[i].getValue();
        size_t max = axis[j].getValue();
        if(min > max) {
          std::swap(min, max);
        }
        gather.push_back({ min, max });
      }
    }
  }
}

void SweepNPruneBroadphase::generateCollisionPairs(BroadphaseTable& broadphase, std::vector<SweepCollisionPair>& results) {
  Sweep2D& sweep = std::get<SharedRow<Sweep2D>>(broadphase.mRows).at();
  results.clear();
  _gatherPairsOnAxis(sweep.mX, results);
  _gatherPairsOnAxis(sweep.mY, results);
  std::sort(results.begin(), results.end());
  std::vector<SweepCollisionPair> finalResults;
  finalResults.reserve(results.size());
  //Any that show up twice overlapped on both axes so are colliding, any that didn't can be ignored
  for(size_t i = 0; i + 1 < results.size();) {
    if(results[i] == results[i + 1]) {
      finalResults.push_back(results[i]);
      i += 2;
    }
    else {
      ++i;
    }
  }
  finalResults.swap(results);
}

void SweepNPruneBroadphase::eraseRange(size_t begin, size_t count,
  BroadphaseTable& broadphase,
  OldMinX& oldMinX,
  OldMinY& oldMinY,
  Key& key) {
  PairChanges& changes = std::get<SharedRow<PairChanges>>(broadphase.mRows).at();
  Sweep2D& sweep = std::get<SharedRow<Sweep2D>>(broadphase.mRows).at();
  SweepNPrune::eraseRange(sweep,
    _unwrapWithOffset(oldMinX, begin),
    _unwrapWithOffset(oldMinY, begin),
    _unwrapWithOffset(key, begin),
    changes.mLost,
    count);
}
