#include "Precompile.h"
#include "Transform.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>

DEFINE_COMPONENT(Transform)
  , mMat(Syx::Mat4::transform(Syx::Quat::Identity, Syx::Vec3::Zero)) {
}

void Transform::set(const Syx::Mat4& m) {
  mMat = m;
}

const Syx::Mat4& Transform::get() {
  return mMat;
}

const Lua::Node* Transform::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = _buildLuaProps();
  return props.get();
}

void Transform::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str(), true);
}

const ComponentTypeInfo& Transform::getTypeInfo() const {
  static ComponentTypeInfo result("Transform");
  return result;
}

std::unique_ptr<Lua::Node> Transform::_buildLuaProps() const {
  using namespace Lua;
  std::unique_ptr<Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<Mat4Node>(NodeOps(*root, "matrix", ::Util::offsetOf(*this, mMat)));
  return root;
}
