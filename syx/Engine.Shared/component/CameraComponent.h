#pragma once
#include "Component.h"

class CameraComponent : public TypedComponent<CameraComponent> {
public:
  using TypedComponent::TypedComponent;
  CameraComponent(const CameraComponent& rhs);
  virtual ~CameraComponent() = default;

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(CameraComponent);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

  void setViewport(const std::string& viewport);
  const std::string& getViewport() const;

private:
  std::string mViewportName;
};