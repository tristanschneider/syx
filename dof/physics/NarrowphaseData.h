#pragma once

#include "Table.h"

template<class>
struct NarrowphaseData {
  struct PosX : Row<float>{};
  struct PosY : Row<float>{};
  struct CosAngle : Row<float>{};
  struct SinAngle : Row<float>{};
};
struct PairA{};
struct PairB{};

struct CollisionPairIndexA : Row<size_t> {};
struct CollisionPairIndexB : Row<size_t> {};

template<class>
struct ContactPoint {
  //Position of contact point in local space on object A
  struct PosX : Row<float> {};
  struct PosY : Row<float> {};
  //Overlap along shared normal
  struct Overlap : Row<float> {};
};
//Up to two contact points allows for a collision pair, which is plenty for 2d
struct ContactOne {};
struct ContactTwo {};

struct SharedNormal {
  struct X : Row<float> {};
  struct Y : Row<float> {};
};

//Sorted by indexA
using CollisionPairsTable = Table<
  CollisionPairIndexA,
  CollisionPairIndexB,
  //These rows are generated and updated by the broadphase and carried through to narrowphase
  //It is convenient here because it's already sorted by indexA which allows for some reduction in fetches
  //to populate the data.
  //If the cost of the broadphase inserts becomes too high due to shuffling these rows then the data could be
  //split off to its own table
  //This is a useful place to persist collision pair related data that needs to last to next frame,
  //like contact points and warm starts, data like that needs to remain sorted as collision pairs are inserted
  //Narrowphase data is regenerated every frame so doesn't have to be, and an optimization would be to only
  //sort the persistent rows then do a final resize on the non-persistent ones
  NarrowphaseData<PairA>::PosX,
  NarrowphaseData<PairA>::PosY,
  NarrowphaseData<PairA>::CosAngle,
  NarrowphaseData<PairA>::SinAngle,

  NarrowphaseData<PairB>::PosX,
  NarrowphaseData<PairB>::PosY,
  NarrowphaseData<PairB>::CosAngle,
  NarrowphaseData<PairB>::SinAngle,

  ContactPoint<ContactOne>::PosX,
  ContactPoint<ContactOne>::PosY,
  ContactPoint<ContactOne>::Overlap,

  ContactPoint<ContactTwo>::PosX,
  ContactPoint<ContactTwo>::PosY,
  ContactPoint<ContactTwo>::Overlap,

  SharedNormal::X,
  SharedNormal::Y
>;