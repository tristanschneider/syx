#pragma once
#include "Component.h"

class Transform : public Component {
public:
  Transform(Handle owner);
  Transform(const Transform& rhs);

  void set(const Syx::Mat4& m);
  const Syx::Mat4& get();

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(Transform);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  Syx::Mat4 mMat;
};