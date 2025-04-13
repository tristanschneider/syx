#pragma once

#include "Table.h"

class IAppModule;

namespace RespawnArea {
  struct RespawnBurstRadius : Row<float> {
    static constexpr std::string_view KEY = "RespawnBurstRadius";
  };

  std::unique_ptr<IAppModule> createModule();
}