#pragma once
#include "event/Event.h"
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

class System : public std::enable_shared_from_this<System>, public EventListener {
public:
  DECLARE_TYPE_CATEGORY
  System(const SystemArgs& args, typeId_t<System> type);
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

  typeId_t<System> getType() const { return mType; };

protected:
  template<class T>
  static typeId_t<System> _typeId() { return typeId<T, System>(); }

  template<class T>
  static std::weak_ptr<T> _getWeakThis(T& self) {
    return std::static_pointer_cast<T>(self.shared_from_this());
  }

  template<class EventT, class Derived>
  void _registerSystemEventHandler(void(Derived::* handler)(const EventT&)) {
    mEventHandler->registerEventListener(std::weak_ptr<Derived>(std::static_pointer_cast<Derived>(shared_from_this())), handler);
  }

  template<class SystemT>
  void _onCallbackEvent(const CallbackEvent& e) {
    e.tryHandle(_typeId<SystemT>());
  }

  template<class Self>
  static void _registerCallbackEventHandler(Self& self) {
    self.mEventHandler->registerEventListener<CallbackEvent, Self>(_getWeakThis(self), &Self::_onCallbackEvent<Self>);
  }

  SystemArgs mArgs;
  const EventBuffer* mEventBuffer;
  std::unique_ptr<EventHandler> mEventHandler;
  const typeId_t<System> mType;
};

class LuaGameSystem;

class ISystemRegistry {
public:
  virtual ~ISystemRegistry() = default;
  virtual void registerSystem(std::shared_ptr<System> system) = 0;
  virtual std::vector<std::shared_ptr<System>> takeSystems() = 0;
};

namespace Registry {
  std::unique_ptr<ISystemRegistry> createSystemRegistry();
}