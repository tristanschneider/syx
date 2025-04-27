#pragma once

class IAppModule;

namespace RespawnArea {
  std::unique_ptr<IAppModule> createModule();
}