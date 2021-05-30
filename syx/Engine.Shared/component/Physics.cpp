#include "Precompile.h"
#include "component/Physics.h"

#include "editor/InspectorFactory.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include "lua/lib/LuaVec3.h"
#include <lua.hpp>

#include "provider/MessageQueueProvider.h"

namespace {
  IComponent& _checkSelf(lua_State* l) {
    return Component::_checkSelf(l, Physics::singleton().getTypeInfo().mTypeName);
  }

  //TODO: this has the odd side-effect of not reflecting in the velocity properties until later frames. Is that weird?
  int _applyForce(lua_State* l) {
    IComponent& self = _checkSelf(l);
    Lua::checkGameContext(l).getMessageProvider().getMessageQueue()->push(ApplyForceEvent(
      self.get().getOwner(),
      ApplyForceEvent::Force{ Lua::Vec3::_getVec(l, 2), Lua::Vec3::_getVec(l, 3) },
      ApplyForceEvent::Mode::Force)
    );
    return 0;
  }

  int _applyForceAtPoint(lua_State* l) {
    IComponent& self = _checkSelf(l);
    Lua::checkGameContext(l).getMessageProvider().getMessageQueue()->push(ApplyForceEvent(
      self.get().getOwner(),
      ApplyForceEvent::ForceAtPoint{ Lua::Vec3::_getVec(l, 2), Lua::Vec3::_getVec(l, 3) },
      ApplyForceEvent::Mode::Force)
    );
    return 0;
  }
}

PhysicsData::PhysicsData()
  : mHasRigidbody(false)
  , mHasCollider(false)
  , mModel(InvalidHandle)
  , mMaterial(InvalidHandle)
  , mPhysToModel(Syx::Mat4::identity())
  , mLinVel(Syx::Vec3::Zero)
  , mAngVel(Syx::Vec3::Zero) {
}

Physics::Physics(const Physics& other)
  : TypedComponent(other)
  , mData(other.mData) {
}

std::unique_ptr<Component> Physics::clone() const {
  return std::make_unique<Physics>(*this);
}

void Physics::set(const Component& component) {
  assert(getType() == component.getType() && "set type must match");
  mData = static_cast<const Physics&>(component).mData;
}

void Physics::setData(const PhysicsData& data) {
  mData = data;
}

void Physics::setCollider(Handle model, Handle material) {
  mData.mHasCollider = true;
  mData.mModel = model;
  mData.mMaterial = material;
}

void Physics::setRigidbody(const Syx::Vec3& linVel, const Syx::Vec3& angVel) {
  mData.mHasRigidbody = true;
  mData.mLinVel = linVel;
  mData.mAngVel = angVel;
}

void Physics::setPhysToModel(const Syx::Mat4& physToModel) {
  mData.mPhysToModel = physToModel;
}

void Physics::setLinVel(const Syx::Vec3& linVel) {
  mData.mLinVel = linVel;
}

void Physics::setAngVel(const Syx::Vec3& angVel) {
  mData.mAngVel = angVel;
}

const Lua::Node* Physics::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = _buildLuaProps();
  return props.get();
}

void Physics::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { "applyForce", _applyForce },
    { "applyForceAtPoint", _applyForceAtPoint },
    { nullptr, nullptr },
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& Physics::getTypeInfo() const {
  static ComponentTypeInfo result("Physics");
  return result;
}

std::unique_ptr<Lua::Node> Physics::_buildLuaProps() const {
  using namespace Lua;
  std::unique_ptr<Lua::Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<BoolNode>(NodeOps(*root, "hasRigidbody", ::Util::offsetOf(*this, mData.mHasRigidbody)));
  makeNode<BoolNode>(NodeOps(*root, "hasCollider", ::Util::offsetOf(*this, mData.mHasCollider)));
  makeNode<Vec3Node>(NodeOps(*root, "linVel", ::Util::offsetOf(*this, mData.mLinVel)));
  makeNode<Vec3Node>(NodeOps(*root, "angVel", ::Util::offsetOf(*this, mData.mAngVel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "model", ::Util::offsetOf(*this, mData.mModel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "material", ::Util::offsetOf(*this, mData.mMaterial)));
  makeNode<Mat4Node>(NodeOps(*root, "physToModel", ::Util::offsetOf(*this, mData.mPhysToModel))).setInspector(Inspector::wrap(Inspector::inspectTransform));
  makeNode<BoolNode>(NodeOps(*root, "lockXRotation", ::Util::offsetOf(*this, mData.mLockXRotation)));
  makeNode<BoolNode>(NodeOps(*root, "lockYRotation", ::Util::offsetOf(*this, mData.mLockYRotation)));
  makeNode<BoolNode>(NodeOps(*root, "lockZRotation", ::Util::offsetOf(*this, mData.mLockZRotation)));
  return root;
}
