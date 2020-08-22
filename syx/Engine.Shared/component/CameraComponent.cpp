#include "Precompile.h"
#include "CameraComponent.h"

#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "Util.h"

CameraComponent::CameraComponent(const CameraComponent& rhs)
  : TypedComponent(rhs)
  , mViewportName(rhs.mViewportName) {
}

std::unique_ptr<Component> CameraComponent::clone() const {
  return std::make_unique<CameraComponent>(*this);
}

void CameraComponent::set(const Component& component) {
  assert(getType() == component.getType() && "set type should match");
  const CameraComponent& rhs = static_cast<const CameraComponent&>(component);
  mViewportName = rhs.mViewportName;
}

const Lua::Node* CameraComponent::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = [this]() {
    using namespace Lua;
    std::unique_ptr<Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
    makeNode<StringNode>(NodeOps(*root, "viewport", ::Util::offsetOf(*this, mViewportName)));
    return root;
  }();
  return props.get();
}

void CameraComponent::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& CameraComponent::getTypeInfo() const {
  static ComponentTypeInfo result("Camera");
  return result;
}

void CameraComponent::setViewport(const std::string& viewport) {
  mViewportName = viewport;
}

const std::string& CameraComponent::getViewport() const {
  return mViewportName;
}
