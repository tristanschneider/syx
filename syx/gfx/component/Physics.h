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

  Handle getHandle() const override {
    return static_cast<Handle>(EventType::PhysicsCompUpdate);
  }
  std::unique_ptr<Event> clone() const override;

  PhysicsData mData;
  Handle mOwner;
};

class Physics : public Component {
public:
  Physics(Handle owner, MessagingSystem& messaging);

  Handle getHandle() const override {
    return static_cast<Handle>(ComponentType::Physics);
  }

  const PhysicsData& getData() const {
    return mData;
  }

  void setData(const PhysicsData& data, bool fireEvent = true);
  void setCollider(Handle model, Handle material);
  void setRigidbody(const Syx::Vec3& linVel, const Syx::Vec3& angVel);
  void setPhysToModel(const Syx::Mat4& physToModel);

private:
  void fireUpdateEvent();

  PhysicsData mData;
};