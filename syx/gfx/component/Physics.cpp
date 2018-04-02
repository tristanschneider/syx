#include "Precompile.h"
#include "component/Physics.h"
#include "lua/LuaNode.h"

DEFINE_EVENT(PhysicsCompUpdateEvent, const PhysicsData& data, Handle owner)
  , mData(data)
  , mOwner(owner) {
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

DEFINE_COMPONENT(Physics) {
}

void Physics::setData(const PhysicsData& data, bool fireEvent) {
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

std::unique_ptr<Lua::Node> Physics::_buildLuaProps() const {
  using namespace Lua;
  std::unique_ptr<Lua::Node> root = makeRootNode(NodeOps(LUA_PROPS_KEY));
  makeNode<BoolNode>(NodeOps(*root, "hasRigidbody", Util::offsetOf(*this, mData.mHasRigidbody)));
  makeNode<BoolNode>(NodeOps(*root, "hasCollider", Util::offsetOf(*this, mData.mHasCollider)));
  makeNode<Vec3Node>(NodeOps(*root, "linVel", Util::offsetOf(*this, mData.mLinVel)));
  makeNode<Vec3Node>(NodeOps(*root, "angVel", Util::offsetOf(*this, mData.mAngVel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "model", Util::offsetOf(*this, mData.mModel)));
  makeNode<LightUserdataSizetNode>(NodeOps(*root, "material", Util::offsetOf(*this, mData.mMaterial)));
  makeNode<Mat4Node>(NodeOps(*root, "physToModel", Util::offsetOf(*this, mData.mPhysToModel)));
  return root;
}
