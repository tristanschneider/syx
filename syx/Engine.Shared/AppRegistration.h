#pragma once

struct IComponentRegistry;
class ISystemRegistry;
class IAssetLoaderRegistry;
struct SystemArgs;

class AppRegistration {
public:
  virtual ~AppRegistration() = default;
  virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) = 0;
  virtual void registerComponents(IComponentRegistry& registry) = 0;
};

namespace Registration {
  std::unique_ptr<AppRegistration> createDefaultApp();
};