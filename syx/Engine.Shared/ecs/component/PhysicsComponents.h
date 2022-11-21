#pragma once

#include "ecs/ECS.h"

//All request components go on the physics entity unless otherwise noted

//These requests go on the owner entity
//Put this on an entity an the physics system will create a PhysicsObject and link the current
//entity to it with a PhysicsOwnerComponent
struct CreatePhysicsObjectRequestComponent {};
//Same idea as create but put on the owner to sever the link and destroy the physics object
struct DestroyPhysicsObjectRequestComponent {};

//On an entity that has a corresponding physics object
//The physics system operates primarily on the physics object entity but extracts
//relevant information from the owner and applies back to it
struct PhysicsOwner {
  Engine::Entity mPhysicsObject;
};

//On entities that the physics system simulates on, this links back to the owner
//for extracting and applying relevant information
struct PhysicsObject {
  Engine::Entity mPhysicsOwner;
};

//Compute the local mass (mass and inertia) for the object
struct ComputeMassRequestComponent {};
//Recompute the inertia tensor based on the local inertia
struct ComputeInertiaRequestComponent {};

//Request that the transform of the owner entity is written to the physics entity
struct SyncTransformRequestComponent {};
//Request that the model of the owner entity is written to the physics entity
struct SyncModelRequestComponent {};
//Request that the ViewedVelocityComponent of the owner entity is written to the physics entity
struct SyncVelocityRequestComponent {};

struct ViewedVelocityComponent {
  float mLinearX = 0.0f;
  float mLinearY = 0.0f;
  float mLinearZ = 0.0f;
  float mAngularX = 0.0f;
  float mAngularY = 0.0f;
  float mAngularZ = 0.0f;
};

template<class Tag, class Element>
struct FloatComponent {
  float mValue = 0.0f;
};

namespace Tags {
  struct Position{};
  struct Rotation{};
  struct LinearVelocity{};
  struct AngularVelocity{};
  struct Inertia{};
  struct LocalInertia{};
  struct Mass{};

  //Vec3
  struct X{};
  struct Y{};
  struct Z{};
  //Quaternion
  struct I{};
  struct J{};
  struct K{};
  struct W{};
  //Symmetrix matrix
  struct A{};
  struct B{};
  struct C{};
  struct D{};
  struct E{};
  struct F{};
  struct Value{};
};
