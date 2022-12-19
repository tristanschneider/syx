#include "Precompile.h"
#include "CameraComponent.h"

#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "Util.h"

OldCameraComponent::OldCameraComponent(const OldCameraComponent& rhs)
  : TypedComponent(rhs)
  , mViewportName(rhs.mViewportName) {
}

std::unique_ptr<Component> OldCameraComponent::clone() const {
  return std::make_unique<OldCameraComponent>(*this);
}

void OldCameraComponent::set(const Component& component) {
  assert(getType() == component.getType() && "set type should match");
  const OldCameraComponent& rhs = static_cast<const OldCameraComponent&>(component);
  mViewportName = rhs.mViewportName;
}

const Lua::Node* OldCameraComponent::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = [this]() {
    using namespace Lua;
    std::unique_ptr<Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
    makeNode<StringNode>(NodeOps(*root, "viewport", ::Util::offsetOf(*this, mViewportName)));
    return root;
  }();
  return props.get();
}

void OldCameraComponent::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& OldCameraComponent::getTypeInfo() const {
  static ComponentTypeInfo result("Camera");
  return result;
}

void OldCameraComponent::setViewport(const std::string& viewport) {
  mViewportName = viewport;
}

const std::string& OldCameraComponent::getViewport() const {
  return mViewportName;
}
