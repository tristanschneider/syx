#include "Precompile.h"
#include "Renderable.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>

DEFINE_EVENT(RenderableUpdateEvent, const RenderableData& data, Handle obj)
  , mObj(obj)
  , mData(data) {
}

DEFINE_COMPONENT(Renderable) {
  mData.mModel = mData.mDiffTex = InvalidHandle;
}

Renderable::Renderable(const Renderable& other)
  : Component(other.getType(), other.getOwner())
  , mData(other.mData) {
}

const RenderableData& Renderable::get() const {
  return mData;
}

void Renderable::set(const RenderableData& data) {
  mData = data;
}

std::unique_ptr<Component> Renderable::clone() const {
  return std::make_unique<Renderable>(*this);
}

void Renderable::set(const Component& component) {
  assert(getType() == component.getType() && "Set component type must match");
  mData = static_cast<const Renderable&>(component).mData;
}

const Lua::Node* Renderable::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = std::move(_buildLuaProps());
  return props.get();
}

void Renderable::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    nullptr, nullptr
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& Renderable::getTypeInfo() const {
  static ComponentTypeInfo result("Renderable");
  return result;
}

std::unique_ptr<Lua::Node> Renderable::_buildLuaProps() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "model", ::Util::offsetOf(*this, mData.mModel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "diffuseTexture", ::Util::offsetOf(*this, mData.mDiffTex)));
  return root;
}
