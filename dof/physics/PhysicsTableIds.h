#pragma once

struct PhysicsTableIds {
  size_t mZeroMassTable{};
  size_t mSharedMassTable{};
  size_t mTableIDMask{};
};

struct CollisionPairOrder {
  static bool tryOrderCollisionPair(size_t& pairA, size_t& pairB, const PhysicsTableIds& tables) {
    //Always make zero mass object 'B' in a pair for simplicity
    if((pairA & tables.mTableIDMask) == tables.mZeroMassTable) {
      if((pairB & tables.mTableIDMask) == tables.mZeroMassTable) {
        //If they're both static, skip this pair
        return false;
      }
      std::swap(pairA, pairB);
    }
    return true;
  }
};