#pragma once
#include "util/TypeId.h"

class App;
class IWorkerPool;
class Task;
class Event;
class EventBuffer;
class EventHandler;
class SystemProvider;
class AppPlatform;
class MessageQueueProvider;
class GameObjectHandleProvider;
class ProjectLocator;

//TODO: Update all uses to use template
#define SYSTEM_EVENT_HANDLER(eventType, handler) mEventHandler->registerEventHandler(Event::typeId<eventType>(), [this](const Event& e) {\
    handler(static_cast<const eventType&>(e));\
  });

struct SystemArgs {
  IWorkerPool* mPool;
  SystemProvider* mSystems;
  MessageQueueProvider* mMessages;
  GameObjectHandleProvider* mGameObjectGen;
  const ProjectLocator* mProjectLocator;
  AppPlatform* mAppPlatform;
};

class System {
public:
  DECLARE_TYPE_CATEGORY
  System(const SystemArgs& args);
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

protected:
  template<class EventT>
  std::function<void(Event)> _registerSystemEventHandler(void(System::* handler)(const EventT&)) {
    return [this, handler](const Event& event) {
      this.*handler(static_cast<const EventT&>(event));
    };
  }

  SystemArgs mArgs;
  const EventBuffer* mEventBuffer;
  std::unique_ptr<EventHandler> mEventHandler;
};

class ISystemRegistry {
public:
  virtual void registerSystem(std::unique_ptr<System> system) = 0;
  virtual std::vector<std::unique_ptr<System>> takeSystems() = 0;
};

namespace Registry {
  std::unique_ptr<ISystemRegistry> createSystemRegistry();
}