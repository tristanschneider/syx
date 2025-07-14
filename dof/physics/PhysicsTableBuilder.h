#pragma once

class StorageTableBuilder;

namespace PhysicsTableBuilder {
  StorageTableBuilder& addRigidbody(StorageTableBuilder& table);
  StorageTableBuilder& addMass(StorageTableBuilder& table);
  StorageTableBuilder& addImmobile(StorageTableBuilder& table);
  StorageTableBuilder& addCollider(StorageTableBuilder& table);
  StorageTableBuilder& addRigidbodySharedMass(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedJoint(StorageTableBuilder& table);
  StorageTableBuilder& addAutoManagedCustomJoint(StorageTableBuilder& table);
}