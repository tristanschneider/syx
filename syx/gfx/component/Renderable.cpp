#include "Precompile.h"
#include "Renderable.h"
#include "lua/LuaNode.h"

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

std::unique_ptr<Lua::Node> Renderable::_buildLuaProps() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "model", Util::offsetOf(*this, mData.mModel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "diffuseTexture", Util::offsetOf(*this, mData.mDiffTex)));
  return root;
}
