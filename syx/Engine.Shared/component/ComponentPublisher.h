#pragma once

class Component;
class EventBuffer;
struct lua_State;
class MessageQueueProvider;

//A const wrapper to push to the lua stack to discourage accidental modifications to components outside of the messaging system.
class ComponentPublisher {
public:
  ComponentPublisher(const Component& component);
  ComponentPublisher(const ComponentPublisher&) = default;
  ComponentPublisher& operator=(const ComponentPublisher&) = default;

  const Component* get() const;
  template<class T>
  const T* get() const {
    return static_cast<const T*>(mComponent);
  }
  const Component& operator*() const;
  const Component* operator->() const;

  void publish(const Component& component, lua_State* l) const;
  void publish(const Component& component, MessageQueueProvider& msg) const;
  static void forcePublish(const Component& component, EventBuffer& events, uint64_t diff = ~0, size_t fromSystem = 0);

private:
  const Component* mComponent = nullptr;
};
