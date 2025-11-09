#pragma once

class StorageTableBuilder;

namespace math {
  class AxisFlags;
}

namespace Narrowphase {
  using CollisionMask = uint8_t;
}

namespace PhysicsTableBuilder {
  //Shapes are just one row and fine to add directly. They are exposed here as well for easier discoverability
  StorageTableBuilder& addCircle(StorageTableBuilder& table);
  StorageTableBuilder& addRectangle(StorageTableBuilder& table);

  StorageTableBuilder& addVelocity(StorageTableBuilder& table, math::AxisFlags axes);
  StorageTableBuilder& addAcceleration(StorageTableBuilder& table, math::AxisFlags axes);
  StorageTableBuilder& addRigidbody(StorageTableBuilder& table);
  StorageTableBuilder& addMass(StorageTableBuilder& table);
  StorageTableBuilder& addImmobile(StorageTableBuilder& table);
  StorageTableBuilder& addCollider(StorageTableBuilder& table);
  StorageTableBuilder& addColliderMaskAll(StorageTableBuilder& table);
  StorageTableBuilder& addCollider(StorageTableBuilder& table, Narrowphase::CollisionMask mask);
  StorageTableBuilder& addRigidbodySharedMass(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedJoint(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedCustomJoint(StorageTableBuilder& table);
  StorageTableBuilder& addStaticTriangleMesh(StorageTableBuilder& table);
  StorageTableBuilder& addThickness(StorageTableBuilder& table, float defaultThickness = 0);
  StorageTableBuilder& addSharedThickness(StorageTableBuilder& table, float defaultThickness = 0);
}