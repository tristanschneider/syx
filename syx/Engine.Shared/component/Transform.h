#pragma once
#include "Component.h"
#include "SyxMat4.h"

namespace Syx {
  struct Vec3;
  struct Quat;
};

class Transform : public Component {
public:
  Transform(Handle owner);
  Transform(const Transform& rhs);

  void set(const Syx::Mat4& m);
  const Syx::Mat4& get() const;

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(Transform);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

  virtual void onEditorUpdate(const LuaGameObject& self, bool selected, EditorUpdateArgs& args) const override;

private:
  Syx::Mat4 mMat;
};