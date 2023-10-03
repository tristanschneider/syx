#pragma once

class IAppBuilder;
struct GameDB;
struct TaskRange;

namespace World {
  void enforceWorldBoundary(IAppBuilder& builder);
}