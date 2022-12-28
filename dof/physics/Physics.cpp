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
    const size_t cx = glm::clamp(x, dimensions.mOriginX, int(dimensions.mCellsX));
    const size_t cy = glm::clamp(y, dimensions.mOriginY, int(dimensions.mCellsY));
    return size_t(cx - dimensions.mOriginX) + size_t(cy - dimensions.mOriginY)*dimensions.mCellsX;
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

void Physics::allocateBroadphase(GridBroadphase::BroadphaseTable& table) {
  const auto& dimensions = std::get<SharedRow<GridBroadphase::RequestedDimensions>>(table.mRows).at();
  auto& allocatedDimensions = std::get<SharedRow<GridBroadphase::AllocatedDimensions>>(table.mRows).at();
  constexpr float cellSize = 1.0f;
  assert(dimensions.mMin.x <= dimensions.mMax.x);
  assert(dimensions.mMin.y <= dimensions.mMax.y);

  const IRect rect = _buildRect(dimensions.mMin, dimensions.mMax);

  allocatedDimensions.mOriginX = size_t(rect.mMin.x);
  allocatedDimensions.mOriginY = size_t(rect.mMin.y);
  allocatedDimensions.mCellsX = std::max(size_t(1), size_t(rect.mMax.x) - allocatedDimensions.mOriginX);
  allocatedDimensions.mCellsY = std::max(size_t(1), size_t(rect.mMax.y) - allocatedDimensions.mOriginY);

  //Allocate desired space
  TableOperations::resizeTable(table, allocatedDimensions.mCellsY * allocatedDimensions.mCellsX);
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
  const glm::vec2 extents(std::sqrt(centerToEdge*centerToEdge)*2);
  for(size_t i = 0; i < insertCount; ++i) {
    const glm::vec2 center{ xPositions[i], yPositions[i] };
    const IRect rect = _buildRect(center - extents, center + extents);
    const size_t indexToStore = baseIndex + i;

    //Store index to this in all cells it overlaps with
    for(int x = rect.mMin.x; x < rect.mMax.x; ++x) {
      for(int y = rect.mMin.y; y < rect.mMax.y; ++y) {
        bool slotFound = false;
        for(size_t& storedIndex : cells[_toIndex(x, y, dimensions)].mElements) {
          if(storedIndex == GridBroadphase::EMPTY_ID) {
            storedIndex = indexToStore;
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
}

//TODO: this is a bit clunky to do separately from generation and update because most of the time all collision pairs from last frame would have been fine
void Physics::generateCollisionPairs(const GridBroadphase::BroadphaseTable& broadphase, CollisionPairsTable& pairs) {
  const GridBroadphase::Overflow& overflow = std::get<SharedRow<GridBroadphase::Overflow>>(broadphase.mRows).at();

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
          continue;
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
  ispc::UniformConstVec2 positionsB{ _unwrapRow<NarrowphaseData<PairB>::PosX>(pairs), _unwrapRow<NarrowphaseData<PairB>::PosY>(pairs) };
  ispc::UniformVec2 normals{ std::get<SharedNormal::X>(pairs.mRows).mElements.data(), std::get<SharedNormal::Y>(pairs.mRows).mElements.data() };
  ispc::UniformContact contacts{
    _unwrapRow<ContactPoint<ContactOne>::PosX>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::PosY>(pairs),
    _unwrapRow<ContactPoint<ContactOne>::Overlap>(pairs)
  };
  ispc::generateUnitSphereSphereContacts(positionsA, positionsB, normals, contacts, uint32_t(TableOperations::size(pairs)));
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
  std::vector<int>& syncIndex,
  std::vector<int>& syncType,
  ConstraintData::VisitData::Location location) {
  //Set it to nosync for now, later iteration might set this as new constraints are visited
  syncType[attempt.mDesiredConstraintIndex] = ispc::NoSync;
  if(attempt.mIt == visited.end() || attempt.mIt->mObjectIndex != attempt.mDesiredObjectIndex) {
    //If this is the first time visiting this object, no need to sync anything, but note it for later
    visited.insert(attempt.mIt, ConstraintData::VisitData{ attempt.mDesiredObjectIndex, attempt.mDesiredConstraintIndex, location });
  }
  else {
    //A has been visited before, add a sync index
    switch(attempt.mIt->mLocation) {
      case ConstraintData::VisitData::Location::InA: syncType[attempt.mDesiredConstraintIndex] = ispc::SyncToIndexA; break;
      case ConstraintData::VisitData::Location::InB: syncType[attempt.mDesiredConstraintIndex] = ispc::SyncToIndexB; break;
    }
    //Make the previously visited constraint publish the velocity forward to this one
    syncIndex[attempt.mIt->mConstraintIndex] =  attempt.mDesiredConstraintIndex;
    //Now that the latest instance of this object is at this visit location, update the visit data
    attempt.mIt->mConstraintIndex = attempt.mDesiredConstraintIndex;
    attempt.mIt->mLocation = location;
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
  //TODO: only rebuild if collision pairs changed
  TableOperations::resizeTable(constraints, TableOperations::size(pairs));

  std::vector<ConstraintData::VisitData>& visited = std::get<ConstraintData::SharedVisitData>(constraints.mRows).at();
  visited.clear();
  ConstraintSyncData data(pairs, constraints);

  //The same object can't be within this many indices of itself since the velocity needs to be seen immediately by the next constraint
  //which wouldn't be the case if it was being solved in another simd lane
  const size_t targetWidth = size_t(ispc::getTargetWidth());
  //Figure out sync indices
  std::deque<size_t> indicesToFill(TableOperations::size(pairs));
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
    const size_t desiredA = data.mPairIndexA.at(indexToFill);
    const size_t desiredB = data.mPairIndexB.at(indexToFill);
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

    _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA.mElements, data.mSyncTypeA.mElements, ConstraintData::VisitData::Location::InA);
    _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexB.mElements, data.mSyncTypeB.mElements, ConstraintData::VisitData::Location::InB);

    ++currentConstraintIndex;
    failedPlacements = 0;
  }

  //If elements remain here it means there wasn't an order of elements possible as-is.
  //Add padding between these remaining elements to solve the problem.
  while(!indicesToFill.empty()) {
    const size_t indexToFill = indicesToFill.front();
    const size_t desiredA = data.mPairIndexA.at(indexToFill);
    const size_t desiredB = data.mPairIndexB.at(indexToFill);
    VisitAttempt visitA = _tryVisit(visited, desiredA, currentConstraintIndex, targetWidth);
    VisitAttempt visitB = _tryVisit(visited, desiredB, currentConstraintIndex, targetWidth);
    const size_t padding = std::max(visitA.mRequiredPadding, visitB.mRequiredPadding);

    //If no padding is required that means the previous iteration made space, add the constraint here
    if(!padding) {
      _syncConstraintData(data, currentConstraintIndex, indexToFill, desiredA, desiredB);
      _setVisitDataAndTrySetSyncPoint(visited, visitA, data.mSyncIndexA.mElements, data.mSyncTypeA.mElements, ConstraintData::VisitData::Location::InA);
      _setVisitDataAndTrySetSyncPoint(visited, visitB, data.mSyncIndexB.mElements, data.mSyncTypeB.mElements, ConstraintData::VisitData::Location::InB);
      indicesToFill.pop_front();
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
  //Currently computing as sphere
  const float pi = 3.14159265359f;
  const float r = 0.5f;
  const float density = 1.0f;
  const float mass = pi*r*r;
  const float inertia = pi*(r*r*r*r)*0.25f;
  const float invMass = 1.0f/mass;
  const float invInertia = 1.0f/inertia;
  const float bias = 0.1f;
  ispc::UniformVec2 normal{ _unwrapRow<SharedNormal::X>(constraints), _unwrapRow<SharedNormal::Y>(constraints) };
  ispc::UniformVec2 aToContact{ _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjA>::CenterToContactY>(constraints) };
  ispc::UniformVec2 bToContact{ _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactX>(constraints), _unwrapRow<ConstraintObject<ConstraintObjB>::CenterToContactY>(constraints) };
  float* overlap = _unwrapRow<ContactPoint<ContactOne>::Overlap>(constraints);
  ispc::UniformConstraintData data = _unwrapUniformConstraintData(constraints);

  ispc::setupConstraintsSharedMass(invMass, invInertia, bias, normal, aToContact, bToContact, overlap, data, uint32_t(TableOperations::size(constraints)));
}

void Physics::solveConstraints(ConstraintsTable& constraints) {
  ispc::UniformConstraintData data = _unwrapUniformConstraintData(constraints);
  ispc::UniformConstraintObject objectA = _unwrapUniformConstraintObject<ConstraintObjA>(constraints);
  ispc::UniformConstraintObject objectB = _unwrapUniformConstraintObject<ConstraintObjB>(constraints);
  float* lambdaSum = _unwrapRow<ConstraintData::LambdaSum>(constraints);

  ispc::solveContactConstraints(data, objectA, objectB, lambdaSum, uint32_t(TableOperations::size(constraints)));
}
