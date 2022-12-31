#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"
#include "TableOperations.h"

namespace {
  template<class RowT, class TableT>
  auto* _unwrapRow(TableT& t) {
    return std::get<RowT>(t.mRows).mElements.data();
  }

  ispc::UniformConstraintData _unwrapUniformConstraintData(ConstraintsTable& constraints) {
    return {
      _unwrapRow<ConstraintData::LinearAxisX>(constraints),
      _unwrapRow<ConstraintData::LinearAxisY>(constraints),
      _unwrapRow<ConstraintData::AngularAxisA>(constraints),
      _unwrapRow<ConstraintData::AngularAxisB>(constraints),
      _unwrapRow<ConstraintData::ConstraintMass>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseX>(constraints),
      _unwrapRow<ConstraintData::LinearImpulseY>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseA>(constraints),
      _unwrapRow<ConstraintData::AngularImpulseB>(constraints),
      _unwrapRow<ConstraintData::Bias>(constraints)
    };
  }

  template<class CObj>
  ispc::UniformConstraintObject _unwrapUniformConstraintObject(ConstraintsTable& constraints) {
    using ConstraintT = ConstraintObject<CObj>;
    return {
      _unwrapRow<ConstraintT::LinVelX>(constraints),
      _unwrapRow<ConstraintT::LinVelY>(constraints),
      _unwrapRow<ConstraintT::AngVel>(constraints),
      _unwrapRow<ConstraintT::SyncIndex>(constraints),
      _unwrapRow<ConstraintT::SyncType>(constraints)
    };
  }

  struct IRect {
    glm::ivec2 mMin, mMax;
  };

  IRect _buildRect(const glm::vec2& min, const glm::vec2& max) {
    return {
      glm::ivec2{ int(std::floor(min.x)), int(std::floor(min.y)) },
      glm::ivec2{ int(std::ceil(max.x)), int(std::ceil(max.y)) }
    };
  }

  size_t _toIndex(int x, int y, const GridBroadphase::AllocatedDimensions& dimensions) {
    const int cx = glm::clamp(x, dimensions.mMin.x, dimensions.mMax.x);
    const int cy = glm::clamp(y, dimensions.mMin.y, dimensions.mMax.y);
    return size_t(cx - dimensions.mMin.x) + size_t(cy - dimensions.mMin.y)*dimensions.mStride;
  }

  void _addCollisionPair(size_t self, size_t other, CollisionPairsTable& table) {
    CollisionPairIndexA& rowA = std::get<CollisionPairIndexA>(table.mRows);
    CollisionPairIndexB& rowB = std::get<CollisionPairIndexB>(table.mRows);
    //Table is sorted by index A, so find A first
    auto it = std::lower_bound(rowA.begin(), rowA.end(), self);
    if(it != rowA.end() && *it == self) {
      //If A was found, start here in B to see if that index exists
      const size_t indexA = std::distance(rowA.begin(), it);
      for(size_t i = indexA; i < rowA.size(); ++i) {
        //If this passed the range of A indices then stop now, this is not a duplicate
        if(rowA.at(i) != self) {
          break;
        }
        //If B was found, that means the pair of A and B already exists, so no need to add it
        if(rowB.at(i) == other) {
          return;
        }
      }
    }

    CollisionPairsTable::ElementRef result = TableOperations::addToTableAt(table, rowA, it);
    result.get<0>() = self;
    result.get<1>() = other;
  }
}

void Physics::details::_integratePositionAxis(float* velocity, float* position, size_t count) {
  ispc::integratePosition(position, velocity, uint32_t(count));
}

void Physics::details::_integrateRotation(float* rotX, float* rotY, float* velocity, size_t count) {
  ispc::integrateRotation(rotX, rotY, velocity, uint32_t(count));
}

void Physics::details::_applyDampingMultiplier(float* velocity, float amount, size_t count) {
  ispc::applyDampingMultiplier(velocity, amount, uint32_t(count));
}

