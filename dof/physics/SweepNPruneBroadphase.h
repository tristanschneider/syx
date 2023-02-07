#pragma once

#include <cassert>
#include "PhysicsTableIds.h"
#include "Table.h"
#include "TableOperations.h"
#include "StableElementID.h"
#include "SweepNPrune.h"

//SweepNPrune is the base data structure containing no dependencies on table,
//this is the wrapper around it to facilitate use within Simulation
struct SweepNPruneBroadphase {
  //TODO: some of these are only needed temporarily while determining if boundaries need to be updated
  //It would also be possible to minimize necessary space by using a single point and implied size
  struct OldMinX : Row<float> {};
  struct OldMinY : Row<float> {};
  struct OldMaxX : Row<float> {};
  struct OldMaxY : Row<float> {};
  struct NewMinX : Row<float> {};
  struct NewMinY : Row<float> {};
  struct NewMaxX : Row<float> {};
  struct NewMaxY : Row<float> {};
  using Key = StableIDRow;
  //Byte set to nonzero for true. Hack since std vector bool specialization is tricky with the templates
  struct NeedsReinsert : Row<uint8_t> {};

  struct CollisionPairMappings {
    std::unordered_map<SweepCollisionPair, size_t> mSweepPairToCollisionTableIndex;
    std::vector<SweepCollisionPair> mCollisionTableIndexToSweepPair;
  };
  struct PairChanges {
    //New collision pairs caused by reinserts or inserts
    std::vector<SweepCollisionPair> mGained;
    //Removed collision pairs caused by reinserts or erases
    std::vector<SweepCollisionPair> mLost;
  };
  struct ChangedCollisionPairs {
    std::vector<StableElementID> mGained;
    std::vector<StableElementID> mLost;
  };
  using BroadphaseTable = Table<
    SharedRow<Sweep2D>,
    SharedRow<PairChanges>,
    SharedRow<ChangedCollisionPairs>,
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

  static bool recomputeBoundaries(const float* oldMinAxis, const float* oldMaxAxis,
    float* newMinAxis, float* newMaxAxis,
    const float* pos,
    const BoundariesConfig& cfg,
    NeedsReinsert& needsReinsert);

