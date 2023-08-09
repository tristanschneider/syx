#pragma once
#include "StableElementID.h"

struct PhysicsTableIds {
  size_t mZeroMassObjectTable{};
  size_t mSharedMassObjectTable{};
  size_t mSharedMassConstraintTable{};
  size_t mZeroMassConstraintTable{};
  size_t mConstriantsCommonTable{};
  size_t mSpatialQueriesTable{};
  size_t mNarrowphaseTable{};
  size_t mTableIDMask{};
  size_t mElementIDMask{};
};

struct CollisionPairOrder {
  static bool tryOrderCollisionPair(StableElementID& pairA, StableElementID& pairB, const PhysicsTableIds& tables) {
    const size_t tableA = pairA.mUnstableIndex & tables.mTableIDMask;
    const size_t tableB = pairB.mUnstableIndex & tables.mTableIDMask;
    //Spatial query goes to 'A'
    if(tableA == tables.mSpatialQueriesTable) {
      //Spatial queries don't currently hit each other
      return tableB != tables.mSpatialQueriesTable;
    }
    else if(tableB == tables.mSpatialQueriesTable) {
      std::swap(pairA, pairB);
      return true;
    }

    //Always make zero mass object 'B' in a pair for simplicity
    if(tableA == tables.mZeroMassObjectTable) {
      if(tableB == tables.mZeroMassObjectTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }
};