void Physics::allocateBroadphase(GridBroadphase::BroadphaseTable& table) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(table.mRows).at();
  auto& allocatedDimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(table.mRows).at();
  constexpr float cellSize = 1.0f;
  assert(dimensions.mMin.x <= dimensions.mMax.x);
  assert(dimensions.mMin.y <= dimensions.mMax.y);

  const IRect rect = _buildRect(dimensions.mMin, dimensions.mMax);

  allocatedDimensions.mMin = rect.mMin;
  allocatedDimensions.mMax = rect.mMax;
  allocatedDimensions.mStride = std::max(size_t(1), size_t(rect.mMax.x - rect.mMin.x));
  size_t sizeY = size_t(allocatedDimensions.mMax.y - allocatedDimensions.mMin.y) + size_t(1);

  //Allocate desired space
  TableOperations::resizeTable(table, sizeY * allocatedDimensions.mStride);
  clearBroadphase(table);
}

void Physics::rebuildBroadphase(
  size_t baseIndex,
  const float* xPositions,
  const float* yPositions,
  GridBroadphase::BroadphaseTable& broadphase,
  size_t insertCount) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(broadphase.mRows).at();
  auto& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();
  std::vector<GridBroadphase::Cell>& cells = std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements;

  //Vector to max extents of the shape regardless of rotation
  const float centerToEdge = 0.5f;
  const glm::vec2 extents(std::sqrt(centerToEdge*centerToEdge*2));
  for(size_t i = 0; i < insertCount; ++i) {
    const glm::vec2 center{ xPositions[i], yPositions[i] };
    const IRect rect = _buildRect(center - extents, center + extents);
    const size_t indexToStore = baseIndex + i;

    //Store index to this in all cells it overlaps with
    for(int x = rect.mMin.x; x < rect.mMax.x; ++x) {
      for(int y = rect.mMin.y; y < rect.mMax.y; ++y) {
        bool slotFound = false;
        const size_t cellIndex = _toIndex(x, y, dimensions);
        for(size_t& storedIndex : cells[cellIndex].mElements) {
          if(storedIndex == GridBroadphase::EMPTY_ID) {
            storedIndex = indexToStore;
            slotFound = true;
            break;
          }
          //Don't put self in list multiple times. Shouldn't generally happen unless index is getting clamped due to being outside the boundaries
          else if(storedIndex == indexToStore) {
            slotFound = true;
            break;
          }
        }

        if(!slotFound) {
          overflow.mElements.push_back(indexToStore);
        }
      }
    }
  }

  //Remove duplicates
  std::sort(overflow.mElements.begin(), overflow.mElements.end());
  overflow.mElements.erase(std::unique(overflow.mElements.begin(), overflow.mElements.end()), overflow.mElements.end());
}

void Physics::clearBroadphase(GridBroadphase::BroadphaseTable& broadphase) {
  for(GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    cell.mElements.fill(GridBroadphase::EMPTY_ID);
  }
  std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at().mElements.clear();
}

//TODO: this is a bit clunky to do separately from generation and update because most of the time all collision pairs from last frame would have been fine
void Physics::generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs) {
  const GridBroadphase::Overflow& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();

  //TODO: retain pairs for a while then remove them occasionally if far away
  TableOperations::resizeTable(pairs, 0);

  //There could be a way to optimize for empty cells but the assumption is that most cells are not empty
  for(const GridBroadphase::Cell& cell : std::get<Row<GridBroadphase::Cell>>(broadphase.mRows).mElements) {
    for(size_t self = 0; self < cell.mElements.size(); ++self) {
      const size_t selfID = cell.mElements[self];
      if(selfID == GridBroadphase::EMPTY_ID) {
        continue;
      }
      //Add pairs with all others in the cell
      for(size_t other = self + 1; other < cell.mElements.size(); ++other) {
        const size_t otherID = cell.mElements[other];
        if(otherID == GridBroadphase::EMPTY_ID) {
          break;
        }
        //TODO: optimization here for avoiding pairs of static objects and for doing all inserts at once
        _addCollisionPair(selfID, otherID, pairs);
      }

      //Add pairs with cell-less overflow objects
      //The hope is that overflow is rare/never happens
      for(size_t other : overflow.mElements) {
        if(other != selfID) {
          _addCollisionPair(selfID, other, pairs);
        }
      }
    }
  }
}

