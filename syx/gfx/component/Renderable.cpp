#include "Precompile.h"
#include "Renderable.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>

const std::string Renderable::CLASS_NAME = "Renderable";
const std::pair<std::string, size_t> Renderable::NAME_HASH = Util::getHashPair("renderable", Util::constHash);

DEFINE_EVENT(RenderableUpdateEvent, const RenderableData& data, Handle obj)
  , mObj(obj)
  , mData(data) {
}

DEFINE_COMPONENT(Renderable) {
  mData.mModel = mData.mDiffTex = InvalidHandle;
}

const RenderableData& Renderable::get() const {
  return mData;
}

void Renderable::set(const RenderableData& data) {
  mData = data;
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
  Lua::Util::registerClass(l, statics, members, CLASS_NAME.c_str(), true);
}

const std::string& Renderable::getName() const {
  return NAME_HASH.first;
}

const std::string& Renderable::getTypeName() const {
  return CLASS_NAME;
}

size_t Renderable::getNameConstHash() const {
  return NAME_HASH.second;
}

std::unique_ptr<Lua::Node> Renderable::_buildLuaProps() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "model", ::Util::offsetOf(*this, mData.mModel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "diffuseTexture", ::Util::offsetOf(*this, mData.mDiffTex)));
  return root;
}
