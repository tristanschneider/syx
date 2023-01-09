#pragma once

#include "Table.h"
#include "TableOperations.h"
#include "SweepNPrune.h"

//SweepNPrune is the base data structure containing no dependencies on table,
//this is the wrapper around it to facilitate use within Simulation
struct SweepNPruneBroadphase {
  //TODO: some of these are only needed temporarily while determining if boundaries need to be updated
  struct OldMinX : Row<float> {};
  struct OldMinY : Row<float> {};
  struct OldMaxX : Row<float> {};
  struct OldMaxY : Row<float> {};
  struct NewMinX : Row<float> {};
  struct NewMinY : Row<float> {};
  struct NewMaxX : Row<float> {};
  struct NewMaxY : Row<float> {};
  struct Key : Row<size_t> {};
  struct NeedsReinsert : Row<bool> {};

  struct CollisionPairMappings {
    std::unordered_map<SweepCollisionPair, size_t> mSweepPairToCollisionTableIndex;
    std::vector<SweepCollisionPair> mCollisionTableIndexToSweepPair;
    std::unordered_map<size_t, size_t> mKeyToTableElementId;
  };
  struct Keygen {
    size_t mNewKey{};
  };
  struct PairChanges {
    std::vector<SweepCollisionPair> mGained;
    std::vector<SweepCollisionPair> mLost;
  };
  using BroadphaseTable = Table<
    SharedRow<Sweep2D>,
    SharedRow<Keygen>,
    SharedRow<PairChanges>,
    SharedRow<CollisionPairMappings>
  >;

  struct BoundariesConfig {
    const static inline float UNIT_CUBE_EXTENTS = std::sqrt(0.5f*0.5f + 0.5f*0.5f);
    //Distance from pos to extent used to compute where this is relative to its boundaries
    float mHalfSize = UNIT_CUBE_EXTENTS;
    //The amount the boundaries are extended past the size when modifying boundaries
    float mPadding = 0.3f;
    //How close an extent is allowed to get to a boundary before it is recomputed
    float mResizeThreshold = 0.0f;
  };

  bool recomputeBoundaries(const float* oldMinAxis, const float* oldMaxAxis,
    float* newMinAxis, float* newMaxAxis,
    const float* pos,
    const BoundariesConfig& cfg,
    NeedsReinsert& needsReinsert);

  void insertRange(size_t tableID, size_t begin, size_t count,
    BroadphaseTable& broadphase,
    OldMinX& oldMinX,
    OldMinY& oldMinY,
    OldMaxX& oldMaxX,
    OldMaxY& oldMaxY,
    NewMinX& newMinX,
    NewMinY& newMinY,
    NewMaxX& newMaxX,
    NewMaxY& newMaxY,
    Key& key);

  void reinsertRange(size_t begin, size_t count,
    BroadphaseTable& broadphase,
    OldMinX& oldMinX,
    OldMinY& oldMinY,
    OldMaxX& oldMaxX,
    OldMaxY& oldMaxY,
    NewMinX& newMinX,
    NewMinY& newMinY,
    NewMaxX& newMaxX,
    NewMaxY& newMaxY,
    Key& key);

  void reinsertRangeAsNeeded(NeedsReinsert& needsReinsert,
    BroadphaseTable& broadphase,
    OldMinX& oldMinX,
    OldMinY& oldMinY,
    OldMaxX& oldMaxX,
    OldMaxY& oldMaxY,
    NewMinX& newMinX,
    NewMinY& newMinY,
    NewMaxX& newMaxX,
    NewMaxY& newMaxY,
    Key& key);

  void eraseRange(size_t begin, size_t count,
    BroadphaseTable& broadphase,
    OldMinX& oldMinX,
    OldMinY& oldMinY,
    Key& key);

  void informObjectMovedTables(CollisionPairMappings& mappings, size_t key, size_t elementID);

  //Create and remove based on the changes in PairChanges
  template<class TableT, class PairIndexA, class PairIndexB>
  void updateCollisionPairs(PairChanges& changes, CollisionPairMappings& mappings, TableT& table) {
    for(SweepCollisionPair loss : changes.mLost) {
      //TODO: order these somewhere else
      if(loss.mA > loss.mB) {
        std::swap(loss.mA, loss.mB);
      }
      if(auto it = mappings.mSweepPairToCollisionTableIndex.find(loss); it != mappings.mSweepPairToCollisionTableIndex.end()) {
        const size_t swappedIndex = TableOperations::size(table) - 1;
        const size_t removedPairIndex = it->second;
        TableOperations::swapRemove(table, removedPairIndex);
        std::swap(mappings.mCollisionTableIndexToSweepPair[removedPairIndex], mappings.mCollisionTableIndexToSweepPair[swappedIndex]);
        mappings.mCollisionTableIndexToSweepPair.pop_back();
        //Remove reference to this index
        mappings.mSweepPairToCollisionTableIndex.erase(it);
        //Update mapping of swap removed element
        mappings.mSweepPairToCollisionTableIndex[mappings.mCollisionTableIndexToSweepPair[removedPairIndex]] = removedPairIndex;
      }
    }

    if(changes.mGained.empty()) {
      return;
    }
    //Resize to fit all the new elements
    const size_t gainBegin = TableOperations::size(table);
    const size_t newSize = gainBegin + changes.mGained.size();
    TableOperations::resizeTable(table, newSize);
    //Should always match the size of the collision table
    mappings.mCollisionTableIndexToSweepPair.resize(newSize);
    auto& pairA = std::get<PairIndexA>(table.mRows);
    auto& pairB = std::get<PairIndexB>(table.mRows);
    for(size_t i = 0; i < changes.mGained.size(); ++i) {
      SweepCollisionPair gain = changes.mGained[i];
      if(gain.mA > gain.mB) {
        std::swap(gain.mA, gain.mB);
      }
      const size_t addIndex = gainBegin + i;
      //Assign mappings so this can be found above in removal
      mappings.mCollisionTableIndexToSweepPair[addIndex] = gain;
      mappings.mSweepPairToCollisionTableIndex[gain] = addIndex;

      //Assign pair indices, the mappings are populated upon insertion and when objects move tables
      auto elementA = mappings.mKeyToTableElementId.find(gain.mA);
      auto elementB = mappings.mKeyToTableElementId.find(gain.mB);
      assert(elementA != mappings.mKeyToTableElementId.end());
      assert(elementB != mappings.mKeyToTableElementId.end());
      if(elementA != mappings.mKeyToTableElementId && elementB != mappings.mKeyToTableElementId.end()) {
        pairA.at(addIndex) = elementA->second;
        pairB.at(addIndex) = elementB->second;
      }
    }
  }
};