void Physics::generateContacts(CollisionPairsTable& pairs) {
  ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  ispc::UniformRotation rotationsA{ _unwrapRow<NarrowphaseData<PairA>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairA>::SinAngle>(pairs) };
  ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  ispc::UniformRotation rotationsB{ _unwrapRow<NarrowphaseData<PairB>::CosAngle>(pairs), _unwrapRow<NarrowphaseData<PairB>::SinAngle>(pairs) };
  ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };

  ispc::UniformContact contactsOne{
    _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  };
  ispc::UniformContact contactsTwo{
    _unwrapRow<ContactPoint<ContactTwo>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactTwo>::Overlap>(pairs)
  };
  ispc::generateUnitCubeCubeContacts(positionsA, rotationsA, positionsB, rotationsB, normals, contactsOne, contactsTwo, uint32_t(TableOperations::size(pairs)));
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));

  //ispc::UniformConstVec2 positionsA{ _unwrapRow<NarrowphaseData<PairA>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairA>::PosY>(pairs) };
  //ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  //ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };
  //ispc::UniformContact contacts{
  //  _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
  //  _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  //};
  //ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));
}

template<class PosRowA, class PosRowB, class ContactRow, class DstRowA, class DstRowB>
void _toRVector(CollisionPairsTable& pairs, ConstraintsTable& constraints) {
  PosRowA& posA = std::get<PosRowA>(pairs.mRows);
  PosRowB& posB = std::get<PosRowB>(pairs.mRows);
  ContactRow& contacts = std::get<ContactRow>(pairs.mRows);
  DstRowA& dstA = std::get<DstRowA>(constraints.mRows);
  DstRowB& dstB = std::get<DstRowB>(constraints.mRows);

  ispc::turnContactsToRVectors(
    posA.mElements.data(),
    posB.mElements.data(),
    contacts.mElements.data(),
    dstA.mElements.data(),
    dstB.mElements.data(),
    uint32_t(TableOperations::size(pairs)));
}

struct ConstraintSyncData {
  ConstraintSyncData(CollisionPairsTable& pairs, ConstraintsTable& constraints)
    : mSyncIndexA(std::get<ConstraintObject<ConstraintObjA>::SyncIndex>(constraints.mRows))
    , mSyncTypeA(std::get<ConstraintObject<ConstraintObjA>::SyncType>(constraints.mRows))
    , mPairIndexA(std::get<CollisionPairIndexA>(constraints.mRows))
    , mCenterToContactXA(std::get<ConstraintObject<ConstraintObjA>::CenterToContactX>(constraints.mRows))
    , mCenterToContactYA(std::get<ConstraintObject<ConstraintObjA>::CenterToContactY>(constraints.mRows))
    , mSyncIndexB(std::get<ConstraintObject<ConstraintObjB>::SyncIndex>(constraints.mRows))
    , mSyncTypeB(std::get<ConstraintObject<ConstraintObjB>::SyncType>(constraints.mRows))
    , mPairIndexB(std::get<CollisionPairIndexB>(constraints.mRows))
    , mCenterToContactXB(std::get<ConstraintObject<ConstraintObjB>::CenterToContactX>(constraints.mRows))
    , mCenterToContactYB(std::get<ConstraintObject<ConstraintObjB>::CenterToContactY>(constraints.mRows))
    , mDestContactOneOverlap(std::get<ContactPoint<ContactOne>::Overlap>(constraints.mRows))
    , mDestContactTwoOverlap(std::get<ContactPoint<ContactTwo>::Overlap>(constraints.mRows))
    , mDestNormalX(std::get<SharedNormal::X>(constraints.mRows))
    , mDestNormalY(std::get<SharedNormal::Y>(constraints.mRows))
    , mContactOneX(std::get<ContactPoint<ContactOne>::PosX>(pairs.mRows))
    , mContactOneY(std::get<ContactPoint<ContactOne>::PosY>(pairs.mRows))
    , mPosAX(std::get<NarrowphaseData<PairA>::PosX>(pairs.mRows))
    , mPosAY(std::get<NarrowphaseData<PairA>::PosY>(pairs.mRows))
    , mPosBX(std::get<NarrowphaseData<PairB>::PosX>(pairs.mRows))
    , mPosBY(std::get<NarrowphaseData<PairB>::PosY>(pairs.mRows))
    , mSourceContactOneOverlap(std::get<ContactPoint<ContactOne>::Overlap>(pairs.mRows))
    , mSourceContactTwoOverlap(std::get<ContactPoint<ContactTwo>::Overlap>(pairs.mRows))
    , mSourceNormalX(std::get<SharedNormal::X>(pairs.mRows))
    , mSourceNormalY(std::get<SharedNormal::Y>(pairs.mRows)) {
  }

