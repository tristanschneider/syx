#pragma once
#include "Component.h"

class Transform : public Component {
public:
  Transform(Handle owner);

  void set(const Syx::Mat4& m);
  const Syx::Mat4& get();

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(Transform);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  Syx::Mat4 mMat;
};