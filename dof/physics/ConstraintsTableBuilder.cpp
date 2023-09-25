#include "Precompile.h"
#include "ConstraintsTableBuilder.h"

#include "AppBuilder.h"

#include "out_ispc/unity.h"

namespace ctbdetails {
  struct WorkOffset {
    size_t mOffset{};
    bool mHasWork{};
  };

  //True if the given element is within a target element in either direction of startIndex
  bool isWithinTargetWidth(const std::vector<StableElementID>& ids, const StableElementID& id, size_t startIndex, size_t targetWidth, const std::pair<size_t, size_t>& range) {
    const size_t start = std::max(range.first, startIndex > targetWidth ? startIndex - targetWidth : 0);
    const size_t end = std::min(range.second, startIndex + targetWidth);
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
    size_t targetWidth,
    const PhysicsTableIds& tableIds,
    size_t targetTable,
    const std::pair<size_t, size_t>& range) {
    //Suitable if neither can be found within the target width, except B can ignore it if B is static
    //Only B would be static due to the way collision pairs are ordered
    return !isWithinTargetWidth(idA.mElements, desiredA, location, targetWidth, range)
      && (targetTable == tableIds.mZeroMassConstraintTable || !isWithinTargetWidth(idB.mElements, desiredB, location, targetWidth, range));
  }

  std::optional<size_t> getTargetConstraintTable(const StableElementID& a, const StableElementID& b, const PhysicsTableIds& tables) {
    if((b.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassObjectTable) {
      //If they're both zero mass no constraint is needed
      if((a.mUnstableIndex & tables.mTableIDMask) == tables.mZeroMassObjectTable) {
        return {};
      }
      //One is zero mass, solve in zero mass table
      return tables.mZeroMassConstraintTable;
    }
    //Neither is zero mass, solve in shared table
    return tables.mSharedMassConstraintTable;
  }

  std::pair<size_t, size_t> getTargetElementRange(size_t targetTable, const PhysicsTableIds& tables, const ConstraintsTableMappings& mappings, size_t totalElements) {
    return targetTable == tables.mSharedMassConstraintTable ? std::make_pair(size_t(0), mappings.mZeroMassStartIndex) : std::make_pair(mappings.mZeroMassStartIndex, totalElements);
  }

  void addToFreeList(ConstraintsTableMappings& constraintsMappings,
    StableElementID toAdd,
    ConstraintData::IsEnabled& isEnabled,
    CollisionPairIndexA& constraintIndexA,
    CollisionPairIndexB& constraintIndexB,
    IIDResolver& ids) {
    const size_t element = ids.uncheckedUnpack(toAdd).getElementIndex();
    CollisionMask::addToConstraintsFreeList(isEnabled.at(element));
    constraintIndexA.at(element) = constraintIndexB.at(element) = StableElementID::invalid();
    constraintsMappings.mConstraintFreeList.push_back(toAdd);
  }

  StableElementID tryTakeSuitableFreeSlot(size_t startIndex,
    size_t targetTable,
    const std::pair<size_t, size_t>& range,
    ConstraintsTableMappings& mappings,
    const PhysicsTableIds& tables,
    const StableElementID& a,
    const StableElementID& b,
    const CollisionPairIndexA& constraintIndexA,
    const CollisionPairIndexB& constraintIndexB,
    IIDResolver& ids,
    size_t targetWidth) {
    for(size_t f = startIndex; f < mappings.mConstraintFreeList.size(); ++f) {
      auto freeSlot = ids.tryResolveStableID(mappings.mConstraintFreeList[f]);
      assert(freeSlot.has_value() && "Constraint entries shouldn't disappear");
      if(freeSlot) {
        const size_t freeElement = freeSlot->mUnstableIndex & tables.mElementIDMask;
        if(freeElement >= range.first && freeElement < range.second && isSuitablePairLocation(constraintIndexA, constraintIndexB, freeElement, a, b, targetWidth, tables, targetTable, range)) {
          //Found one, use this and swap remove it from the free list
          mappings.mConstraintFreeList[f] = mappings.mConstraintFreeList.back();
          mappings.mConstraintFreeList.pop_back();
          return *freeSlot;
        }
      }
    }
    return StableElementID::invalid();
  }
}

namespace ConstraintsTableBuilder {
  struct RemoveDeps {
    static RemoveDeps query(RuntimeDatabaseTaskBuilder& task) {
      RemoveDeps result;
      result.ids = task.getIDResolver();
      auto q = task.query<
        ConstraintData::IsEnabled,
        const StableIDRow,
        CollisionPairIndexA,
        CollisionPairIndexB
      >();
      assert(q.size() == 1);
      result.isEnabled = &q.get<0>(0);
      result.commonStableIds = &q.get<1>(0);
      result.constraintIndexA = &q.get<2>(0);
      result.constraintIndexB = &q.get<3>(0);
      result.constraintsMappings = task.query<SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      return result;
    }

