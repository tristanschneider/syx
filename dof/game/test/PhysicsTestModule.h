#pragma once

class IAppModule;

namespace PhysicsTestModule {
  std::unique_ptr<IAppModule> create();
}