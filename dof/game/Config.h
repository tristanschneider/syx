#pragma once

#include "curve/CurveDefinition.h"
#include "config/Config.h"
#include <memory>
#include "Table.h"

//Plain data config is in config library, any types that contain dependencies on other projects are converted from plain data here


namespace Ability {
  struct AbilityInput;
}

namespace Config {
  CurveDefinition& getCurve(CurveConfigExt& ext);
  const CurveDefinition& getCurve(const CurveConfigExt& ext);
  Ability::AbilityInput& getAbility(AbilityConfigExt& ext);

  std::unique_ptr<Config::IFactory> createFactory();
}
