#include <Precompile.h>
#include <PhysicsTableBuilder.h>

#include <RuntimeDatabase.h>
#include <ConstraintSolver.h>
#include <module/MassModule.h>
#include <module/PhysicsEvents.h>
#include <SweepNPruneBroadphase.h>
#include <Narrowphase.h>
#include <Constraints.h>
#include <shapes/Mesh.h>
#include <shapes/Circle.h>
#include <shapes/Rectangle.h>
#include <RelationModule.h>
#include <math/AxisFlags.h>
#include <Physics.h>

namespace PhysicsTableBuilder {
  StorageTableBuilder& addCircle(StorageTableBuilder& table) {
    return table.addRows<Shapes::CircleRow>();
  }

  StorageTableBuilder& addRectangle(StorageTableBuilder& table) {
    return table.addRows<Shapes::RectangleRow>();
  }

  StorageTableBuilder& addVelocity(StorageTableBuilder& table, math::AxisFlags axes) {
    if(axes.hasX()) {
      table.addRows<VelX>();
    }
    if(axes.hasY()) {
      table.addRows<VelY>();
    }
    if(axes.hasZ()) {
      table.addRows<VelZ>();
    }
    if(axes.hasA()) {
      table.addRows<VelA>();
    }
    return table;
  }

  StorageTableBuilder& addAcceleration(StorageTableBuilder& table, math::AxisFlags axes) {
    if(axes.hasX()) {
      table.addRows<AccelX>();
    }
    if(axes.hasY()) {
      table.addRows<AccelY>();
    }
    if(axes.hasZ()) {
      table.addRows<AccelZ>();
    }
    return table;
  }

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
    return addCollider(table, Narrowphase::CollisionMask{});
  }

  StorageTableBuilder& addColliderMaskAll(StorageTableBuilder& table) {
    return addCollider(table, static_cast<Narrowphase::CollisionMask>(~0));
  }

  StorageTableBuilder& addCollider(StorageTableBuilder& table, Narrowphase::CollisionMask mask) {
    auto [maskRow, keys] = table.addAndGetRows<
      Narrowphase::CollisionMaskRow,
      SweepNPruneBroadphase::BroadphaseKeys
    >();
    maskRow->setDefaultValue(mask);
    return table;
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

  StorageTableBuilder& addStaticTriangleMesh(StorageTableBuilder& table) {
    return addImmobile(table).addRows<
      Shapes::StaticTriangleMeshReferenceRow,
      Narrowphase::ThicknessRow,
      Relation::HasChildrenRow
    >();
  }

  StorageTableBuilder& addThickness(StorageTableBuilder& table, float defaultThickness) {
    auto [thickness] = table.addAndGetRows<Narrowphase::ThicknessRow>();
    thickness->setDefaultValue(defaultThickness);
    return table;
  }

  StorageTableBuilder& addSharedThickness(StorageTableBuilder& table, float defaultThickness) {
    auto [thickness] = table.addAndGetRows<Narrowphase::SharedThicknessRow>();
    thickness->setDefaultValue(defaultThickness);
    thickness->at() = defaultThickness;
    return table;
  }
}