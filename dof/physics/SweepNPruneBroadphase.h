#pragma once

#include <cassert>
#include "Profile.h"
#include "Table.h"
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

  //Broadphase is responsible for making sure gains and losses are unique and pairs have a deterministic order,
  //meaning the order wouldn't be flipped between a gain and loss
  struct PairChanges {
    //New collision pairs caused by reinserts or inserts
    std::vector<Broadphase::SweepCollisionPair> mGained;
    //Removed collision pairs caused by reinserts or erases
    std::vector<Broadphase::SweepCollisionPair> mLost;
  };
  using BroadphaseTable = Table<
    SharedRow<Broadphase::SweepGrid::Grid>,
    SharedRow<PairChanges>
  >;

  struct BoundariesConfig {
    const static inline float UNIT_CUBE_EXTENTS = std::sqrt(0.5f*0.5f + 0.5f*0.5f);
    //Distance from pos to extent used to compute where this is relative to its boundaries
    float mHalfSize = UNIT_CUBE_EXTENTS;
    //The amount the boundaries are extended past the size when modifying boundaries
    float mPadding = 0.0f;
  };

  void updateBroadphase(IAppBuilder& builder, const BoundariesConfig& cfg, const PhysicsAliases& aliases);

  //Before table service
  //New elements are added to the broadphase if they have a broadphase key row
  //Removed elements are removed from the broadphase
  //Moved elements are given one final bounds update if they moved to an immobile table, otherwise ignored
  void preProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events);

  //After table service
  void postProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events, const PhysicsAliases& aliases, const BoundariesConfig& cfg);
};