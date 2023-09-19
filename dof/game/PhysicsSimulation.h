#pragma once

class IAppBuilder;
struct TaskRange;
struct GameDB;
struct PhysicsTableIds;
struct DBEvents;

namespace SweepNPruneBroadphase {
  struct BoundariesConfig;
}

namespace PhysicsSimulation {
  void init(GameDB db);
  void initFromConfig(GameDB db);
  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  void updatePhysics(IAppBuilder& builder);
  //Before table service
  TaskRange preProcessEvents(GameDB db);
  //After table service
  TaskRange postProcessEvents(GameDB db);

  PhysicsTableIds _getPhysicsTableIds();
};