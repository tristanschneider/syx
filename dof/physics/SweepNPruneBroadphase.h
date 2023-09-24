#pragma once

#include <cassert>
#include "Profile.h"
#include "PhysicsTableIds.h"
#include "Table.h"
#include "TableOperations.h"
#include "StableElementID.h"
#include "SweepNPrune.h"
#include "Scheduler.h"
#include "AppBuilder.h"

//SweepNPrune is the base data structure containing no dependencies on table,
//this is the wrapper around it to facilitate use within Simulation
namespace SweepNPruneBroadphase {
  using Key = StableIDRow;
  struct BroadphaseKeys : Row<Broadphase::BroadphaseKey> {};

  struct CollisionPairMappings {
    //Fake index used to indicate if a pair is a spatial query which means it doesn't get a collision table mapping
    static constexpr size_t SPATIAL_QUERY_INDEX = std::numeric_limits<size_t>::max();
    std::unordered_map<Broadphase::SweepCollisionPair, size_t> mSweepPairToCollisionTableIndex;
    std::vector<Broadphase::SweepCollisionPair> mCollisionTableIndexToSweepPair;
  };
  struct PairChanges {
    //New collision pairs caused by reinserts or inserts
    std::vector<Broadphase::SweepCollisionPair> mGained;
    //Removed collision pairs caused by reinserts or erases
    std::vector<Broadphase::SweepCollisionPair> mLost;
  };
  struct SpatialQueryPair {
    StableElementID query{};
    StableElementID object{};
  };
  struct ChangedCollisionPairs {
    std::vector<StableElementID> mGained;
    std::vector<StableElementID> mLost;
    std::vector<SpatialQueryPair> gainedQueries;
    std::vector<SpatialQueryPair> lostQueries;
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
  //Implies unit cube configured with BoundariesConfig
  struct BoundariesQuery {
    const Row<float>* posX{};
    const Row<float>* posY{};
    const BroadphaseKeys* keys{};
  };
  //Directly copies the given bounds
  struct RawBoundariesQuery {
    const Row<float>* minX{};
    const Row<float>* minY{};
    const Row<float>* maxX{};
    const Row<float>* maxY{};
    const BroadphaseKeys* keys{};
  };
  void updateBroadphase(IAppBuilder& builder, const BoundariesConfig& cfg);

  TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<BoundariesQuery> query, const BoundariesConfig& cfg);
  TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<RawBoundariesQuery> query);
  //Compute collision pair changes
  TaskRange computeCollisionPairs(BroadphaseTable& broadphase);

  //Before table service
  //New elements are added to the broadphase if they have a broadphase key row
  //Removed elements are removed from the broadphase
  //Moved elements are given one final bounds update if they moved to an immobile table, otherwise ignored
  template<class PosX, class PosY, class Immobile>
  void preProcessEvents(const DBEvents& events, Broadphase::SweepGrid::Grid& grid, TableResolver<PosX, PosY, BroadphaseKeys, StableIDRow, Immobile> resolver, const BoundariesConfig&, const DatabaseDescription& desc) {
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      //Insert new elements
      if(cmd.isCreate()) {
        const auto unpacked = cmd.destination.toUnpacked(desc);
        auto* keys = resolver.tryGetRow<BroadphaseKeys>(unpacked);
        auto* stable = resolver.tryGetRow<StableIDRow>(unpacked);
        if(keys && stable) {
          Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
          Broadphase::UserKey userKey = stable->at(unpacked.getElementIndex());
          Broadphase::SweepGrid::insertRange(grid, &userKey, &key, 1);
        }
      }
      else if(cmd.isDestroy()) {
        //Remove elements that are about to be destroyed
        const auto unpacked = cmd.source.toUnpacked(desc);
        if(auto keys = resolver.tryGetRow<BroadphaseKeys>(unpacked)) {
          Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
          Broadphase::SweepGrid::eraseRange(grid, &key, 1);
          key = {};
        }
      }
    }
  }

  //After table service
  template<class PosX, class PosY, class Immobile>
  void postProcessEvents(const DBEvents& events, Broadphase::SweepGrid::Grid& grid, TableResolver<PosX, PosY, BroadphaseKeys, StableIDRow, Immobile> resolver, const BoundariesConfig& cfg, const DatabaseDescription& desc, const StableElementMappings& mappings) {
    //Bounds update elements that moved to an immobile row
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      if(auto found = mappings.findKey(cmd.source.mStableID)) {
        //The stable mappings are pointing at the raw index, then assume that it ended up at the destination table
        UnpackedDatabaseElementID self{ found->second, desc.elementIndexBits };
        const UnpackedDatabaseElementID rawDest = cmd.destination.toUnpacked(desc);
        //Should always be the case unless it somehow moved more than once
        if(self.getTableIndex() == rawDest.getTableIndex()) {
          const auto unpacked = self;
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
      }
    }
  }

  std::optional<std::pair<StableElementID, StableElementID>> _tryGetOrderedCollisionPair(const Broadphase::SweepCollisionPair& key, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, bool assertIfMissing);
  bool isSpatialQueryPair(const std::pair<StableElementID, StableElementID>& orderedCollisionPair, const PhysicsTableIds& tableIds);
  SpatialQueryPair getSpatialQueryPair(const std::pair<StableElementID, StableElementID>& orderedCollisionPair);

  //Turn collision pair changes into collision table entries
  //Create and remove based on the changes in PairChanges
  void udpateCollisionPairs(IAppBuilder& builder);
};