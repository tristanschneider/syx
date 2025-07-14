#include <Precompile.h>
#include <PhysicsTableBuilder.h>

#include <RuntimeDatabase.h>
#include <ConstraintSolver.h>
#include <module/MassModule.h>
#include <module/PhysicsEvents.h>
#include <SweepNPruneBroadphase.h>
#include <Narrowphase.h>
#include <Constraints.h>

namespace PhysicsTableBuilder {
  StorageTableBuilder& addRigidbody(StorageTableBuilder& table) {
    return addMass(table).addRows<
      ConstraintSolver::ConstraintMaskRow,
      ConstraintSolver::SharedMaterialRow
    >();
  }

  StorageTableBuilder& addMass(StorageTableBuilder& table) {
    return table.addRows<
      MassModule::MassRow,
      PhysicsEvents::RecomputeMassRow
    >();
  }

  StorageTableBuilder& addImmobile(StorageTableBuilder& table) {
    return table.addRows<MassModule::IsImmobile>();
  }

  StorageTableBuilder& addCollider(StorageTableBuilder& table) {
    return table.addRows<
      SweepNPruneBroadphase::BroadphaseKeys,
      Narrowphase::CollisionMaskRow
    >();
  }

  StorageTableBuilder& addRigidbodySharedMass(StorageTableBuilder& table) {
    return addMass(table).addRows<
      ConstraintSolver::ConstraintMaskRow,
      ConstraintSolver::SharedMaterialRow
    >();
  }

  StorageTableBuilder& addAutoManagedJoint(StorageTableBuilder& table) {
    return table.addRows<
      Constraints::AutoManageJointTag,
      Constraints::TableConstraintDefinitionsRow,
      Constraints::ConstraintChangesRow,
      Constraints::JointRow,
      Constraints::ConstraintStorageRow
    >();
  }

  StorageTableBuilder& addAutoManagedCustomJoint(StorageTableBuilder& table) {
    addAutoManagedJoint(table);
    return table.addRows<Constraints::CustomConstraintRow>();
  }
}