    std::shared_ptr<IIDResolver> ids;
    ConstraintData::IsEnabled* isEnabled{};
    const StableIDRow* commonStableIds{};
    CollisionPairIndexA* constraintIndexA{};
    CollisionPairIndexB* constraintIndexB{};
    ConstraintsTableMappings* constraintsMappings{};
  };

  void removeCollisionPairs(const StableElementID* toRemove, size_t count, RemoveDeps& deps) {
    PROFILE_SCOPE("physics", "removeCollisionPairs");
    for(size_t i = 0; i < count; ++i) {
      if(auto commonElement = deps.ids->tryResolveStableID(toRemove[i])) {
        ctbdetails::addToFreeList(*deps.constraintsMappings, *commonElement, *deps.isEnabled, *deps.constraintIndexA, *deps.constraintIndexB, *deps.ids);
      }
    }
  }

  struct AddDeps {
    static AddDeps query(RuntimeDatabaseTaskBuilder& task) {
      AddDeps result;
      result.ids = task.getIDResolver();
      auto pair = task.query<
        CollisionPairIndexA,
        CollisionPairIndexB,
        ConstraintElement
      >();

      auto constraint = task.query<
        CollisionPairIndexA,
        CollisionPairIndexB,
        const StableIDRow,
        ConstraintData::ConstraintContactPair,
        ConstraintData::IsEnabled
      >();
      assert(pair.size() == constraint.size() == 1);

      result.pairIndexA = &pair.get<0>(0);
      result.pairIndexB = &pair.get<1>(0);
      result.pairElement = &pair.get<2>(0);

      result.constraintIndexA = &constraint.get<0>(0);
      result.constraintIndexB = &constraint.get<1>(0);
      result.constraintPairIds = &constraint.get<2>(0);
      result.constraintContactPair = &constraint.get<3>(0);
      result.constraintEnabled = &constraint.get<4>(0);

      result.constraintsMappings = task.query<SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      result.commonTable = constraint.matchingTableIDs[0];
      result.commonTableModifier = task.getModifierForTable(result.commonTable);
      return result;
    }

    std::shared_ptr<IIDResolver> ids;
    CollisionPairIndexA* pairIndexA{};
    CollisionPairIndexB* pairIndexB{};
    ConstraintElement* pairElement{};

    CollisionPairIndexA* constraintIndexA{};
    CollisionPairIndexB* constraintIndexB{};
    const StableIDRow* constraintPairIds{};
    ConstraintData::ConstraintContactPair* constraintContactPair{};
    ConstraintData::IsEnabled* constraintEnabled{};
    ConstraintsTableMappings* constraintsMappings{};
    std::shared_ptr<ITableModifier> commonTableModifier;
    UnpackedDatabaseElementID commonTable;
  };

  void assignConstraint(const StableElementID& collisionPair,
    const StableElementID& a,
    const StableElementID& b,
    const StableElementID& constraintLocation,
    AddDeps& deps) {
    //Associate collision pair with constraint
    deps.pairElement->at(deps.ids->uncheckedUnpack(collisionPair).getElementIndex()) = constraintLocation;

    const size_t element = deps.ids->uncheckedUnpack(constraintLocation).getElementIndex();
    //Associate constraint with collision pair
    deps.constraintContactPair->at(element) = collisionPair;
    //Assign the two objects. Their sync indices will be decided later
    deps.constraintIndexA->at(element) = a;
    deps.constraintIndexB->at(element) = b;
    CollisionMask::removeConstraintFromFreeList(deps.constraintEnabled->at(element));
  }

