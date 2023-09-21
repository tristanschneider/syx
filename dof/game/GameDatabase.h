#pragma once

struct IDatabase;
struct StableElementMappings;

//TODO: match name to file when GameDatabase no longer exists
namespace GameData {
  std::unique_ptr<IDatabase> create(StableElementMappings& mappings);
}