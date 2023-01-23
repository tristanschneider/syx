#include "Precompile.h"
#include "ConstraintsTableBuilder.h"

#include "out_ispc/unity.h"

namespace {
  //True if the given element is within a target element in either direction of startIndex
  bool isWithinTargetWidth(const std::vector<StableElementID>& ids, const StableElementID& id, size_t startIndex, size_t targetWidth) {
    const size_t start = startIndex > targetWidth ? startIndex - targetWidth : 0;
    const size_t end = std::min(ids.size(), startIndex + targetWidth);
    for(size_t i = start; i < end; ++i) {
      if(ids[i].mStableID == id.mStableID) {
        return true;
      }
    }
    return false;
  }

  bool isSuitablePairLocation(const CollisionPairIndexA& idA, const CollisionPairIndexB& idB,
    size_t location,
    const StableElementID& desiredA,
    const StableElementID& desiredB,
    size_t targetWidth) {
    return !isWithinTargetWidth(idA.mElements, desiredA, location, targetWidth)
      && !isWithinTargetWidth(idB.mElements, desiredB, location, targetWidth);
  }

  std::optional<size_t> getTargetConstraintTable(const StableElementID& a, const StableElementID& b, const PhysicsTableIds& tables) {
    if((a.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassTable) {
      //If they're both zero mass no constraint is needed
      if((b.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassTable) {
        return {};
      }
      //One is zero mass, solve in zero mass table
      return tables.mZeroMassTable;
    }
    //Neither is zero mass, solve in shared table
    return tables.mSharedMassTable;
  }

  std::pair<size_t, size_t> getTargetElementRange(size_t targetTable, const PhysicsTableIds& tables, const ConstraintsTableMappings& mappings, size_t totalElements) {
    return targetTable == tables.mSharedMassTable ? std::make_pair(size_t(0), mappings.mZeroMassStartIndex) : std::make_pair(mappings.mZeroMassStartIndex, totalElements);
  }

  void assignConstraint(const StableElementID& collisionPair,
    const StableElementID& a,
    const StableElementID& b,
    const StableElementID& constraintLocation,
    CollisionPairsTable& pairs,
    ConstraintCommonTable& constraints,
    const PhysicsTableIds& tables) {
    //Associate collision pair with constraint
    std::get<ConstraintElement>(pairs.mRows).at(collisionPair.mUnstableIndex & tables.mElementIDMask) = constraintLocation;

    const size_t element = constraintLocation.mUnstableIndex & tables.mElementIDMask;
    //Associate constraint with collision pair
    std::get<ConstraintData::ConstraintContactPair>(constraints.mRows).at(element) = collisionPair;
    //Assign the two objects. Their sync indices will be decided later
    std::get<CollisionPairIndexA>(constraints.mRows).at(element) = a;
    std::get<CollisionPairIndexB>(constraints.mRows).at(element) = b;
    std::get<ConstraintData::IsEnabled>(constraints.mRows).at(element) = true;
  }

  void addToFreeList(ConstraintsTableMappings& constraintsMappings,
    StableElementID toAdd,
    ConstraintData::IsEnabled& isEnabled,
    CollisionPairIndexA& constraintIndexA,
    CollisionPairIndexB& constraintIndexB,
    const PhysicsTableIds& tables) {
    const size_t element = toAdd.mUnstableIndex & tables.mElementIDMask;
    isEnabled.at(element) = false;
    constraintIndexA.at(element) = constraintIndexB.at(element) = StableElementID::invalid();
    constraintsMappings.mConstraintFreeList.push_back(toAdd);
  }
}

void ConstraintsTableBuilder::removeCollisionPairs(const StableElementID* toRemove, size_t count, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds) {
  ConstraintData::IsEnabled& isEnabled = std::get<ConstraintData::IsEnabled>(common.mRows);
  StableIDRow& commonStableIds = std::get<StableIDRow>(common.mRows);
  CollisionPairIndexA& constraintIndexA = std::get<CollisionPairIndexA>(common.mRows);
  CollisionPairIndexB& constraintIndexB = std::get<CollisionPairIndexB>(common.mRows);
  const size_t elementMask = tableIds.mElementIDMask;

  for(size_t i = 0; i < count; ++i) {
    if(auto commonElement = StableOperations::tryResolveStableIDWithinTable(toRemove[i], commonStableIds, mappings, elementMask)) {
      addToFreeList(constraintsMappings, *commonElement, isEnabled, constraintIndexA, constraintIndexB, tableIds);
    }
  }
}

StableElementID tryTakeSuitableFreeSlot(size_t startIndex,
  const std::pair<size_t, size_t>& range,
  ConstraintsTableMappings& mappings,
  const StableElementMappings& stableMappings,
  const PhysicsTableIds& tables,
  const StableElementID& a,
  const StableElementID& b,
  const CollisionPairIndexA& constraintIndexA,
  const CollisionPairIndexB& constraintIndexB,
  const StableIDRow& constraintPairIds,
  size_t targetWidth) {
  for(size_t f = startIndex; f < mappings.mConstraintFreeList.size(); ++f) {
    auto freeSlot = StableOperations::tryResolveStableIDWithinTable(mappings.mConstraintFreeList[f], constraintPairIds, stableMappings, tables.mElementIDMask);
    assert(freeSlot.has_value() && "Constraint entries shouldn't disappear");
    if(freeSlot) {
      const size_t freeElement = freeSlot->mUnstableIndex & tables.mElementIDMask;
      if(freeElement >= range.first && freeElement < range.second && isSuitablePairLocation(constraintIndexA, constraintIndexB, freeElement, a, b, targetWidth)) {
        //Found one, use this and swap remove it from the free list
        mappings.mConstraintFreeList[f] = mappings.mConstraintFreeList.back();
        mappings.mConstraintFreeList.pop_back();
        return *freeSlot;
      }
    }
  }
  return StableElementID::invalid();
}

void addPaddingToTable(size_t targetTable,
  size_t amount,
  ConstraintCommonTable& table,
  StableElementMappings& stableMappings,
  const PhysicsTableIds& tableIds,
  ConstraintsTableMappings& constraintMappings) {
  size_t oldSize = TableOperations::size(table);
  size_t startIndex = 0;

  //This is the last table, meaning it can resize off the end
  if(targetTable == tableIds.mZeroMassTable) {
    startIndex = oldSize;
    TableOperations::stableResizeTable(table, UnpackedDatabaseElementID::fromElementMask(tableIds.mElementIDMask, tableIds.mZeroMassTable, size_t(0)), oldSize + amount, stableMappings);
  }
  else if(targetTable == tableIds.mSharedMassTable) {
    //This needs to shift over the start index by adding new entries to the middle
    startIndex = constraintMappings.mZeroMassStartIndex;
    TableOperations::stableInsertRangeAt(table, UnpackedDatabaseElementID::fromElementMask(tableIds.mElementIDMask, tableIds.mZeroMassTable, startIndex), amount, stableMappings);
    constraintMappings.mZeroMassStartIndex += amount;
  }
  else {
    assert("unhandled case");
  }

  //Add all the newly created entries to the free list
  CollisionPairIndexA& constraintIndexA = std::get<CollisionPairIndexA>(table.mRows);
  CollisionPairIndexB& constraintIndexB = std::get<CollisionPairIndexB>(table.mRows);
  ConstraintData::IsEnabled& isEnabled = std::get<ConstraintData::IsEnabled>(table.mRows);
  StableIDRow& constraintPairIds = std::get<StableIDRow>(table.mRows);
  for(size_t i = startIndex; i < startIndex + amount; ++i) {
    UnpackedDatabaseElementID unstable = UnpackedDatabaseElementID::fromElementMask(tableIds.mElementIDMask, tableIds.mConstriantsCommonTable, i);
    addToFreeList(constraintMappings, StableOperations::getStableID(constraintPairIds, unstable), isEnabled, constraintIndexA, constraintIndexB, tableIds);
  }
}

void ConstraintsTableBuilder::addCollisionPairs(const StableElementID* toAdd, size_t count, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds, CollisionPairsTable& pairs) {
  StableIDRow& pairIds = std::get<StableIDRow>(pairs.mRows);
  CollisionPairIndexA& pairIndexA = std::get<CollisionPairIndexA>(pairs.mRows);
  CollisionPairIndexB& pairIndexB = std::get<CollisionPairIndexB>(pairs.mRows);
  CollisionPairIndexA& constraintIndexA = std::get<CollisionPairIndexA>(common.mRows);
  CollisionPairIndexB& constraintIndexB = std::get<CollisionPairIndexB>(common.mRows);
  StableIDRow& constraintPairIds = std::get<StableIDRow>(common.mRows);
  const size_t targetWidth = ispc::getTargetWidth();

  for(size_t i = 0; i < count; ++i) {
    auto pairId = StableOperations::tryResolveStableIDWithinTable(toAdd[i], pairIds, mappings, tableIds.mElementIDMask);
    if(!pairId) {
      continue;
    }

    const size_t element = pairId->mUnstableIndex & tableIds.mElementIDMask;
    //The collision pair is assumed to be ordered properly when added
    const StableElementID a = pairIndexA.at(element);
    const StableElementID b = pairIndexB.at(element);
    const std::optional<size_t> targetTable = getTargetConstraintTable(a, b, tableIds);
    if(!targetTable) {
      continue;
    }

    //Element must be within this index range to pair to the proper constraint type
    const std::pair<size_t, size_t> range = getTargetElementRange(*targetTable, tableIds, constraintsMappings, TableOperations::size(common));
    std::optional<StableElementID> foundSlot;
    //Try the free list for suitable entries
    StableElementID found = tryTakeSuitableFreeSlot(0, range, constraintsMappings, mappings, tableIds, a, b, constraintIndexA, constraintIndexB, constraintPairIds, targetWidth);
    //If that didn't work, make space then try again in the newly created free list entries
    if(found == StableElementID::invalid()) {
      const size_t oldEnd = constraintsMappings.mConstraintFreeList.size();
      addPaddingToTable(*targetTable, targetWidth*2, common, mappings, tableIds, constraintsMappings);
      const std::pair<size_t, size_t> newRange = getTargetElementRange(*targetTable, tableIds, constraintsMappings, TableOperations::size(common));
      found = tryTakeSuitableFreeSlot(oldEnd, newRange, constraintsMappings, mappings, tableIds, a, b, constraintIndexA, constraintIndexB, constraintPairIds, targetWidth);
      assert(found != StableElementID::invalid() && "Space should exist after making space for the element");
    }

    assignConstraint(*pairId, a, b, found, pairs, common, tableIds);
  }
}

struct VisitAttempt {
  StableElementID mDesiredObjectIndex{};
  size_t mDesiredConstraintIndex{};
  std::vector<ConstraintData::VisitData>::iterator mIt;
};

struct ConstraintSyncData {
  int* mSyncIndex{};
  int* mSyncType{};
  StableElementID* mObject{};
};

VisitAttempt _tryVisit(std::vector<ConstraintData::VisitData>& visited, const StableElementID& toVisit, size_t currentConstraintIndex) {
  VisitAttempt result;
  result.mIt = std::lower_bound(visited.begin(), visited.end(), toVisit);
  result.mDesiredConstraintIndex = currentConstraintIndex;
  result.mDesiredObjectIndex = toVisit;
  return result;
}

void _setVisitDataAndTrySetSyncPoint(std::vector<ConstraintData::VisitData>& visited, VisitAttempt& attempt,
  ConstraintSyncData& syncA,
  ConstraintSyncData& syncB,
  ConstraintData::VisitData::Location location) {
  //Set it to nosync for now, later iteration might set this as new constraints are visited
  *(syncA.mSyncType + attempt.mDesiredConstraintIndex) = ispc::NoSync;
  *(syncB.mSyncType + attempt.mDesiredConstraintIndex) = ispc::NoSync;
  if(attempt.mIt == visited.end() || attempt.mIt->mObjectIndex != attempt.mDesiredObjectIndex) {
    //If this is the first time visiting this object, no need to sync anything, but note it for later
    visited.insert(attempt.mIt, ConstraintData::VisitData{
      attempt.mDesiredObjectIndex,
      attempt.mDesiredConstraintIndex,
      location,
      attempt.mDesiredConstraintIndex,
      location
    });
  }
  else {
    //A has been visited before, add a sync index
    const int newLocation = location == ConstraintData::VisitData::Location::InA ? ispc::SyncToIndexA : ispc::SyncToIndexB;
    //Make the previously visited constraint publish the velocity forward to this one
    switch(attempt.mIt->mLocation) {
      case ConstraintData::VisitData::Location::InA:
        *(syncA.mSyncType + attempt.mIt->mConstraintIndex) = newLocation;
        *(syncA.mSyncIndex + attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
      case ConstraintData::VisitData::Location::InB:
        *(syncB.mSyncType + attempt.mIt->mConstraintIndex) = newLocation;
        *(syncB.mSyncIndex + attempt.mIt->mConstraintIndex) = attempt.mDesiredConstraintIndex;
        break;
    }
    //Now that the latest instance of this object is at this visit location, update the visit data
    attempt.mIt->mConstraintIndex = attempt.mDesiredConstraintIndex;
    attempt.mIt->mLocation = location;
  }
}

template<class Obj>
ConstraintSyncData getSyncData(ConstraintCommonTable& table) {
  ConstraintSyncData result {
    TableOperations::_unwrapRowWithOffset<typename ConstraintObject<Obj>::SyncIndex>(table, 0),
    TableOperations::_unwrapRowWithOffset<typename ConstraintObject<Obj>::SyncType>(table, 0),
  };
  if constexpr(std::is_same_v<Obj, ConstraintObjA>) {
    result.mObject = TableOperations::_unwrapRowWithOffset<CollisionPairIndexA>(table, 0);
  }
  else {
    result.mObject = TableOperations::_unwrapRowWithOffset<CollisionPairIndexB>(table, 0);
  }
  return result;
}

//If an object shows up in multiple constraints, sync the velocity data from the last constraint back to the first
void _trySetFinalSyncPoint(const ConstraintData::VisitData& visited, ConstraintSyncData& syncA, ConstraintSyncData& syncB) {
  //If the first is the latest, there is only one instance of this object meaning its velocity was never copied
  if(visited.mFirstConstraintIndex == visited.mConstraintIndex) {
    return;
  }

  //The container to sync from, meaning the final visited entry where the most recent velocity is
  ConstraintSyncData* syncFrom = visited.mLocation == ConstraintData::VisitData::Location::InA ? &syncA : &syncB;

  //Sync from this visited entry back to the first element
  *(syncFrom->mSyncIndex + visited.mConstraintIndex) = visited.mFirstConstraintIndex;
  //Write from the location of the final entry to the location of the first
  *(syncFrom->mSyncType + visited.mConstraintIndex) = std::invoke([&] {
    switch(visited.mFirstLocation) {
      case ConstraintData::VisitData::Location::InA: return ispc::SyncToIndexA;
      case ConstraintData::VisitData::Location::InB: return ispc::SyncToIndexB;
    }
    assert(false);
    return ispc::NoSync;
  });
}

//Fills in syncIndex and syncType. The required padding and target width should already be in place from the previous steps, and object handles resolved
void ConstraintsTableBuilder::buildSyncIndices(ConstraintCommonTable& constraints, const ConstraintsTableMappings& constraintsMappings) {
  ConstraintData::SharedVisitData& visitData = std::get<ConstraintData::SharedVisitDataRow>(constraints.mRows).at();
  std::vector<ConstraintData::VisitData>& visited = visitData.mVisited;
  visited.clear();
  //Need to reserve big enough because visited vector growth would cause iterator invalidation for visitA/visitB cases below
  //Size of pairs is bigger than necessary, it only needs to be the number of objects, but that's not known here and over-allocating
  //a bit isn't a problem
  visited.reserve(TableOperations::size(constraints));

  const auto& isEnabled = std::get<ConstraintData::IsEnabled>(constraints.mRows);
  ConstraintSyncData syncA = getSyncData<ConstraintObjA>(constraints);
  ConstraintSyncData syncB = getSyncData<ConstraintObjB>(constraints);
  for(size_t i = 0; i < isEnabled.size(); ++i) {
    if(!isEnabled.at(i)) {
      continue;
    }

    const size_t localConstraintIndex = i - (i >= constraintsMappings.mZeroMassStartIndex ? constraintsMappings.mZeroMassStartIndex : size_t(0));
    const size_t globalConstraintIndex = i;

    //Sync for A
    VisitAttempt attempt = _tryVisit(visited, *(syncA.mObject + i), globalConstraintIndex);
    _setVisitDataAndTrySetSyncPoint(visited, attempt, syncA, syncB, ConstraintData::VisitData::Location::InA);
    //Sync for B
    attempt = _tryVisit(visited, *(syncB.mObject + i), globalConstraintIndex);
    _setVisitDataAndTrySetSyncPoint(visited, attempt, syncA, syncB, ConstraintData::VisitData::Location::InB);
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
    _trySetFinalSyncPoint(v, syncA, syncB);

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

template<class RowT, class TableT>
void fillConstraintRow(size_t begin, size_t end, const ConstraintCommonTable& common, TableT& constraints, const CollisionPairsTable& pairs) {
  auto& isEnabled = std::get<ConstraintData::IsEnabled>(common.mRows);
  const auto& src = std::get<RowT>(pairs.mRows);
  auto& dst = std::get<RowT>(constraints.mRows);
  const auto& pairIds = std::get<ConstraintData::ConstraintContactPair>(common.mRows);

  for(size_t i = begin; i < end && isEnabled.at(i); ++i) {
    const StableElementID& pairId = pairIds.at(i);
    //dst is the specific constraint table while i is iterating over common table indices. Subtracting begin gets the local index into the table
    dst.at(i - begin) = src.at(pairId.mUnstableIndex);
  }
}

//Fill a row that contains contact points and in the process convert them to rvectors, meaning subtracting the position
template<class ContactT, class PosT, class DstT, class TableT>
void fillRVectorRow(size_t begin, size_t end, const ConstraintCommonTable& common, TableT& constraints, const CollisionPairsTable& pairs) {
  auto& isEnabled = std::get<ConstraintData::IsEnabled>(common.mRows);
  const auto& srcContact = std::get<ContactT>(pairs.mRows);
  const auto& srcCenter = std::get<PosT>(pairs.mRows);
  auto& dst = std::get<DstT>(constraints.mRows);
  const auto& pairIds = std::get<ConstraintData::ConstraintContactPair>(common.mRows);

  for(size_t i = begin; i < end && isEnabled.at(i); ++i) {
    const StableElementID& pairId = pairIds.at(i);
    const size_t d = i - begin;
    //dst is the specific constraint table while i is iterating over common table indices. Subtracting begin gets the local index into the table
    dst.at(i - begin) = srcContact.at(pairId.mUnstableIndex) - srcCenter.at(pairId.mUnstableIndex);
  }
}

template<class TableT>
void fillCommonContactData(size_t begin, size_t end, const ConstraintCommonTable& common, TableT& constraints, const CollisionPairsTable& pairs) {
  fillConstraintRow<ContactPoint<ContactOne>::Overlap>(begin, end, common, constraints, pairs);
  fillConstraintRow<ContactPoint<ContactTwo>::Overlap>(begin, end, common, constraints, pairs);

  fillConstraintRow<SharedNormal::X>(begin, end, common, constraints, pairs);
  fillConstraintRow<SharedNormal::Y>(begin, end, common, constraints, pairs);

  fillRVectorRow<ContactPoint<ContactOne>::PosX, NarrowphaseData<PairA>::PosX, ConstraintObject<ConstraintObjA>::CenterToContactOneX>(begin, end, common, constraints, pairs);
  fillRVectorRow<ContactPoint<ContactOne>::PosY, NarrowphaseData<PairA>::PosY, ConstraintObject<ConstraintObjA>::CenterToContactOneY>(begin, end, common, constraints, pairs);
  fillRVectorRow<ContactPoint<ContactTwo>::PosX, NarrowphaseData<PairA>::PosX, ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(begin, end, common, constraints, pairs);
  fillRVectorRow<ContactPoint<ContactTwo>::PosY, NarrowphaseData<PairA>::PosY, ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(begin, end, common, constraints, pairs);
}

void ConstraintsTableBuilder::fillConstraintNarrowphaseData(const ConstraintCommonTable& constraints, ConstraintsTable& contacts, const CollisionPairsTable& pairs, const ConstraintsTableMappings& constraintsMappings) {
  const size_t begin = constraintsMappings.SHARED_MASS_START_INDEX;
  const size_t end = constraintsMappings.mZeroMassStartIndex;

  fillCommonContactData(begin, end, constraints, contacts, pairs);

  fillRVectorRow<ContactPoint<ContactOne>::PosX, NarrowphaseData<PairB>::PosX, ConstraintObject<ConstraintObjB>::CenterToContactOneX>(begin, end, constraints, contacts, pairs);
  fillRVectorRow<ContactPoint<ContactOne>::PosY, NarrowphaseData<PairB>::PosY, ConstraintObject<ConstraintObjB>::CenterToContactOneY>(begin, end, constraints, contacts, pairs);
  fillRVectorRow<ContactPoint<ContactTwo>::PosX, NarrowphaseData<PairB>::PosX, ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(begin, end, constraints, contacts, pairs);
  fillRVectorRow<ContactPoint<ContactTwo>::PosY, NarrowphaseData<PairB>::PosY, ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(begin, end, constraints, contacts, pairs);
}

void ConstraintsTableBuilder::fillConstraintNarrowphaseData(const ConstraintCommonTable& constraints, ContactConstraintsToStaticObjectsTable& contacts, const CollisionPairsTable& pairs, const ConstraintsTableMappings& constraintsMappings) {
  const size_t begin = constraintsMappings.mZeroMassStartIndex;
  const size_t end = TableOperations::size(constraints);

  fillCommonContactData(begin, end, constraints, contacts, pairs);
}

void ConstraintsTableBuilder::createConstraintTables(const ConstraintCommonTable& common,
  ConstraintsTable& contacts,
  ContactConstraintsToStaticObjectsTable& staticContacts,
  const ConstraintsTableMappings& mappings) {
  TableOperations::resizeTable(contacts, mappings.mZeroMassStartIndex - mappings.SHARED_MASS_START_INDEX);
  TableOperations::resizeTable(staticContacts, TableOperations::size(common) - mappings.mZeroMassStartIndex);
}
