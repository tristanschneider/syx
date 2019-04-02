#include "Precompile.h"
#include "Transform.h"

#include "DebugDrawer.h"
#include "editor/InspectorFactory.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "LuaGameObject.h"

DEFINE_COMPONENT(Transform)
  , mMat(Syx::Mat4::transform(Syx::Quat::Identity, Syx::Vec3::Zero)) {
}

Transform::Transform(const Transform& rhs)
  : Component(rhs.getType(), rhs.getOwner())
  , mMat(rhs.mMat) {
}

void Transform::set(const Syx::Mat4& m) {
  mMat = m;
}

const Syx::Mat4& Transform::get() const {
  return mMat;
}

std::unique_ptr<Component> Transform::clone() const {
  return std::make_unique<Transform>(*this);
}

void Transform::set(const Component& component) {
  assert(getType() == component.getType() && "set type should match");
  mMat = static_cast<const Transform&>(component).mMat;
}

const Lua::Node* Transform::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = [this]() {
    using namespace Lua;
    std::unique_ptr<Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
    makeNode<Mat4Node>(NodeOps(*root, "matrix", ::Util::offsetOf(*this, mMat))).setInspector(Inspector::wrap(Inspector::inspectTransform));
    return root;
  }();
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
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& Transform::getTypeInfo() const {
  static ComponentTypeInfo result("Transform");
  return result;
}

void Transform::onEditorUpdate(const LuaGameObject& self, bool selected, EditorUpdateArgs& args) const {
  if(selected) {
    using namespace Syx;
    const Vec3 pos = mMat.getCol(3);

    const Vec3 colors[] = { Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(0, 0.75f, 1) };
    //Arbitrary scale to make it look nice
    const float scalar = 3.0f;
    for(int i = 0; i < 3; ++i) {
      args.drawer.setColor(colors[i]);
      args.drawer.drawVector(pos, mMat.getCol(i)*scalar);
    }
  }
}