#pragma once

#include "ecs/ECS.h"

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
struct ComputeInertiaRequestComponent {};

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
