#pragma once
#include "SlimRow.h"
#include "Table.h"

class IAppModule;

namespace Narrowphase {
  using CollisionMask = uint8_t;
}

namespace FragmentSpawner {
  enum class FragmentSpawnState : uint8_t {
    New,
    Spawned,
  };

  struct FragmentSpawnerTagRow : TagRow{};
  struct FragmentSpawnerConfig {
    Narrowphase::CollisionMask fragmentCollisionMask{};
    size_t fragmentCount{};
  };
  struct FragmentSpawnStateRow : SlimRow<FragmentSpawnState> {};
  struct FragmentSpawnerConfigRow : SlimRow<FragmentSpawnerConfig>{};

  std::unique_ptr<IAppModule> createModule();
 }