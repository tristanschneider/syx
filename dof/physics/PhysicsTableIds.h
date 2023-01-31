#pragma once
#include "StableElementID.h"

struct PhysicsTableIds {
  size_t mZeroMassObjectTable{};
  size_t mSharedMassObjectTable{};
  size_t mSharedMassConstraintTable{};
  size_t mZeroMassConstraintTable{};
  size_t mConstriantsCommonTable{};
  size_t mTableIDMask{};
  size_t mElementIDMask{};
};

struct CollisionPairOrder {
  static bool tryOrderCollisionPair(StableElementID& pairA, StableElementID& pairB, const PhysicsTableIds& tables) {
    //Always make zero mass object 'B' in a pair for simplicity
    if((pairA.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassObjectTable) {
      if((pairB.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassObjectTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }
};