#pragma once

struct PhysicsConfig;
struct TaskRange;
struct GameDB;
struct PhysicsTableIds;

namespace SweepNPruneBroadphase {
  struct BoundariesConfig;
}

namespace PhysicsSimulation {
  void init(GameDB db);
  //Insert all objects into the broadphase that can be. Doesn't check if they already are, meant for initial insert
  void initialPopulateBroadphase(GameDB db);
  TaskRange updatePhysics(GameDB db);

  //TODO: don't expose this, updating the broadphase should be within PhysicsSimulation
  SweepNPruneBroadphase::BoundariesConfig _getStaticBoundariesConfig();
  PhysicsTableIds _getPhysicsTableIds();
};