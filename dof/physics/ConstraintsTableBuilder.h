#pragma once

#include "Physics.h"
#include "PhysicsTableIds.h"
#include "SweepNPruneBroadphase.h"

//First inserted and removed collision pairs are populated from the broadphase
//Insertions update the common constraints table and ensure no two velocities are within the target width of each other
//Removals mark as unused and add to a free list
//Insertions and removals both don't update the sync indices yet

//Next, handles to both objects from the collision pairs via CollisionPairIndexA/B are resolved, along with the handles to the contacts
//This also indicates if they changed tables, which may require reordering the collision pairs in which case they are reinserted

//Then the sync indices and sync types are filled in
//Row by row constraint velocities can be filled in in the common table

//At this point, all objects are known, padding applied, sync indices set, and velocities filled,
//The individual constraint tables can be resized to fit the counts from the previous steps, none of the data needs to be persistent
//Next, narrowphase data is filled in row by row to the specific constraint tables

struct ConstraintsTableMappings {
  static constexpr size_t SHARED_MASS_START_INDEX = 0;

  std::vector<StableElementID> mConstraintFreeList;
  size_t mZeroMassStartIndex{};
};

struct ConstraintsTableBuilder {
  //Mark the given constraints identified by their element ids as removed
  //The id is pointing at the elements in the constraint common table
  static void removeCollisionPairs(const StableElementID* toRemove, size_t count, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds);
  //Given ids of a bunch of collision pairs, add entries for them in the constraints table
  //This will try to use the free list and ensures that the insertion location is not within a target width of objects of the same indices
  //Doing so may cause padding or a shift in where the start index of a given constraint table is in the common constraints table
  static void addCollisionPairs(const StableElementID* toAdd, size_t count, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds, CollisionPairsTable& pairs);

  //Resolve the object handles in the constraints common table, making them ready for the upcoming row by row extraction of velocity data
  //For any objects that moved tables, the constraint entry is reinserted
  //For this to work properly the handles on the contact entry should be up to date so that upon reinsertion the StableElementID of the pair is resolved.
  template<class DatabaseT>
  static void resolveObjectHandles(DatabaseT& db, StableElementMappings& mappings, ConstraintCommonTable& common, ConstraintsTableMappings& constraintsMappings, const PhysicsTableIds& tableIds, CollisionPairsTable& pairs) {
    const auto& isEnabled = std::get<ConstraintData::IsEnabled>(common.mRows);
    auto& objA = std::get<CollisionPairIndexA>(common.mRows);
    auto& objB = std::get<CollisionPairIndexB>(common.mRows);
    auto& contacts = std::get<ConstraintData::ConstraintContactPair>(common.mRows);
    const auto& stableIds = std::get<StableIDRow>(common.mRows);

    const auto& contactStableIds = std::get<StableIDRow>(pairs.mRows);

    bool anyRemovals = false;
    for(size_t i = 0; i < objA.size(); ++i) {
      //Skip disabled constraints, meaning padding or free list elements
      if(!isEnabled.at(i)) {
        continue;
      }
      //Resolve the new handles
      StableElementID& prevA = objA.at(i);
      std::optional<StableElementID> newA = StableOperations::tryResolveStableID(prevA, db, mappings);
      StableElementID& prevB = objB.at(i);
      std::optional<StableElementID> newB = StableOperations::tryResolveStableID(prevB, db, mappings);
      StableElementID& contact = contacts.at(i);
      const std::optional<StableElementID> newContact = StableOperations::tryResolveStableIDWithinTable(contact, contactStableIds, mappings, tableIds.mElementIDMask);

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

      const UnpackedDatabaseElementID unpackedConstraint = UnpackedDatabaseElementID::fromElementMask(tableIds.mElementIDMask, tableIds.mConstriantsCommonTable, i);
      const StableElementID thisConstraint = StableOperations::getStableID(stableIds, unpackedConstraint);

      //Remove this if necessary, adding to the free list
      if(needsRemove || needsReinsert) {
        removeCollisionPairs(&contact, 1, mappings, common, constraintsMappings, tableIds);
        anyRemovals = true;
      }
      if(needsReinsert) {
        //Reinsert if desired. This should be able to use the entry that was just added to the free list,
        //meaning that the table size shouldn't have to change. Reinsert vs modifying the entry is used for simplicity
        [[maybe_unused]] const size_t prev = objA.size();
        addCollisionPairs(&contact, 1, mappings, common,constraintsMappings, tableIds, pairs);
        assert(prev == objA.size());
      }
    }

    //Any removals can invalidate the ids of collision pairs, so require another pass to re-resolve
    //This shouldn't be required more than once. Resolving object handles again is unnecessary since they aren't affected by collision pair removal
    if(anyRemovals) {
      for(size_t i = 0; i < objA.size() && isEnabled.at(i); ++i) {
        StableElementID& contact = contacts.at(i);
        contact = *StableOperations::tryResolveStableIDWithinTable(contact, contactStableIds, mappings, tableIds.mElementIDMask);
      }
    }
  }

  static void buildSyncIndices(ConstraintCommonTable& constraints, const ConstraintsTableMappings& constraintsMappings);

  static void createConstraintTables(const ConstraintCommonTable& common,
    ConstraintsTable& contacts,
    ContactConstraintsToStaticObjectsTable& staticContacts,
    const ConstraintsTableMappings& mappings);
  static void fillConstraintNarrowphaseData(const ConstraintCommonTable& constraints, ConstraintsTable& contacts, const CollisionPairsTable& pairs, const ConstraintsTableMappings& constraintsMappings);
  static void fillConstraintNarrowphaseData(const ConstraintCommonTable& constraints, ContactConstraintsToStaticObjectsTable& contacts, const CollisionPairsTable& pairs, const ConstraintsTableMappings& constraintsMappings);

  //Do all of the above to end up with constraints ready to solve except for filling in velocity
  template<class DatabaseT>
  static void build(DatabaseT& db,
    SweepNPruneBroadphase::ChangedCollisionPairs& changedCollisionPairs,
    StableElementMappings& stableMappings,
    ConstraintsTableMappings& constraintMappings,
    const PhysicsTableIds& tableIds) {
    auto& common = std::get<ConstraintCommonTable>(db.mTables);
    auto& pairs = std::get<CollisionPairsTable>(db.mTables);
    auto& contacts = std::get<ConstraintsTable>(db.mTables);
    auto& staticContacts = std::get<ContactConstraintsToStaticObjectsTable>(db.mTables);

    addCollisionPairs(changedCollisionPairs.mGained.data(), changedCollisionPairs.mGained.size(), stableMappings, common, constraintMappings, tableIds, pairs);
    removeCollisionPairs(changedCollisionPairs.mLost.data(), changedCollisionPairs.mLost.size(), stableMappings, common, constraintMappings, tableIds);

    resolveObjectHandles(db, stableMappings, common, constraintMappings, tableIds, pairs);

    buildSyncIndices(common, constraintMappings);

    createConstraintTables(common, contacts, staticContacts, constraintMappings);

    fillConstraintNarrowphaseData(common, contacts, pairs, constraintMappings);
    fillConstraintNarrowphaseData(common, staticContacts, pairs, constraintMappings);

    changedCollisionPairs.mGained.clear();
    changedCollisionPairs.mLost.clear();
  }
};