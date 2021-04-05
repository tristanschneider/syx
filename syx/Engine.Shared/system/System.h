#pragma once
#include "event/EventHandler.h"
#include "util/TypeId.h"

class App;
class AppPlatform;
class ComponentRegistryProvider;
class Event;
class EventBuffer;
class EventHandler;
class GameObjectHandleProvider;
struct IIDRegistry;
class IWorkerPool;
class MessageQueueProvider;
class ProjectLocator;
class SystemProvider;
class Task;

namespace FileSystem {
  struct IFileSystem;
}

//TODO: Update all uses to use template
#define SYSTEM_EVENT_HANDLER(eventType, handler) mEventHandler->registerEventHandler(Event::typeId<eventType>(), [this](const Event& e) {\
    handler(static_cast<const eventType&>(e));\
  });

//This contains information that should be available to all systems. Anything exposed here should be safe for the system to use as it sees fit
//TODO: this is a bit clunky. It would probably be better to have a more formal mechanism to do this
struct SystemArgs {
  IWorkerPool* mPool = nullptr;
  SystemProvider* mSystems = nullptr;
  MessageQueueProvider* mMessages = nullptr;
  GameObjectHandleProvider* mGameObjectGen = nullptr;
  const ProjectLocator* mProjectLocator = nullptr;
  AppPlatform* mAppPlatform = nullptr;
  ComponentRegistryProvider* mComponentRegistry = nullptr;
  FileSystem::IFileSystem* mFileSystem = nullptr;
  IIDRegistry* mIDRegistry = nullptr;
};

class System {
public:
  DECLARE_TYPE_CATEGORY
  System(const SystemArgs& args, size_t type);
  virtual ~System();

  virtual void init() {}
  //Each frame queueTasks is called on all system, then update on all of them.
  //Queueing should be done in queueTasks so the work can be done in the background while any main thread work is done in update
  virtual void queueTasks(float, IWorkerPool&, std::shared_ptr<Task>) {}
  virtual void update(float, IWorkerPool&, std::shared_ptr<Task>) {}
  virtual void uninit() {}

  //Each frame this is updated to point at the message queue for that frame.
  void setEventBuffer(const EventBuffer* buffer);

  SystemProvider& getSystemProvider() const;
  MessageQueueProvider& getMessageQueueProvider() const;

  size_t getType() const { return mType; };

protected:
  template<class T>
  static size_t _typeId() { return typeId<T, System>(); }

  template<class EventT, class Derived>
  void _registerSystemEventHandler(void(Derived::* handler)(const EventT&)) {
    mEventHandler->registerEventHandler([this, handler](const EventT& event) {
      (static_cast<Derived*>(this)->*handler)(static_cast<const EventT&>(event));
    });
  }

  SystemArgs mArgs;
  const EventBuffer* mEventBuffer;
  std::unique_ptr<EventHandler> mEventHandler;
  const size_t mType;
};

class LuaGameSystem;

class ISystemRegistry {
public:
  virtual ~ISystemRegistry() = default;
  virtual void registerSystem(std::unique_ptr<System> system) = 0;
  virtual std::vector<std::unique_ptr<System>> takeSystems() = 0;
};

namespace Registry {
  std::unique_ptr<ISystemRegistry> createSystemRegistry();
}