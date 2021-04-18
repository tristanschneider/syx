#include "Event.h"

#include <SyxEvents.h>

struct IPhysicsSubscription {
  virtual ~IPhysicsSubscription() = default;
};

struct PhysicsSubscriptionResponse : public TypedEvent<PhysicsSubscriptionResponse> {
  std::shared_ptr<IPhysicsSubscription> mSubscription;
};

struct PhysicsSubscriptionRequest : public RequestEvent<PhysicsSubscriptionRequest, PhysicsSubscriptionResponse> {
  Handle mObj = 0;
};

struct OnCollisionStart : public TypedEvent<OnCollisionStart> {
  Syx::CollisionEvent mData;
};

struct OnCollisionEnd : public TypedEvent<OnCollisionEnd> {
  Handle mA = 0;
  Handle mB = 0;
};

struct OnCollisionUpdate : public TypedEvent<OnCollisionUpdate> {
  Syx::CollisionEvent mData;
};