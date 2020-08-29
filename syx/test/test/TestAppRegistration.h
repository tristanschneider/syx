#include "Precompile.h"
#include "AppRegistration.h"

struct LuaRegistration : public AppRegistration {
  virtual ~LuaRegistration() = default;
  virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override;
  virtual void registerComponents(IComponentRegistry& registry) override;
};