  ConstraintObject<ConstraintObjA>::SyncIndex& mSyncIndexA;
  ConstraintObject<ConstraintObjA>::SyncType& mSyncTypeA;
  CollisionPairIndexA& mPairIndexA;
  ConstraintObject<ConstraintObjA>::CenterToContactX& mCenterToContactXA;
  ConstraintObject<ConstraintObjA>::CenterToContactY& mCenterToContactYA;

  ConstraintObject<ConstraintObjB>::SyncIndex& mSyncIndexB;
  ConstraintObject<ConstraintObjB>::SyncType& mSyncTypeB;
  CollisionPairIndexB& mPairIndexB;
  ConstraintObject<ConstraintObjB>::CenterToContactX& mCenterToContactXB;
  ConstraintObject<ConstraintObjB>::CenterToContactY& mCenterToContactYB;

  ContactPoint<ContactOne>::Overlap& mDestContactOneOverlap;
  ContactPoint<ContactTwo>::Overlap& mDestContactTwoOverlap;
  SharedNormal::X& mDestNormalX;
  SharedNormal::Y& mDestNormalY;

  ContactPoint<ContactOne>::PosX& mContactOneX;
  ContactPoint<ContactOne>::PosY& mContactOneY;

  NarrowphaseData<PairA>::PosX& mPosAX;
  NarrowphaseData<PairA>::PosY& mPosAY;
  NarrowphaseData<PairB>::PosX& mPosBX;
  NarrowphaseData<PairB>::PosY& mPosBY;

  ContactPoint<ContactOne>::Overlap& mSourceContactOneOverlap;
  ContactPoint<ContactTwo>::Overlap& mSourceContactTwoOverlap;
  SharedNormal::X& mSourceNormalX;
  SharedNormal::Y& mSourceNormalY;
};

void _syncConstraintData(ConstraintSyncData& data, size_t constraintIndex, size_t objectIndex, size_t indexA, size_t indexB) {
  data.mPairIndexA.at(constraintIndex) = indexA;
  data.mPairIndexB.at(constraintIndex) = indexB;
  data.mCenterToContactXA.at(constraintIndex) = data.mContactOneX.at(objectIndex) - data.mPosAX.at(objectIndex);
  data.mCenterToContactYA.at(constraintIndex) = data.mContactOneY.at(objectIndex) - data.mPosAY.at(objectIndex);
  data.mCenterToContactXB.at(constraintIndex) = data.mContactOneX.at(objectIndex) - data.mPosBX.at(objectIndex);
  data.mCenterToContactYB.at(constraintIndex) = data.mContactOneY.at(objectIndex) - data.mPosBY.at(objectIndex);
  data.mDestContactOneOverlap.at(constraintIndex) = data.mSourceContactOneOverlap.at(objectIndex);
  data.mDestContactTwoOverlap.at(constraintIndex) = data.mSourceContactTwoOverlap.at(objectIndex);
  data.mDestNormalX.at(constraintIndex) = data.mSourceNormalX.at(objectIndex);
  data.mDestNormalY.at(constraintIndex) = data.mSourceNormalY.at(objectIndex);
}

