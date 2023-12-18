#pragma once

class IAppBuilder;
struct TaskRange;
struct GameDB;
struct PhysicsTableIds;
struct DBEvents;

namespace SweepNPruneBroadphase {
  struct BoundariesConfig;
}

namespace Narrowphase {
  struct UnitCubeDefinition;
}

namespace PhysicsSimulation {
  Narrowphase::UnitCubeDefinition getUnitCubeDefinition();

  void init(IAppBuilder& builder);
  void initFromConfig(IAppBuilder& builder);
  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  void updatePhysics(IAppBuilder& builder);
  //Before table service
  void preProcessEvents(IAppBuilder& builder);
  //After table service
  void postProcessEvents(IAppBuilder& builder);
};