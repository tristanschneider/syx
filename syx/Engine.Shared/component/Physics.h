#pragma once
#include "Component.h"
#include "event/Event.h"

struct PhysicsData {
  PhysicsData();

  bool mHasRigidbody;
  bool mHasCollider;
  Syx::Vec3 mLinVel;
  Syx::Vec3 mAngVel;
  Handle mModel;
  Handle mMaterial;
  //Transform from physics space to model space
  Syx::Mat4 mPhysToModel;
};

class PhysicsCompUpdateEvent : public Event {
public:
  PhysicsCompUpdateEvent(const PhysicsData& data, Handle owner);

  PhysicsData mData;
  Handle mOwner;
};

class Physics : public Component {
public:
  Physics(Handle owner);
  Physics(const Physics& other);

  const PhysicsData& getData() const {
    return mData;
  }

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  void setData(const PhysicsData& data);
  void setCollider(Handle model, Handle material);
  void setRigidbody(const Syx::Vec3& linVel, const Syx::Vec3& angVel);
  void setPhysToModel(const Syx::Mat4& physToModel);
  void setLinVel(const Syx::Vec3& linVel);
  void setAngVel(const Syx::Vec3& angVel);

  const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(Physics);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  PhysicsData mData;
};