struct VisitAttempt {
  size_t mDesiredObjectIndex{};
  size_t mDesiredConstraintIndex{};
  std::vector<ConstraintData::VisitData>::iterator mIt;
  size_t mRequiredPadding{};
};

void _setVisitDataAndTrySetSyncPoint(std::vector<ConstraintData::VisitData>& visited, VisitAttempt& attempt,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB,
  ConstraintData::VisitData::Location location,
  VisitAttempt* dependentAttempt) {
  //Set it to nosync for now, later iteration might set this as new constraints are visited
  syncTypeA.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  syncTypeB.at(attempt.mDesiredConstraintIndex) = ispc::NoSync;
  if(attempt.mIt == visited.end() || attempt.mIt->mObjectIndex != attempt.mDesiredObjectIndex) {
    //Goofy hack here, if there's another iterator for object B, make sure the iterator is still valid after the new element is inserted
    //Needs to be rebuilt if it is after the insert location, as everything would have shifted over
    const bool needRebuiltDependent = dependentAttempt && dependentAttempt->mDesiredObjectIndex > attempt.mDesiredObjectIndex;

    //If this is the first time visiting this object, no need to sync anything, but note it for later
    visited.insert(attempt.mIt, ConstraintData::VisitData{
      attempt.mDesiredObjectIndex,
      attempt.mDesiredConstraintIndex,
      location,
      attempt.mDesiredConstraintIndex,
      location
    });

    if(needRebuiltDependent) {
      dependentAttempt->mIt = std::lower_bound(visited.begin(), visited.end(), dependentAttempt->mDesiredObjectIndex);
    }
  }
  else {
    //A has been visited before, add a sync index
    const int newLocation = location == ConstraintData::VisitData::Location::InA ? ispc::SyncToIndexA : ispc::SyncToIndexB;
    //Make the previously visited constraint publish the velocity forward to this one
    switch(attempt.mIt->mLocation) {
      case ConstraintData::VisitData::Location::InA:
        syncTypeA.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexA.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
      case ConstraintData::VisitData::Location::InB:
        syncTypeB.at(attempt.mIt->mConstraintIndex) = newLocation;
        syncIndexB.at(attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
    }
    //Now that the latest instance of this object is at this visit location, update the visit data
    attempt.mIt->mConstraintIndex = attempt.mDesiredConstraintIndex;
    attempt.mIt->mLocation = location;
  }
}

//If an object shows up in multiple constraints, sync the velocity data from the last constraint back to the first
void _trySetFinalSyncPoint(const ConstraintData::VisitData& visited,
  ConstraintObject<ConstraintObjA>::SyncIndex& syncIndexA,
  ConstraintObject<ConstraintObjA>::SyncType& syncTypeA,
  ConstraintObject<ConstraintObjB>::SyncIndex& syncIndexB,
  ConstraintObject<ConstraintObjB>::SyncType& syncTypeB) {
  //If the first is the latest, there is only one instance of this object meaning its velocity was never copied
  if(visited.mFirstConstraintIndex == visited.mConstraintIndex) {
    return;
  }

  //The container to sync from, meaning the final visited entry where the most recent velocity is
  std::vector<int>* syncFromIndex = &syncIndexA.mElements;
  std::vector<int>* syncFromType = &syncTypeA.mElements;
  if(visited.mLocation == ConstraintData::VisitData::Location::InB) {
    syncFromIndex = &syncIndexB.mElements;
    syncFromType = &syncTypeB.mElements;
  }

  //Sync from this visited entry back to the first element
  syncFromIndex->at(visited.mConstraintIndex) = visited.mFirstConstraintIndex;
  //Write from the location of the final entry to the location of the first
  switch(visited.mFirstLocation) {
    case ConstraintData::VisitData::Location::InA: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexA; break;
    case ConstraintData::VisitData::Location::InB: syncFromType->at(visited.mConstraintIndex) = ispc::SyncToIndexB; break;
  }
}

VisitAttempt _tryVisit(std::vector<ConstraintData::VisitData>& visited, size_t toVisit, size_t currentConstraintIndex, size_t targetWidth) {
  VisitAttempt result;
  result.mIt = std::lower_bound(visited.begin(), visited.end(), toVisit);
  if(result.mIt != visited.end() && result.mIt->mObjectIndex == toVisit && 
    (currentConstraintIndex - result.mIt->mConstraintIndex) < targetWidth) {
    result.mRequiredPadding = targetWidth - (currentConstraintIndex - result.mIt->mConstraintIndex);
  }
  result.mDesiredConstraintIndex = currentConstraintIndex;
  result.mDesiredObjectIndex = toVisit;
  return result;
}

//TODO: what if all the constraints were block solved? If they were solved in blocks as wide as simd lanes then
//maybe this complicated shuffling wouldn't be needed. Or maybe even a giant matrix for all objects

void Physics::buildConstraintsTable(CollisionPairsTable& pairs, ConstraintsTable& constraints) {
  //TODO: only rebuild if collision pairs changed. Could be really lazy about it since solving stale constraints should result in no changes to velocity
  TableOperations::resizeTable(constraints, TableOperations::size(pairs));

  ConstraintData::SharedVisitData& visitData = std::get<ConstraintData::SharedVisitDataRow>(constraints.mRows).at();
  std::vector<ConstraintData::VisitData>& visited = visitData.mVisited;
  visited.clear();
  //Need to reserve big enough because visited vector growth would cause iterator invalidation for visitA/visitB cases below
  //Size of pairs is bigger than necessary, it only needs to be the number of objects, but that's not known here and over-allocating
  //a bit isn't a problem
  visited.reserve(TableOperations::size(pairs));
  ConstraintSyncData data(pairs, constraints);

  //Specifically from the collision table not the constraints table, to be used before the indices in the collision table have been created yet
  auto& srcPairIndexA = std::get<CollisionPairIndexA>(pairs.mRows);
  auto& srcPairIndexB = std::get<CollisionPairIndexB>(pairs.mRows);

  //The same object can't be within this many indices of itself since the velocity needs to be seen immediately by the next constraint
  //which wouldn't be the case if it was being solved in another simd lane
  const size_t targetWidth = size_t(ispc::getTargetWidth());
  //Figure out sync indices
  std::deque<size_t>& indicesToFill = visitData.mIndicesToFill;
  indicesToFill.resize(TableOperations::size(pairs));
  for(size_t i = 0; i < TableOperations::size(pairs); ++i) {
    indicesToFill[i] = i;
  }
  size_t currentConstraintIndex = 0;
  size_t failedPlacements = 0;
  //Fill each element in the constraints table one by one, trying the latest in indicesToFill each type
  //If it fails, swap it to back and hope it works later. If it doesn't work this will break out with remaining indices
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    indicesToFill.pop_front();
    const size_t desiredA = srcPairIndexA.at(indexToFill);
    const size_t desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);

    //If this can't be filled now keep going and hopefully this will work later
    if(visitA.mRequiredPadding || visitB.mRequiredPadding) {
      indicesToFill.push_back(indexToFill);
      //If the last few are impossible to fill this way, break
      if(++failedPlacements >= indicesToFill.size()) {
        break;
      }
      continue;
    }

    _syncConstraintData(data, currentConstraintIndex, indexToFill, desiredA, desiredB);

    _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
    _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const size_t desiredA = srcPairIndexA.at(indexToFill);
    const size_t desiredB = srcPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);
    const size_t padding = std::max(visitA.mRequiredPadding, visitB.mRequiredPadding);

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(data, currentConstraintIndex, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InA, &visitB);
      _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB, ConstraintData::VisitData::Location::InB, nullptr);
      indicesToFill.pop_front();
      ++currentConstraintIndex;
    }
    //Space needs to be made, add padding then let the iteration contine which will attempt the index again with the new space
    else {
      TableOperations::resizeTable(constraints, TableOperations::size(constraints) + padding);
      for(size_t i = 0; i < padding; ++i) {
        //These are nonsense entires that won't be used for anything, at least set them to no sync so they don't try to copy stale data
        data.mSyncTypeA.at(currentConstraintIndex) = ispc::NoSync;
        data.mSyncTypeB.at(currentConstraintIndex) = ispc::NoSync;
        ++currentConstraintIndex;
      }
    }
  }

  //Store the final indices that the velocity will end up in
  //This is in the visited data since that's been tracking every access
  FinalSyncIndices& finalData = std::get<SharedRow<FinalSyncIndices>>(constraints.mRows).at();
  finalData.mMappingsA.clear();
  finalData.mMappingsB.clear();
  for(const ConstraintData::VisitData& v : visited) {
    //Link the final constraint entry back to the velocity data of the first that uses the objects if velocity was duplicated
    //This matters from one iteration to the next to avoid working on stale data, doesn't matter for the last iteration
    //since the final results will be copied from the end locations stored in mappings below
    _trySetFinalSyncPoint(v, data.mSyncIndexA, data.mSyncTypeA, data.mSyncIndexB, data.mSyncTypeB);

    //Store the final location mapping
    switch(v.mLocation) {
      case ConstraintData::VisitData::Location::InA: {
        finalData.mMappingsA.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
      case ConstraintData::VisitData::Location::InB: {
        finalData.mMappingsB.emplace_back(FinalSyncIndices::Mapping{ v.mObjectIndex, v.mConstraintIndex });
        break;
      }
    }
  }
}

