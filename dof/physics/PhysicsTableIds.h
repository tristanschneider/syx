#pragma once
#include "StableElementID.h"

struct PhysicsTableIds {
  size_t mZeroMassTable{};
  size_t mSharedMassTable{};
  size_t mTableIDMask{};
};

struct CollisionPairOrder {
  static bool tryOrderCollisionPair(StableElementID& pairA, StableElementID& pairB, const PhysicsTableIds& tables) {
    //Always make zero mass object 'B' in a pair for simplicity
    if((pairA.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassTable) {
      if((pairB.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }
};