  void addPaddingToTable(size_t targetTable,
    size_t amount,
    const PhysicsTableIds& tableIds,
    AddDeps& deps) {
    size_t oldSize = deps.constraintEnabled->size();
    size_t startIndex = 0;

    //This is the last table, meaning it can resize off the end
    if(targetTable == tableIds.mZeroMassConstraintTable) {
      startIndex = oldSize;
      deps.commonTableModifier->resize(oldSize + amount);
    }
    else if(targetTable == tableIds.mSharedMassConstraintTable) {
      //This needs to shift over the start index by adding new entries to the middle
      startIndex = deps.constraintsMappings->mZeroMassStartIndex;
      deps.commonTableModifier->insert(deps.commonTable.remakeElement(startIndex), amount);
      deps.constraintsMappings->mZeroMassStartIndex += amount;
    }
    else {
      assert("unhandled case");
    }

    //Add all the newly created entries to the free list
    CollisionPairIndexA& constraintIndexA = *deps.constraintIndexA;
    CollisionPairIndexB& constraintIndexB = *deps.constraintIndexB;
    ConstraintData::IsEnabled& isEnabled = *deps.constraintEnabled;
    const StableIDRow& constraintPairIds = *deps.constraintPairIds;
    for(size_t i = startIndex; i < startIndex + amount; ++i) {
      UnpackedDatabaseElementID unstable = deps.commonTable.remakeElement(i);
      ctbdetails::addToFreeList(*deps.constraintsMappings, StableOperations::getStableID(constraintPairIds, unstable), isEnabled, constraintIndexA, constraintIndexB, *deps.ids);
    }
  }

  //void addCollisionPairs(const StableElementID* toAdd, size_t count, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds, CollisionPairsTable& pairs, const Config::PhysicsConfig& config) {
  void addCollisionPairs(const StableElementID* toAdd, size_t count, AddDeps& deps, const PhysicsTableIds& tableIds, const Config::PhysicsConfig& config) {
    PROFILE_SCOPE("physics", "addCollisionPairs");
    using namespace ctbdetails;
    const size_t targetWidth = config.mForcedTargetWidth.value_or(ispc::getTargetWidth());

    for(size_t i = 0; i < count; ++i) {
      auto pairId = deps.ids->tryResolveStableID(toAdd[i]);
      if(!pairId) {
        continue;
      }

      const size_t element = deps.ids->uncheckedUnpack(*pairId).getElementIndex();
      //The collision pair is assumed to be ordered properly when added
      const StableElementID a = deps.pairIndexA->at(element);
      const StableElementID b = deps.pairIndexB->at(element);
      const std::optional<size_t> targetTable = getTargetConstraintTable(a, b, tableIds);
      if(!targetTable) {
        continue;
      }

      //Element must be within this index range to pair to the proper constraint type
      const std::pair<size_t, size_t> range = getTargetElementRange(*targetTable, tableIds, *deps.constraintsMappings, deps.constraintEnabled->size());
      std::optional<StableElementID> foundSlot;
      //Try the free list for suitable entries
      StableElementID found = tryTakeSuitableFreeSlot(0, *targetTable, range, *deps.constraintsMappings, tableIds, a, b, *deps.constraintIndexA, *deps.constraintIndexB, *deps.ids, targetWidth);
      //If that didn't work, make space then try again in the newly created free list entries
      if(found == StableElementID::invalid()) {
        const size_t oldEnd = deps.constraintsMappings->mConstraintFreeList.size();
        addPaddingToTable(*targetTable, std::max(size_t(1), targetWidth*2), tableIds, deps);
        const std::pair<size_t, size_t> newRange = getTargetElementRange(*targetTable, tableIds, *deps.constraintsMappings, deps.constraintEnabled->size());
        found = tryTakeSuitableFreeSlot(oldEnd, *targetTable, newRange, *deps.constraintsMappings, tableIds, a, b, *deps.constraintIndexA, *deps.constraintIndexB, *deps.ids, targetWidth);
        assert(found != StableElementID::invalid() && "Space should exist after making space for the element");
      }

      assignConstraint(*pairId, a, b, found, deps);
    }
  }

  struct VisitAttempt {
    StableElementID mDesiredObjectIndex{};
    size_t mDesiredConstraintIndex{};
    std::unordered_map<size_t, ConstraintData::VisitData>::iterator mIt;
  };

  struct ConstraintSyncData {
    int* mSyncIndex{};
    int* mSyncType{};
    StableElementID* mObject{};
  };

  VisitAttempt _tryVisit(std::unordered_map<size_t, ConstraintData::VisitData>& visited, const StableElementID& toVisit, size_t currentConstraintIndex) {
    VisitAttempt result;
    result.mIt = visited.find(toVisit.mStableID);
    result.mDesiredConstraintIndex = currentConstraintIndex;
    result.mDesiredObjectIndex = toVisit;
    return result;
  }