  static void insertRange(size_t begin, size_t count,
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

  static void reinsertRange(size_t begin, size_t count,
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

  static void reinsertRangeAsNeeded(NeedsReinsert& needsReinsert,
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

  //Iterates over the entire broadphase and generates all pairs. Inefficient, but useful for debugging
  static void generateCollisionPairs(BroadphaseTable& broadphase, std::vector<SweepCollisionPair>& results);

  static void eraseRange(size_t begin, size_t count,
    BroadphaseTable& broadphase,
    OldMinX& oldMinX,
    OldMinY& oldMinY,
    Key& key);

  static std::optional<std::pair<StableElementID, StableElementID>> _tryGetOrderedCollisionPair(const SweepCollisionPair& key, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, bool assertIfMissing) {
    auto elementA = stableMappings.mStableToUnstable.find(key.mA);
    auto elementB = stableMappings.mStableToUnstable.find(key.mB);
    if(assertIfMissing) {
      assert(elementA != stableMappings.mStableToUnstable.end());
      assert(elementB != stableMappings.mStableToUnstable.end());
    }
    if(elementA != stableMappings.mStableToUnstable.end() && elementB != stableMappings.mStableToUnstable.end()) {
      const StableElementID originalA{ elementA->second, elementA->first };
      const StableElementID originalB{ elementB->second, elementB->first };
      auto pair = std::make_pair(originalA, originalB);
      //If this isn't an applicable pair, skip to the next without incrementing addIndex
      if(CollisionPairOrder::tryOrderCollisionPair(pair.first, pair.second, tableIds)) {
        return pair;
      }
    }
    return {};
  }

  //Create and remove based on the changes in PairChanges
  template<class PairIndexA, class PairIndexB, class DatabaseT, class TableT>
  static void updateCollisionPairs(PairChanges& changes, CollisionPairMappings& mappings, TableT& table, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, ChangedCollisionPairs& resultChanges) {
    auto& stableIds = std::get<StableIDRow>(table.mRows);
    //TODO: order these somewhere else
    for(size_t i = 0; i < changes.mGained.size(); ++i) {
      SweepCollisionPair& gain = changes.mGained[i];
      if(gain.mA > gain.mB) {
        std::swap(gain.mA, gain.mB);
      }
    }
    for(SweepCollisionPair loss : changes.mLost) {
      //TODO: order these somewhere else
      if(loss.mA > loss.mB) {
        std::swap(loss.mA, loss.mB);
      }
      if(auto it = mappings.mSweepPairToCollisionTableIndex.find(loss); it != mappings.mSweepPairToCollisionTableIndex.end()) {
        const size_t swappedIndex = TableOperations::size(table) - 1;
        const size_t removedPairIndex = it->second;

        auto removeElement = DatabaseT::template getElementID<TableT>(removedPairIndex);
        const StableElementID lostConstraint = std::get<ConstraintElement>(table.mRows).at(removedPairIndex);
        resultChanges.mLost.push_back(lostConstraint);
        //TODO: manual mappings management isn't really necessary anymore sine the stable row
        TableOperations::stableSwapRemove(table, removeElement, stableMappings);
        std::swap(mappings.mCollisionTableIndexToSweepPair[removedPairIndex], mappings.mCollisionTableIndexToSweepPair[swappedIndex]);
        mappings.mCollisionTableIndexToSweepPair.pop_back();
        //Remove reference to this index
        mappings.mSweepPairToCollisionTableIndex.erase(it);
        //Update mapping of swap removed element. Nothing to do if this was at the end because it was popped off
        if(removedPairIndex < mappings.mCollisionTableIndexToSweepPair.size()) {
          mappings.mSweepPairToCollisionTableIndex[mappings.mCollisionTableIndexToSweepPair[removedPairIndex]] = removedPairIndex;
        }
      }
      //If it was gained and lost on the same frame remove it from the gain list. Presumably infrequent enough to not need faster searching
      else if(auto gained = std::find(changes.mGained.begin(), changes.mGained.end(), loss); gained != changes.mGained.end()) {
        *gained = changes.mGained.back();
        changes.mGained.pop_back();
      }
      else {
        printf("didn't find pair to remove");
        //assert(false);
      }
    }

    changes.mLost.clear();
    if(changes.mGained.empty()) {
      return;
    }
    //Resize to fit all the new elements
    const size_t gainBegin = TableOperations::size(table);
    const size_t newSize = gainBegin + changes.mGained.size();
    constexpr auto tableIndex = UnpackedDatabaseElementID::fromPacked(DatabaseT::template getTableIndex<TableT>());
    TableOperations::stableResizeTable(table, tableIndex, newSize, stableMappings);
    //Should always match the size of the collision table
    mappings.mCollisionTableIndexToSweepPair.resize(newSize);
    auto& pairA = std::get<PairIndexA>(table.mRows);
    auto& pairB = std::get<PairIndexB>(table.mRows);
    size_t addIndex = gainBegin;
    for(size_t i = 0; i < changes.mGained.size(); ++i) {
      SweepCollisionPair gain = changes.mGained[i];

      //Assign pair indices, the mappings are populated upon insertion and when objects move tables
      //If this isn't an applicable pair, skip to the next without incrementing addIndex
      if(auto pair = _tryGetOrderedCollisionPair(gain, tableIds, stableMappings, true)) {
        pairA.at(addIndex) = pair->first;
        pairB.at(addIndex) = pair->second;

        //Assign mappings so this can be found above in removal
        assert(mappings.mSweepPairToCollisionTableIndex.find(gain) == mappings.mSweepPairToCollisionTableIndex.end());
        mappings.mCollisionTableIndexToSweepPair[addIndex] = gain;
        mappings.mSweepPairToCollisionTableIndex[gain] = addIndex;

        auto addElement = DatabaseT::template getElementID<TableT>(addIndex);
        resultChanges.mGained.push_back(StableOperations::getStableID(stableIds, addElement));

        ++addIndex;
      }
    }

    //If less were added than expected, shrink down the extra space
    if(addIndex < newSize) {
      TableOperations::stableResizeTable(table, tableIndex, addIndex, stableMappings);
      mappings.mCollisionTableIndexToSweepPair.resize(addIndex);
    }

    changes.mGained.clear();
  }
};