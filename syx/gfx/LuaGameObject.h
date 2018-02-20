#pragma once
#include "component/Component.h"
#include "component/Transform.h"

class LuaGameObject {
public:
  LuaGameObject(Handle h);

  Handle getHandle() const;

  void addComponent(std::unique_ptr<Component> component);
  void removeComponent(size_t type);
  Component* getComponent(size_t type);
  Transform& getTransform();

  template<typename CompType>
  CompType* getComponent() {
    std::unique_ptr<Component>* result = mComponents.get<CompType>();
    return result ? static_cast<CompType*>(result->get()) : nullptr;
  }

  template<typename CompType>
  void removeComponent() {
    mComponents.set<CompType>(nullptr);
  }

private:
  Handle mHandle;
  Transform mTransform;
  TypeMap<std::unique_ptr<Component>, Component> mComponents;
};