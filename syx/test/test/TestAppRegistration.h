#include "Precompile.h"
#include "AppRegistration.h"

struct LuaRegistration : public AppRegistration {
  static size_t TEST_CALLBACK_ID;

  virtual ~LuaRegistration() = default;
  virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override;
  virtual void registerComponents(IComponentRegistry& registry) override;
};