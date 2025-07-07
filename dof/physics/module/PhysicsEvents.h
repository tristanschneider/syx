#pragma once

#include <SparseRow.h>

class IAppModule;

namespace PhysicsEvents {
  struct RecomputeMassRow : SparseFlagRow{};

  //Register this after any modules that view events, which should all be after any can be emitted
  std::unique_ptr<IAppModule> clearEvents();
};