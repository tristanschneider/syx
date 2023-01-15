#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

static std::unordered_set<SweepCollisionPair> trackerHack;

void _validatePairs(SweepNPruneBroadphase::BroadphaseTable& broadphase) {
  std::vector<SweepCollisionPair> results;
  SweepNPruneBroadphase::generateCollisionPairs(broadphase, results);
  std::unordered_set<SweepCollisionPair> s;
  s.insert(results.begin(), results.end());
  if(s != trackerHack) {
    printf("mismatched pairs");
  }
}

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

void SweepNPruneBroadphase::insertRange(size_t tableID, size_t begin, size_t count,
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
  Keygen& keygen = std::get<SharedRow<Keygen>>(broadphase.mRows).at();
  CollisionPairMappings& mappings = std::get<SharedRow<CollisionPairMappings>>(broadphase.mRows).at();
  size_t* keys = _unwrapWithOffset(key, begin);
  //Assign new keys that will be inserted below
  for(size_t i = 0; i < count; ++i) {
    keys[i] = ++keygen.mNewKey;
    //Store the object index for later use in updateCollisionPairs
    mappings.mKeyToTableElementId[keys[i]] = tableID + i;
  }

  _validatePairs(broadphase);

  //Use new boundaries to insert
  SweepNPrune::insertRange(sweep,
    _unwrapWithOffset(newMinX, begin),
    _unwrapWithOffset(newMinY, begin),
    _unwrapWithOffset(newMaxX, begin),
    _unwrapWithOffset(newMaxY, begin),
    _unwrapWithOffset(key, begin),
    changes.mGained,
    count);

  for(auto p : changes.mGained) {
    if(p.mA > p.mB) {
      std::swap(p.mA, p.mB);
    }
    trackerHack.insert(p);
  }
  _validatePairs(broadphase);

  //Store old boundaries
  _copyRange(_unwrapWithOffset(newMinX, begin), _unwrapWithOffset(oldMinX, begin), count);
  _copyRange(_unwrapWithOffset(newMaxX, begin), _unwrapWithOffset(oldMaxX, begin), count);
  _copyRange(_unwrapWithOffset(newMinY, begin), _unwrapWithOffset(oldMinY, begin), count);
  _copyRange(_unwrapWithOffset(newMaxY, begin), _unwrapWithOffset(oldMaxY, begin), count);
}

struct Temp {
  SweepNPruneBroadphase::BroadphaseTable broadphase;
  SweepNPruneBroadphase::OldMinX oldMinX;
  SweepNPruneBroadphase::OldMinY oldMinY;
  SweepNPruneBroadphase::OldMaxX oldMaxX;
  SweepNPruneBroadphase::OldMaxY oldMaxY;
  SweepNPruneBroadphase::NewMinX newMinX;
  SweepNPruneBroadphase::NewMinY newMinY;
  SweepNPruneBroadphase::NewMaxX newMaxX;
  SweepNPruneBroadphase::NewMaxY newMaxY;
  SweepNPruneBroadphase::Key key;
};

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
  Temp old {
    broadphase,
    oldMinX,
    oldMinY,
    oldMaxX,
    oldMaxY,
    newMinX,
    newMinY,
    newMaxX,
    newMaxY,
    key
  };



  PairChanges& changes = std::get<SharedRow<PairChanges>>(broadphase.mRows).at();
  std::vector<SweepCollisionPair> gainedTemp = changes.mGained;
  std::vector<SweepCollisionPair> lostTemp = changes.mLost;
  changes.mGained.clear();
  changes.mLost.clear();


  _validatePairs(broadphase);

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

  bool redo = false;
  for(SweepCollisionPair p : changes.mLost) {
    if(p.mA > p.mB) {
      std::swap(p.mA, p.mB);
    }
    auto l = trackerHack.find(p);
    if(l == trackerHack.end()) {
      if(auto it = std::find(changes.mGained.begin(), changes.mGained.end(), p); it != changes.mGained.end()) {
        changes.mGained.erase(it);
      }
      else {
        redo = true;
        printf("unexpeced loss\n");
      }
    }
    else {
      trackerHack.erase(l);
    }
  }
  for(SweepCollisionPair p : changes.mGained) {
    if(p.mA > p.mB) {
      std::swap(p.mA, p.mB);
    }
    if(trackerHack.find(p) != trackerHack.end()) {
      redo = true;
      printf("unexpected addition\n");
    }
    trackerHack.insert(p);
  }

  _validatePairs(broadphase);

  if(redo) {
    PairChanges& tempchanges = std::get<SharedRow<PairChanges>>(old.broadphase.mRows).at();
    Sweep2D& tempsweep = std::get<SharedRow<Sweep2D>>(old.broadphase.mRows).at();
    SweepNPrune::reinsertRange(tempsweep,
      _unwrapWithOffset(old.oldMinX, begin),
      _unwrapWithOffset(old.oldMinY, begin),
      _unwrapWithOffset(old.newMinX, begin),
      _unwrapWithOffset(old.newMinY, begin),
      _unwrapWithOffset(old.newMaxX, begin),
      _unwrapWithOffset(old.newMaxY, begin),
      _unwrapWithOffset(old.key, begin),
      tempchanges.mGained,
      tempchanges.mLost,
      count);
  }

  gainedTemp.insert(gainedTemp.end(), changes.mGained.begin(), changes.mGained.end());
  lostTemp.insert(lostTemp.end(), changes.mLost.begin(), changes.mLost.end());
  changes.mGained = gainedTemp;
  changes.mLost = lostTemp;

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
  CollisionPairMappings& mappings = std::get<SharedRow<CollisionPairMappings>>(broadphase.mRows).at();
  SweepNPrune::eraseRange(sweep,
    _unwrapWithOffset(oldMinX, begin),
    _unwrapWithOffset(oldMinY, begin),
    _unwrapWithOffset(key, begin),
    changes.mLost,
    count);

  //Remove the mappings to these objects now. Their collision pairs will be removed in the next updateCollisionPairs
  const size_t* keys = _unwrapWithOffset(key, begin);
  for(size_t i = 0; i < count; ++i) {
    if(auto it = mappings.mKeyToTableElementId.find(keys[i]); it != mappings.mKeyToTableElementId.end()) {
      mappings.mKeyToTableElementId.erase(it);
    }
  }
}

void SweepNPruneBroadphase::informObjectMovedTables(CollisionPairMappings& mappings, size_t key, size_t elementID) {
  mappings.mKeyToTableElementId[key] = elementID;
}
