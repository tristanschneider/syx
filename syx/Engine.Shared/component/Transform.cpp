#include "Precompile.h"
#include "Transform.h"

#include "component/ComponentPublisher.h"
#include "DebugDrawer.h"
#include "editor/InspectorFactory.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "LuaGameObject.h"

#include "lua/lib/LuaVec3.h"
#include "lua/lib/LuaQuat.h"
#include "Util.h"

namespace {
  IComponent& _checkSelfTransform(lua_State* l) {
    static const Transform typeInfo(0);
    return Component::_checkSelf(l,  typeInfo.getTypeInfo().mTypeName);
  }

  void _setTransform(const Syx::Mat4& mat, IComponent& component) {
    Transform result(component.get().getOwner());
    result.set(mat);
    component.set(result);
  }

  Syx::Mat4 _getMatrix(IComponent& component) {
    return component.get<Transform>().get();
  }

  int _getTranslate(lua_State* l) {
    return Lua::Vec3::construct(l, _checkSelfTransform(l).get<Transform>().get().getTranslate());
  }

  int _setTranslate(lua_State* l) {
    IComponent& self = _checkSelfTransform(l);
    Syx::Mat4 mat = _getMatrix(self);
    mat.setTranslate(Lua::Vec3::_getVec(l, 2));
    _setTransform(mat, self);
    return 0;
  }

  int _getRotate(lua_State* l) {
    return Lua::Quat::construct(l, _checkSelfTransform(l).get<Transform>().get().getRotQ());
  }

  int _setRotate(lua_State* l) {
    IComponent& self(_checkSelfTransform(l));
    Syx::Mat4 mat = _getMatrix(self);
    mat.setRot(Lua::Quat::_getQuat(l, 2));
    _setTransform(mat, self);
    return 0;
  }

  int _getScale(lua_State* l) {
    return Lua::Vec3::construct(l, _checkSelfTransform(l).get<Transform>().get().getScale());
  }

  int _setScale(lua_State* l) {
    IComponent& self(_checkSelfTransform(l));
    Syx::Mat4 mat = _getMatrix(self);
    mat.setScale(Lua::Vec3::_getVec(l, 2));
    _setTransform(mat, self);
    return 0;
  }

  int _getComponents(lua_State* l) {
    Syx::Vec3 translate, scale;
    Syx::Mat3 rotate;
    _checkSelfTransform(l).get<Transform>().get().decompose(scale, rotate, translate);
    Lua::Vec3::construct(l, translate);
    Lua::Quat::construct(l, rotate.toQuat());
    Lua::Vec3::construct(l, scale);
    return 3;
  }

  int _setComponents(lua_State* l) {
    _setTransform(Syx::Mat4::transform(Lua::Vec3::_getVec(l, 3), Lua::Quat::_getQuat(l, 2), Lua::Vec3::_getVec(l, 1)), _checkSelfTransform(l));
    return 0;
  }

  int _modelToWorld(lua_State* l) {
    return Lua::Vec3::construct(l, _checkSelfTransform(l).get<Transform>().get() * Lua::Vec3::_getVec(l, 2));
  }

  int _worldToModel(lua_State* l) {
    return Lua::Vec3::construct(l, _checkSelfTransform(l).get<Transform>().get().affineInverse() * Lua::Vec3::_getVec(l, 2));
  }
}

Transform::Transform(Handle h)
  : TypedComponent(h)
  , mMat(Syx::Mat4::transform(Syx::Quat::Identity, Syx::Vec3::Zero)) {
}

Transform::Transform(const Transform& rhs)
  : TypedComponent(rhs)
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
    { "getTranslate", _getTranslate },
    { "setTranslate", _setTranslate },
    { "getRotate", _getRotate },
    { "setRotate", _setRotate },
    { "getScale", _getScale },
    { "setScale", _setScale },
    { "getComponents", _getComponents },
    { "setComponents", _setComponents },
    { "modelToWorld", _modelToWorld },
    { "worldToModel", _worldToModel },
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& Transform::getTypeInfo() const {
  static ComponentTypeInfo result("Transform");
  return result;
}

void Transform::onEditorUpdate(const LuaGameObject&, bool selected, EditorUpdateArgs& args) const {
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