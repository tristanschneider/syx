#pragma once

#include <cassert>
#include "Profile.h"
#include "PhysicsTableIds.h"
#include "Table.h"
#include "TableOperations.h"
#include "StableElementID.h"
#include "SweepNPrune.h"
#include "Scheduler.h"

//SweepNPrune is the base data structure containing no dependencies on table,
//this is the wrapper around it to facilitate use within Simulation
namespace SweepNPruneBroadphase {
  using Key = StableIDRow;
  struct BroadphaseKeys : Row<Broadphase::BroadphaseKey> {};

  struct CollisionPairMappings {
    std::unordered_map<Broadphase::SweepCollisionPair, size_t> mSweepPairToCollisionTableIndex;
    std::vector<Broadphase::SweepCollisionPair> mCollisionTableIndexToSweepPair;
  };
  struct PairChanges {
    //New collision pairs caused by reinserts or inserts
    std::vector<Broadphase::SweepCollisionPair> mGained;
    //Removed collision pairs caused by reinserts or erases
    std::vector<Broadphase::SweepCollisionPair> mLost;
  };
  struct ChangedCollisionPairs {
    std::vector<StableElementID> mGained;
    std::vector<StableElementID> mLost;
  };
  using BroadphaseTable = Table<
    SharedRow<Broadphase::SweepGrid::Grid>,
    SharedRow<PairChanges>,
    SharedRow<ChangedCollisionPairs>,
    SharedRow<CollisionPairMappings>
  >;

  struct BoundariesConfig {
    const static inline float UNIT_CUBE_EXTENTS = std::sqrt(0.5f*0.5f + 0.5f*0.5f);
    //Distance from pos to extent used to compute where this is relative to its boundaries
    float mHalfSize = UNIT_CUBE_EXTENTS;
    //The amount the boundaries are extended past the size when modifying boundaries
    float mPadding = 0.0f;
  };

  //Update boundaries for existing elements
  struct BoundariesQuery {
    const Row<float>* posX{};
    const Row<float>* posY{};
    const BroadphaseKeys* keys{};
  };
  TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<BoundariesQuery> query, const BoundariesConfig& cfg);
  //Compute collision pair changes
  TaskRange computeCollisionPairs(BroadphaseTable& broadphase);

  //New elements are added to the broadphase if they have a broadphase key row
  //Removed elements are removed from the broadphase
  //Moved elements are given one final bounds update if they moved to an immobile table, otherwise ignored
  template<class PosX, class PosY, class Immobile>
  void processEvents(const DBEvents& events, Broadphase::SweepGrid::Grid& grid, TableResolver<PosX, PosY, BroadphaseKeys, StableIDRow, Immobile> resolver, const BoundariesConfig& cfg, const DatabaseDescription& desc) {
    //Insert new elements
    for(const StableElementID& id : events.newElements) {
      const auto unpacked = id.toUnpacked(desc);
      auto* keys = resolver.tryGetRow<BroadphaseKeys>(unpacked);
      auto* stable = resolver.tryGetRow<StableIDRow>(unpacked);
      if(keys && stable) {
        Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
        Broadphase::UserKey userKey = stable->at(unpacked.getElementIndex());
        Broadphase::SweepGrid::insertRange(grid, &userKey, &key, 1);
      }
    }
    //Bounds update elementst that moved to an immobile row
    for(const StableElementID& id : events.movedElements) {
      const auto unpacked = id.toUnpacked(desc);
      if(resolver.tryGetRow<Immobile>(unpacked)) {
        auto posX = resolver.tryGetRow<PosX>(unpacked);
        auto posY = resolver.tryGetRow<PosY>(unpacked);
        auto keys = resolver.tryGetRow<BroadphaseKeys>(unpacked);
        if(posX && posY) {
          const float halfSize = cfg.mHalfSize + cfg.mPadding;
          const size_t i = unpacked.getElementIndex();
          glm::vec2 min{ posX->at(i) - halfSize, posY->at(i) - halfSize };
          glm::vec2 max{ posX->at(i) + halfSize, posY->at(i) + halfSize };
          auto key = keys->at(i);
          Broadphase::SweepGrid::updateBoundaries(grid, &min.x, &max.x, &min.y, &max.y, &key, 1);
        }
      }
    }
    //Remove elements that are about to be destroyed
    for(const StableElementID& id : events.toBeRemovedElements) {
      const auto unpacked = id.toUnpacked(desc);
      if(auto keys = resolver.tryGetRow<BroadphaseKeys>(unpacked)) {
        Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
        Broadphase::SweepGrid::eraseRange(grid, &key, 1);
        key = {};
      }
    }
  }

  std::optional<std::pair<StableElementID, StableElementID>> _tryGetOrderedCollisionPair(const Broadphase::SweepCollisionPair& key, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, bool assertIfMissing);

  //Turn collision pair changes into collision table entries
  //Create and remove based on the changes in PairChanges
  template<class PairIndexA, class PairIndexB, class DatabaseT, class TableT>
  void updateCollisionPairs(PairChanges& changes, CollisionPairMappings& mappings, TableT& table, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, ChangedCollisionPairs& resultChanges) {
    PROFILE_SCOPE("physics", "updateCollisionPairs");
    auto& stableIds = std::get<StableIDRow>(table.mRows);
    {
      PROFILE_SCOPE("physics", "losses");
      for(Broadphase::SweepCollisionPair loss : changes.mLost) {
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
      }
      changes.mLost.clear();
    }

    {
      PROFILE_SCOPE("physics", "gains");
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
        Broadphase::SweepCollisionPair gain = changes.mGained[i];
        if(mappings.mSweepPairToCollisionTableIndex.find(gain) != mappings.mSweepPairToCollisionTableIndex.end()) {
          continue;
        }

        //Assign pair indices, the mappings are populated upon insertion and when objects move tables
        //If this isn't an applicable pair, skip to the next without incrementing addIndex
        if(auto pair = _tryGetOrderedCollisionPair(gain, tableIds, stableMappings, true)) {
          pairA.at(addIndex) = pair->first;
          pairB.at(addIndex) = pair->second;

          //Assign mappings so this can be found above in removal
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
  }
};