#include "Precompile.h"
#include "GameTime.h"

#include "config/Config.h"

#include "AppBuilder.h"

namespace GameTime {
  const float* getDeltaTime(RuntimeDatabaseTaskBuilder& task) {
    const auto* res = task.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    return res ? &res->world.deltaTime : nullptr;
  }
}