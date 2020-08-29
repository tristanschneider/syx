#include "Precompile.h"
#include "test/TestAppRegistration.h"

#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/LuaComponentRegistry.h"
#include "component/NameComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "component/SpaceComponent.h"
#include "component/Transform.h"

#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"

void LuaRegistration::registerSystems(const SystemArgs& args, ISystemRegistry& registry) {
  registry.registerSystem(std::make_unique<AssetRepo>(args, Registry::createAssetLoaderRegistry()));
  registry.registerSystem(std::make_unique<LuaGameSystem>(args));
}

void LuaRegistration::registerComponents(IComponentRegistry& registry) {
  registry.registerComponent<CameraComponent>();
  registry.registerComponent<LuaComponent>();
  registry.registerComponent<NameComponent>();
  registry.registerComponent<Physics>();
  registry.registerComponent<Renderable>();
  registry.registerComponent<SpaceComponent>();
  registry.registerComponent<Transform>();
}