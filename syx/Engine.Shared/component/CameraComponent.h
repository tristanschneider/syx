#pragma once
#include "Component.h"

class OldCameraComponent : public TypedComponent<OldCameraComponent> {
public:
  using TypedComponent::TypedComponent;
  OldCameraComponent(const OldCameraComponent& rhs);
  virtual ~OldCameraComponent() = default;

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(OldCameraComponent);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

  void setViewport(const std::string& viewport);
  const std::string& getViewport() const;

private:
  std::string mViewportName;
};