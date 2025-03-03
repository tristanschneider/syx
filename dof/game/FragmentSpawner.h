#pragma once
#include "SlimRow.h"
#include "Table.h"

class IAppModule;

namespace FragmentSpawner {
  enum class FragmentSpawnState : uint8_t {
    New,
    Spawned,
  };

  // If created with any of the following, they will be forwarded to the resulting fragments:
  // - SharedTextureRow
  // - SharedMeshRow
  // - Narrowphase::CollisionMaskRow
  struct FragmentSpawnerTagRow : TagRow{};
  struct FragmentSpawnerCount {
    size_t fragmentCount{};
  };
  struct FragmentSpawnStateRow : SlimRow<FragmentSpawnState> {};
  struct FragmentSpawnerCountRow : SlimRow<FragmentSpawnerCount>{
    static constexpr std::string_view KEY = "FragmentCount";
  };

  std::unique_ptr<IAppModule> createModule();
}