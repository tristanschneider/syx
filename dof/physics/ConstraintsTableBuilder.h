#pragma once

#include "Physics.h"
#include "PhysicsTableIds.h"
#include "Profile.h"
#include "SweepNPruneBroadphase.h"
#include "Scheduler.h"

class IAppBuilder;

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

namespace ConstraintsTableBuilder {
  struct AddDeps {
    static AddDeps query(RuntimeDatabaseTaskBuilder& task);

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
    AddDeps& deps);
  void addPaddingToTable(size_t targetTable,
    size_t amount,
    const PhysicsTableIds& tableIds,
    AddDeps& deps);

  //Do all of the above to end up with constraints ready to solve except for filling in velocity
  void build(IAppBuilder& builder, const Config::PhysicsConfig& config);
};

namespace ctbdetails {
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
    size_t targetWidth);
  std::pair<size_t, size_t> getTargetElementRange(size_t targetTable, const PhysicsTableIds& tables, const ConstraintsTableMappings& mappings, size_t totalElements);
}