void Physics::setupConstraints(ConstraintsTable& constraints) {
  //Currently computing as square
  //const float pi = 3.14159265359f;
  //const float r = 0.5f;
  //const float density = 1.0f;
  //const float mass = pi*r*r;
  //const float inertia = pi*(r*r*r*r)*0.25f;
  const float w = 0.5f;
  const float h = 0.5f;
  const float mass = w*h;
  const float inertia = (2.0f*w)*(2.0f*w)*(2.0f*w)*(2.0f*w)/12.0f;
  const float invMass = 1.0f/mass;
  const float invInertia = 1.0f/inertia;
  const float bias = 0.1f;
  ispc::UniformVec2 normal{ _unwrapRow<SharedNormal::X>(constraints), _unwrapRow<SharedNormal::Y>(constraints) };
  ispc::UniformVec2 aToContact{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactY>(constraints) };
  ispc::UniformVec2 bToContact{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactY>(constraints) };
  float* overlap = _unwrapRow<ContactPoint<ContactOne>::Overlap>(constraints);
  ispc::UniformConstraintData data = _unwrapUniformConstraintData(constraints);

  //TODO: don't clear this here and use it for warm start
  float* sums = _unwrapRow<ConstraintData::LambdaSum>(constraints);
  for(size_t i = 0; i < TableOperations::size(constraints); ++i) {
    sums[i] = 0.0f;
  }

  ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContact, bToContact, overlap, data, uint32_t(TableOperations::size(constraints)));
}

void Physics::solveConstraints(ConstraintsTable& constraints) {
  ispc::UniformConstraintData data = _unwrapUniformConstraintData(constraints);
  ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject<ConstraintObjA>(constraints);
  ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject<ConstraintObjB>(constraints);
  float* lambdaSum = _unwrapRow<ConstraintData::LambdaSum>(constraints);

  ispc::solveContactConstraints(data, objectA, objectB, lambdaSum, uint32_t(TableOperations::size(constraints)));
}
