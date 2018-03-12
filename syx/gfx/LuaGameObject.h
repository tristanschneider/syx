#pragma once
#include "component/Transform.h"

class Component;
class LuaComponent;

class LuaGameObject {
public:
  LuaGameObject(Handle h);
  ~LuaGameObject();

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

  LuaComponent* addLuaComponent(size_t script);
  LuaComponent* getLuaComponent(size_t script);
  void removeLuaComponent(size_t script);
  std::unordered_map<size_t, LuaComponent>& getLuaComponents();

private:
  Handle mHandle;
  Transform mTransform;
  TypeMap<std::unique_ptr<Component>, Component> mComponents;
  //Id of script in asset repo to the lua component holding it
  std::unordered_map<size_t, LuaComponent> mLuaComponents;
};