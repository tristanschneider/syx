#include "Precompile.h"

#include "ecs/EngineAppContext.h"
#include "AppRegistration.h"
#include "util/TypeId.h"

class System;

struct LuaRegistration : public AppRegistration {
  static typeId_t<System> TEST_CALLBACK_ID;

  virtual ~LuaRegistration() = default;
  virtual void registerAppContext(Engine::AppContext& context) override {
    Registration::createDefaultApp()->registerAppContext(context);
  }


  virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override;
  virtual void registerComponents(IComponentRegistry& registry) override;
};

namespace TestRegistration {
  std::unique_ptr<AppRegistration> createEditorRegistration();
  std::unique_ptr<AppRegistration> createPhysicsRegistration();
}