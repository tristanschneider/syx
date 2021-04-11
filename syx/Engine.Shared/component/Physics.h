#pragma once
#include "Component.h"

#include "event/Event.h"
#include <optional>
#include <variant>
#include "SyxMat4.h"
#include "SyxVec3.h"

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

//Directly setting the velocity would cancel out a physics update, use these events to apply forces
//in-line with the physics simulation
//TODO: is some kind of generic transaction mechanism needed for property update issues like this?
struct ApplyForceEvent : public TypedEvent<ApplyForceEvent> {
  enum class Mode : uint8_t {
    Force,
    Impulse,
  };
  struct ForceAtPoint {
    Syx::Vec3 mPoint;
    Syx::Vec3 mForce;
  };
  struct Force {
    Syx::Vec3 mLinear;
    Syx::Vec3 mAngular;
  };

  ApplyForceEvent(Handle obj, const ForceAtPoint& atPoint, Mode mode)
    : mForce(atPoint)
    , mMode(mode)
    , mObj(obj) {
  }

  ApplyForceEvent(Handle obj, const Force& plainForce, Mode mode)
    : mForce(plainForce)
    , mMode(mode)
    , mObj(obj) {
  }

  std::variant<Force, ForceAtPoint> mForce;
  Handle mObj = 0;
  Mode mMode = Mode::Force;
};

class Physics : public TypedComponent<Physics> {
public:
  using TypedComponent::TypedComponent;
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