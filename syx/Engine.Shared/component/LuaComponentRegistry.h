#pragma once
#include <optional>

class Component;
class ComponentRegistryProvider;
struct ComponentType;
struct ComponentTypeInfo;

struct IComponentRegistry {
  using Constructor = std::function<std::unique_ptr<Component>(Handle)>;

  virtual ~IComponentRegistry() = default;

  virtual void registerComponent(const std::string& name, Constructor constructor) = 0;
  virtual std::unique_ptr<Component> construct(const std::string& name, Handle owner) const = 0;
  virtual std::unique_ptr<Component> construct(const ComponentType& type, Handle owner) const = 0;

  virtual std::optional<size_t> getComponentType(const std::string& name) const = 0;
  virtual std::optional<ComponentType> getComponentFullType(const std::string& name) const = 0;
  virtual const Component* getInstanceByPropName(const char* name) const = 0;
  virtual const Component* getInstanceByPropNameConstHash(size_t hash) const = 0;
  virtual void forEachComponent(const std::function<void(const Component&)>& callback) const = 0;

  template<class T>
  void registerComponent() {
    auto ctor = [](Handle h) { return std::make_unique<T>(h); };
    std::string name = ctor(0)->getTypeInfo().mTypeName;
    registerComponent(std::move(name), std::move(ctor));
  }
};

namespace Registry {
  std::unique_ptr<IComponentRegistry> createComponentRegistry();
  std::unique_ptr<ComponentRegistryProvider> createComponentRegistryProvider();
}