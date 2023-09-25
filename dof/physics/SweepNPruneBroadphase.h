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

class RuntimeDatabaseTaskBuilder;
struct DBEvents;
struct PhysicsAliases;

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
  //struct BoundariesQuery {
  //  const Row<float>* posX{};
  //  const Row<float>* posY{};
  //  const BroadphaseKeys* keys{};
  //};
  ////Directly copies the given bounds
  //struct RawBoundariesQuery {
  //  const Row<float>* minX{};
  //  const Row<float>* minY{};
  //  const Row<float>* maxX{};
  //  const Row<float>* maxY{};
  //  const BroadphaseKeys* keys{};
  //};
  void updateBroadphase(IAppBuilder& builder, const BoundariesConfig& cfg);

  //TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<BoundariesQuery> query, const BoundariesConfig& cfg);
  //TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<RawBoundariesQuery> query);

  //Before table service
  //New elements are added to the broadphase if they have a broadphase key row
  //Removed elements are removed from the broadphase
  //Moved elements are given one final bounds update if they moved to an immobile table, otherwise ignored
  void preProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events);

  //After table service
  void postProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events, const PhysicsAliases& aliases, const BoundariesConfig& cfg);

  std::optional<std::pair<StableElementID, StableElementID>> _tryGetOrderedCollisionPair(const Broadphase::SweepCollisionPair& key, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, bool assertIfMissing);
  bool isSpatialQueryPair(const std::pair<StableElementID, StableElementID>& orderedCollisionPair, const PhysicsTableIds& tableIds);
  SpatialQueryPair getSpatialQueryPair(const std::pair<StableElementID, StableElementID>& orderedCollisionPair);
};