  void _setVisitDataAndTrySetSyncPoint(std::unordered_map<size_t, ConstraintData::VisitData>& visited, VisitAttempt& attempt,
    ConstraintSyncData& syncA,
    ConstraintSyncData& syncB,
    ConstraintData::VisitData::Location location) {
    //Set it to nosync for now, later iteration might set this as new constraints are visited
    *(syncA.mSyncType + attempt.mDesiredConstraintIndex) = ispc::NoSync;
    *(syncB.mSyncType + attempt.mDesiredConstraintIndex) = ispc::NoSync;
    if(attempt.mIt == visited.end() || attempt.mIt->second.mObjectIndex != attempt.mDesiredObjectIndex) {
      //If this is the first time visiting this object, no need to sync anything, but note it for later
      visited.insert(attempt.mIt, std::make_pair(attempt.mDesiredObjectIndex.mStableID, ConstraintData::VisitData{
        attempt.mDesiredObjectIndex,
        attempt.mDesiredConstraintIndex,
        location,
        attempt.mDesiredConstraintIndex,
        location
      }));
    }
    else {
      //A has been visited before, add a sync index
      const int newLocation = location == ConstraintData::VisitData::Location::InA ? ispc::SyncToIndexA : ispc::SyncToIndexB;
      //Make the previously visited constraint publish the velocity forward to this one
      switch(attempt.mIt->second.mLocation) {
        case ConstraintData::VisitData::Location::InA:
          *(syncA.mSyncType + attempt.mIt->second.mConstraintIndex) = newLocation;
          *(syncA.mSyncIndex + attempt.mIt->second.mConstraintIndex) = attempt.mDesiredConstraintIndex;
          break;
        case ConstraintData::VisitData::Location::InB:
          *(syncB.mSyncType + attempt.mIt->second.mConstraintIndex) = newLocation;
          *(syncB.mSyncIndex + attempt.mIt->second.mConstraintIndex) = attempt.mDesiredConstraintIndex;
          break;
      }
      //Now that the latest instance of this object is at this visit location, update the visit data
      attempt.mIt->second.mConstraintIndex = attempt.mDesiredConstraintIndex;
      attempt.mIt->second.mLocation = location;
    }
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
  //void buildSyncIndices(ConstraintCommonTable& constraints, const ConstraintsTableMappings&) {
  void buildSyncIndices(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("build sync indices");
    auto query = task.query<
      ConstraintData::SharedVisitDataRow,
      SharedRow<FinalSyncIndices>,
      ConstraintData::IsEnabled,
      ConstraintObject<ConstraintObjA>::SyncIndex,
      ConstraintObject<ConstraintObjA>::SyncType,
      CollisionPairIndexA,
      ConstraintObject<ConstraintObjB>::SyncIndex,
      ConstraintObject<ConstraintObjB>::SyncType,
      CollisionPairIndexB
    >();
    assert(query.size() == 1);

    task.setCallback([query](AppTaskArgs&) mutable {
      ConstraintData::SharedVisitData& visitData = query.get<0>(0).at();
      const auto& isEnabled = query.get<ConstraintData::IsEnabled>(0);
      std::unordered_map<size_t, ConstraintData::VisitData>& visited = visitData.mVisited;
      visited.clear();
      //Need to reserve big enough because visited vector growth would cause iterator invalidation for visitA/visitB cases below
      //Size of pairs is bigger than necessary, it only needs to be the number of objects, but that's not known here and over-allocating
      //a bit isn't a problem
      visited.reserve(isEnabled.size());

      ConstraintSyncData syncA{ query.get<3>(0).data(), query.get<4>(0).data(), query.get<5>(0).data() };
      ConstraintSyncData syncB{ query.get<6>(0).data(), query.get<7>(0).data(), query.get<8>(0).data() };
      for(size_t i = 0; i < isEnabled.size(); ++i) {
        //This is before the masks are combined
        if(!CollisionMask::isConstraintEnabled(isEnabled.at(i))) {
          continue;
        }

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
      FinalSyncIndices& finalData = query.get<SharedRow<FinalSyncIndices>>(0).at();
      finalData.mMappingsA.clear();
      finalData.mMappingsB.clear();
      for(const auto& pair : visited) {
        auto& v = pair.second;
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
    });
    builder.submitTask(std::move(task));
  }

  template<class RowT>
  void fillConstraintRow(IAppBuilder& builder, std::shared_ptr<ctbdetails::WorkOffset> offset, const UnpackedDatabaseElementID& constraintTable, std::vector<std::shared_ptr<AppTaskConfig>>& configs) {
    auto task = builder.createTask();
    task.setName("fill constraint row");
    configs.push_back(task.getConfig());
    auto common = task.query<
      const ConstraintData::IsEnabled,
      const ConstraintData::ConstraintContactPair>();
    auto srcQuery = task.query<const RowT, const NarrowphaseTableTag>();
    auto dstQuery = task.query<RowT>(constraintTable);
    auto ids = task.getIDResolver();
    assert(common.size() == srcQuery.size() == dstQuery.size() == 1);

    task.setCallback([offset, common, srcQuery, dstQuery, ids](AppTaskArgs& args) mutable {
      PROFILE_SCOPE("physics", "fillConstraintRow");
      const auto& isEnabled = common.get<0>(0);
      const auto& src = srcQuery.get<0>(0);
      auto& dst = dstQuery.get<0>(0);
      const auto& pairIds = common.get<1>(0);

      const auto& o = *offset;
      if(!o.mHasWork) {
        return;
      }
      const size_t begin = args.begin + o.mOffset;
      const size_t end = args.end + o.mOffset;
      for(size_t i = begin; i < end; ++i) {
        if(CollisionMask::shouldSolveConstraint(isEnabled.at(i))) {
          const StableElementID& pairId = pairIds.at(i);
          //dst is the specific constraint table while i is iterating over common table indices. Subtracting begin gets the local index into the table
          dst.at(i - o.mOffset) = src.at(ids->uncheckedUnpack(pairId).getElementIndex());
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  //Set constraint as disabled if collision mask is empty
  void fillCollisionMasks(IAppBuilder& builder, std::shared_ptr<ctbdetails::WorkOffset> offset, std::vector<std::shared_ptr<AppTaskConfig>>& configs) {
    auto task = builder.createTask();
    task.setName("fill collision masks");
    configs.push_back(task.getConfig());
    auto commonQuery = task.query<
      ConstraintData::IsEnabled,
      const ConstraintData::ConstraintContactPair>();
    auto narrowphase = task.query<const CollisionMaskRow, const NarrowphaseTableTag>();
    assert(commonQuery.size() == narrowphase.size() == 1);
    auto ids = task.getIDResolver();

    task.setCallback([commonQuery, narrowphase, offset, ids](AppTaskArgs& args) mutable {
      auto& isEnabled = commonQuery.get<0>(0);
      const auto& src = narrowphase.get<0>(0);
      const auto& pairIds = commonQuery.get<1>(0);

      const auto& o = *offset;
      if(!o.mHasWork) {
        return;
      }
      const size_t begin = args.begin + o.mOffset;
      const size_t end = args.end + o.mOffset;
      for(size_t i = begin; i < end; ++i) {
        uint8_t& enabled = isEnabled.at(i);
        if(CollisionMask::isConstraintEnabled(enabled)) {
          const StableElementID& pairId = pairIds.at(i);
          enabled = CollisionMask::combineForConstraintsTable(enabled, src.at(ids->uncheckedUnpack(pairId).getElementIndex()));
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  //Fill a row that contains contact points and in the process convert them to rvectors, meaning subtracting the position
  template<class ContactT, class PosT, class DstT>
  void fillRVectorRow(IAppBuilder& builder, std::shared_ptr<ctbdetails::WorkOffset> offset, const UnpackedDatabaseElementID& constraintTable, std::vector<std::shared_ptr<AppTaskConfig>>& configs) {
    auto task = builder.createTask();
    task.setName("fill rvector row");
    configs.push_back(task.getConfig());
    auto common = task.query<
      const ConstraintData::IsEnabled,
      const ConstraintData::ConstraintContactPair>();
    auto srcQuery = task.query<const ContactT, const PosT, const NarrowphaseTableTag>();
    auto dstQuery = task.query<DstT>(constraintTable);
    auto ids = task.getIDResolver();
    assert(common.size() == srcQuery.size() == dstQuery.size() == 1);

    task.setCallback([offset, common, srcQuery, dstQuery, ids](AppTaskArgs& args) mutable {
      PROFILE_SCOPE("physics", "fillConstraintRow");
      const auto& isEnabled = common.get<0>(0);
      const auto& srcContact = srcQuery.get<0>(0);
      const auto& srcCenter = srcQuery.get<1>(0);
      auto& dst = dstQuery.get<0>(0);
      const auto& pairIds = common.get<1>(0);

      const auto& o = *offset;
      if(!o.mHasWork) {
        return;
      }
      const size_t begin = args.begin + o.mOffset;
      const size_t end = args.end + o.mOffset;
      for(size_t i = begin; i < end; ++i) {
        if(CollisionMask::shouldSolveConstraint(isEnabled.at(i))) {
          const StableElementID& pairId = pairIds.at(i);
          //dst is the specific constraint table while i is iterating over common table indices. Subtracting begin gets the local index into the table
          const size_t pi = ids->uncheckedUnpack(pairId).getElementIndex();
          dst.at(i - o.mOffset) = srcContact.at(pi) - srcCenter.at(pi);
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void fillCommonContactData(IAppBuilder& builder, std::shared_ptr<ctbdetails::WorkOffset> begin, const UnpackedDatabaseElementID& constraintTable, std::vector<std::shared_ptr<AppTaskConfig>>& config) {
    fillConstraintRow<ContactPoint<ContactOne>::Overlap>(builder, begin, constraintTable, config);
    fillConstraintRow<ContactPoint<ContactTwo>::Overlap>(builder, begin, constraintTable, config);

    fillConstraintRow<SharedNormal::X>(builder, begin, constraintTable, config);
    fillConstraintRow<SharedNormal::Y>(builder, begin, constraintTable, config);

    fillRVectorRow<ContactPoint<ContactOne>::PosX, NarrowphaseData<PairA>::PosX, ConstraintObject<ConstraintObjA>::CenterToContactOneX>(builder, begin, constraintTable, config);
    fillRVectorRow<ContactPoint<ContactOne>::PosY, NarrowphaseData<PairA>::PosY, ConstraintObject<ConstraintObjA>::CenterToContactOneY>(builder, begin, constraintTable, config);
    fillRVectorRow<ContactPoint<ContactTwo>::PosX, NarrowphaseData<PairA>::PosX, ConstraintObject<ConstraintObjA>::CenterToContactTwoX>(builder, begin, constraintTable, config);
    fillRVectorRow<ContactPoint<ContactTwo>::PosY, NarrowphaseData<PairA>::PosY, ConstraintObject<ConstraintObjA>::CenterToContactTwoY>(builder, begin, constraintTable, config);
  }

  void fillContactConstraintNarrowphaseData(IAppBuilder& builder) {
    const UnpackedDatabaseElementID destinationTable = builder.queryTables<SharedMassConstraintsTableTag>().matchingTableIDs[0];

    std::shared_ptr<std::vector<std::shared_ptr<AppTaskConfig>>> cc = std::make_shared<std::vector<std::shared_ptr<AppTaskConfig>>>();
    auto sharedBegin = std::make_shared<ctbdetails::WorkOffset>();
    {
      auto configure = builder.createTask();
      configure.setName("configure fill");
      const ConstraintsTableMappings* constraintsMappings = configure.query<const SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      //Artificial dependency to make all children wait since they read/write this
      configure.query<ConstraintData::IsEnabled>();

      configure.setCallback([sharedBegin, cc, constraintsMappings](AppTaskArgs&) mutable {
        const size_t begin = constraintsMappings->SHARED_MASS_START_INDEX;
        const size_t end = constraintsMappings->mZeroMassStartIndex;
        const size_t size = end - begin;
        AppTaskSize taskSize;
        taskSize.batchSize = 100;
        taskSize.workItemCount = size;
        for(auto&& c : *cc) {
          c->setSize(taskSize);
        }
      });

      builder.submitTask(std::move(configure));
    }

    auto childConfigs = *cc;

    fillCollisionMasks(builder, sharedBegin, childConfigs);

    fillCommonContactData(builder, sharedBegin, destinationTable, childConfigs);

    fillRVectorRow<ContactPoint<ContactOne>::PosX, NarrowphaseData<PairB>::PosX, ConstraintObject<ConstraintObjB>::CenterToContactOneX>(builder, sharedBegin, destinationTable, childConfigs);
    fillRVectorRow<ContactPoint<ContactOne>::PosY, NarrowphaseData<PairB>::PosY, ConstraintObject<ConstraintObjB>::CenterToContactOneY>(builder, sharedBegin, destinationTable, childConfigs);
    fillRVectorRow<ContactPoint<ContactTwo>::PosX, NarrowphaseData<PairB>::PosX, ConstraintObject<ConstraintObjB>::CenterToContactTwoX>(builder, sharedBegin, destinationTable, childConfigs);
    fillRVectorRow<ContactPoint<ContactTwo>::PosY, NarrowphaseData<PairB>::PosY, ConstraintObject<ConstraintObjB>::CenterToContactTwoY>(builder, sharedBegin, destinationTable, childConfigs);
  }

  void fillStaticContactConstraintNarrowphaseData(IAppBuilder& builder) {
    const UnpackedDatabaseElementID destinationTable = builder.queryTables<ContactConstraintsToStaticObjectsTable>().matchingTableIDs[0];

    std::shared_ptr<std::vector<std::shared_ptr<AppTaskConfig>>> cc = std::make_shared<std::vector<std::shared_ptr<AppTaskConfig>>>();
    auto sharedBegin = std::make_shared<ctbdetails::WorkOffset>();
    {
      auto configure = builder.createTask();
      configure.setName("configure fill");
      const ConstraintsTableMappings* constraintsMappings = configure.query<const SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
      //Artificial dependency to make all children wait since they read/write this
      auto common = configure.query<ConstraintData::IsEnabled>();

      configure.setCallback([sharedBegin, cc, constraintsMappings, common](AppTaskArgs&) mutable {
        const size_t begin = constraintsMappings->mZeroMassStartIndex;
        const size_t end = common.get<0>(0).size();
        const size_t size = end - begin;
        AppTaskSize taskSize;
        taskSize.batchSize = 100;
        taskSize.workItemCount = size;
        for(auto&& c : *cc) {
          c->setSize(taskSize);
        }
      });

      builder.submitTask(std::move(configure));
    }

    auto childConfigs = *cc;

    fillCollisionMasks(builder, sharedBegin, childConfigs);
    fillCommonContactData(builder, sharedBegin, destinationTable, childConfigs);
  }

  void createConstraintTables(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("create constraint tables");
    const ConstraintsTableMappings* mappings = task.query<const SharedRow<ConstraintsTableMappings>>().tryGetSingletonElement();
    auto common = task.query<const ConstraintsCommonTableTag>();
    auto staticContacts = task.query<const ZeroMassConstraintsTableTag, ConstraintData::CommonTableStartIndex>();
    auto contacts = task.query<const SharedMassConstraintsTableTag, ConstraintData::CommonTableStartIndex>();
    assert(common.size() == staticContacts.size() == contacts.size() == 1);
    auto staticModifier = task.getModifierForTable(staticContacts.matchingTableIDs[0]);
    auto contactModifier = task.getModifierForTable(contacts.matchingTableIDs[0]);

    task.setCallback([staticModifier, contactModifier, common, mappings, contacts, staticContacts](AppTaskArgs&) mutable {
      contactModifier->resize(mappings->mZeroMassStartIndex - mappings->SHARED_MASS_START_INDEX);
      staticModifier->resize(common.get<0>(0).size() - mappings->mZeroMassStartIndex);

      contacts.get<1>(0).at() = mappings->SHARED_MASS_START_INDEX;
      staticContacts.get<1>(0).at() = mappings->mZeroMassStartIndex;
    });

    builder.submitTask(std::move(task));
  }

  //Resolve the object handles in the constraints common table, making them ready for the upcoming row by row extraction of velocity data
  //For any objects that moved tables, the constraint entry is reinserted
  //For this to work properly the handles on the contact entry should be up to date so that upon reinsertion the StableElementID of the pair is resolved.
  //void resolveObjectHandles(DatabaseT& db, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds, CollisionPairsTable& pairs, const Config::PhysicsConfig& config) {
  void resolveObjectHandles(IAppBuilder& builder, const PhysicsTableIds& tableIds, const Config::PhysicsConfig& config) {
    auto task = builder.createTask();
    auto commonQuery = task.query<
      const ConstraintData::IsEnabled,
      CollisionPairIndexA,
      CollisionPairIndexB,
      ConstraintData::ConstraintContactPair,
      const StableIDRow>();
    auto ids = task.getIDResolver();
    RemoveDeps removeDeps = RemoveDeps::query(task);
    AddDeps addDeps = AddDeps::query(task);

    task.setCallback([commonQuery, ids, tableIds, removeDeps, &config, addDeps](AppTaskArgs&) mutable {
      const ConstraintData::IsEnabled& isEnabled = commonQuery.get<0>(0);
      CollisionPairIndexA& objA = commonQuery.get<1>(0);
      CollisionPairIndexB& objB = commonQuery.get<2>(0);
      ConstraintData::ConstraintContactPair& contacts = commonQuery.get<3>(0);
      const StableIDRow& stableIds = commonQuery.get<4>(0);
      const UnpackedDatabaseElementID constraintCommonTable = commonQuery.matchingTableIDs[0];

      bool anyRemovals = false;
      for(size_t i = 0; i < objA.size(); ++i) {
        //Skip disabled constraints, meaning padding or free list elements
        if(!CollisionMask::isConstraintEnabled(isEnabled.at(i))) {
          continue;
        }
        //Resolve the new handles
        StableElementID& prevA = objA.at(i);
        std::optional<StableElementID> newA = ids->tryResolveStableID(prevA);
        StableElementID& prevB = objB.at(i);
        std::optional<StableElementID> newB = ids->tryResolveStableID(prevB);
        StableElementID& contact = contacts.at(i);
        const std::optional<StableElementID> newContact = ids->tryResolveStableID(contact);

        //If the pair changed such that no collision resolution is necessary, remove the pair
        const bool needsRemove = !newA || !newB || !CollisionPairOrder::tryOrderCollisionPair(*newA, *newB, tableIds) || !newContact;
        //Reinsert if the order of the pair changed from tryOrder above
        const bool needsReinsert = !needsRemove && newA->mStableID != prevA.mStableID;

        //Update to the resolved entries. This doesn't matter if the remove/reinsert cases happen below
        if(newA && newB && newContact) {
          prevA = *newA;
          prevB = *newB;
          contact = *newContact;
        }

        const UnpackedDatabaseElementID unpackedConstraint = constraintCommonTable.remakeElement(i);
        const StableElementID thisConstraint = StableOperations::getStableID(stableIds, unpackedConstraint);

        //Remove this if necessary, adding to the free list
        if(needsRemove || needsReinsert) {
          removeCollisionPairs(&thisConstraint, 1, removeDeps);
          anyRemovals = true;
        }
        if(needsReinsert) {
          //Reinsert if desired. This may be able to use the entry that was just added to the free list,
          //meaning that the table size wouldn't have to change.
          //If this went from nonstatic to static or is no longer satisfying padding then the insert location won't use the free list
          addCollisionPairs(&contact, 1, addDeps, tableIds, config);
        }
      }

      //Any removals can invalidate the ids of collision pairs, so require another pass to re-resolve
      //This shouldn't be required more than once. Resolving object handles again is unnecessary since they aren't affected by collision pair removal
      if(anyRemovals) {
        for(size_t i = 0; i < objA.size(); ++i) {
          if(isEnabled.at(i)) {
            StableElementID& contact = contacts.at(i);
            contact = *ids->tryResolveStableID(contact);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  const SweepNPruneBroadphase::ChangedCollisionPairs* readChangedPairs(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>().tryGetSingletonElement();
  }

  //Given ids of a bunch of collision pairs, add entries for them in the constraints table
  //This will try to use the free list and ensures that the insertion location is not within a target width of objects of the same indices
  //Doing so may cause padding or a shift in where the start index of a given constraint table is in the common constraints table
  void addCollisionPairs(IAppBuilder& builder, const PhysicsTableIds& tableIds, const Config::PhysicsConfig& config) {
    auto task = builder.createTask();
    task.setName("add collision pairs");
    const SweepNPruneBroadphase::ChangedCollisionPairs* pairs = readChangedPairs(task);
    AddDeps deps = AddDeps::query(task);

    task.setCallback([pairs, deps, tableIds, &config](AppTaskArgs&) mutable {
      addCollisionPairs(pairs->mGained.data(), pairs->mGained.size(), deps, tableIds, config);
    });

    builder.submitTask(std::move(task));
  }

  //Mark the given constraints identified by their element ids as removed
  //The id is pointing at the elements in the constraint common table
  void removeCollisionPairs(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("remove collision pairs");
    const SweepNPruneBroadphase::ChangedCollisionPairs* pairs = readChangedPairs(task);
    RemoveDeps deps = RemoveDeps::query(task);

    task.setCallback([pairs, deps](AppTaskArgs&) mutable {
      removeCollisionPairs(pairs->mLost.data(), pairs->mLost.size(), deps);
    });

    builder.submitTask(std::move(task));
  }

  void clearChangedCollisionPairs(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("clear changed pairs");
    SweepNPruneBroadphase::ChangedCollisionPairs* pairs = task.query<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>().tryGetSingletonElement();
    task.setCallback([pairs](AppTaskArgs&) {
      pairs->mGained.clear();
      pairs->mLost.clear();
    });
    builder.submitTask(std::move(task));
  }

  void build(IAppBuilder& builder, const Config::PhysicsConfig& config) {
    const PhysicsTableIds tableIds = Physics::getTableIds(builder);
    addCollisionPairs(builder, tableIds, config);
    removeCollisionPairs(builder);
    clearChangedCollisionPairs(builder);
    resolveObjectHandles(builder, tableIds, config);
    buildSyncIndices(builder);
    createConstraintTables(builder);
    fillContactConstraintNarrowphaseData(builder);
    fillStaticContactConstraintNarrowphaseData(builder);
  }
}