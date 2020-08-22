#pragma once
#include "Component.h"

class NameComponent : public TypedComponent<NameComponent> {
public:
  NameComponent(Handle owner);
  NameComponent(const NameComponent& rhs);

  std::string_view getName() const;
  void setName(std::string_view name);

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(NameComponent);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  std::string mName;
};