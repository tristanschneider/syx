#pragma once
#include <array>

namespace Syx {
  template<typename EventType>
  struct EventListener {
    std::vector<EventType> mEvents;
  };

  struct IEventSubscription {
    virtual ~IEventSubscription() = default;
  };

  struct UpdateEvent {
    Handle mHandle = 0;
    Vec3 mPos;
    Quat mRot;
    Vec3 mLinVel;
    Vec3 mAngVel;
  };

  struct CollisionEventSubscription : public IEventSubscription {};

  struct CollisionEvent {
    static constexpr size_t MAX_CONTACT_POINTS = 4;

    enum class Type : uint8_t {
      Start,
      Update,
      End,
    };

    struct Contact {
      Vec3 mOnA;
      Vec3 mOnB;
      Vec3 mNormal;

      float getPenetration() const {
        return std::abs((mOnA - mOnB).dot(mNormal));
      }
    };

    Handle mA = 0;
    Handle mB = 0;
    size_t mNumContacts = 0;
    Type mEventType = Type::Start;
    std::array<Contact, MAX_CONTACT_POINTS> mContacts;
  };
}