#pragma once
namespace Syx {
  template<typename EventType>
  struct EventListener {
    std::vector<EventType> mEvents;
  };

  struct UpdateEvent {
    Handle mHandle;
    Vec3 mPos;
    Quat mRot;
    Vec3 mLinVel;
    Vec3 mAngVel;
  };
}