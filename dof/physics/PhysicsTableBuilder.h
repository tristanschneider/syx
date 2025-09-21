#pragma once

class StorageTableBuilder;

namespace math {
  class AxisFlags;
}

namespace PhysicsTableBuilder {
  StorageTableBuilder& addVelocity(StorageTableBuilder& table, math::AxisFlags axes);
  StorageTableBuilder& addAcceleration(StorageTableBuilder& table, math::AxisFlags axes);
  StorageTableBuilder& addRigidbody(StorageTableBuilder& table);
  StorageTableBuilder& addMass(StorageTableBuilder& table);
  StorageTableBuilder& addImmobile(StorageTableBuilder& table);
  StorageTableBuilder& addCollider(StorageTableBuilder& table);
  StorageTableBuilder& addRigidbodySharedMass(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedJoint(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedCustomJoint(StorageTableBuilder& table);
  StorageTableBuilder& addStaticTriangleMesh(StorageTableBuilder& table);
}