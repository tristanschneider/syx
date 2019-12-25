#include "Precompile.h"
#include "component/NameComponent.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>

DEFINE_COMPONENT(NameComponent)
  , mName("object") {
}

NameComponent::NameComponent(const NameComponent& rhs)
  : Component(rhs.getType(), rhs.getOwner())
  , mName(rhs.mName) {
}

std::string_view NameComponent::getName() const {
  return mName;
}

void NameComponent::setName(std::string_view name) {
  mName = name;
}

std::unique_ptr<Component> NameComponent::clone() const {
  return std::make_unique<NameComponent>(*this);
}

void NameComponent::set(const Component& component) {
  assert(getType() == component.getType() && "set type should match");
  mName = static_cast<const NameComponent&>(component).mName;
}

const Lua::Node* NameComponent::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = _buildLuaProps();
  return props.get();
}

void NameComponent::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& NameComponent::getTypeInfo() const {
  static ComponentTypeInfo result("Name");
  return result;
}

std::unique_ptr<Lua::Node> NameComponent::_buildLuaProps() const {
  using namespace Lua;
  std::unique_ptr<Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<StringNode>(NodeOps(*root, "name", ::Util::offsetOf(*this, mName)));
  return root;
}
