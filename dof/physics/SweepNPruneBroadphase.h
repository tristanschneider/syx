#pragma once

#include <cassert>
#include "Profile.h"
#include "Table.h"
#include "StableElementID.h"
#include "SweepNPrune.h"
#include "Scheduler.h"
#include "AppBuilder.h"

class RuntimeDatabaseTaskBuilder;

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

  void updateBroadphase(IAppBuilder& builder);

  //Before table service
  //New elements are added to the broadphase if they have a broadphase key row
  //Removed elements are removed from the broadphase
  //Moved elements are given one final bounds update if they moved to an immobile table, otherwise ignored
  void preProcessEvents(IAppBuilder& builder);

  //After table service
  void postProcessEvents(IAppBuilder& builder);
};