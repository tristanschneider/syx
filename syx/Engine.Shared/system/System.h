#pragma once

class App;
class IWorkerPool;
class Task;
class EventBuffer;
class EventHandler;
class SystemProvider;
class AppPlatform;
class MessageQueueProvider;
class GameObjectHandleProvider;
class ProjectLocator;

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
  class Registry {
  public:
    using SystemConstructor = std::function<std::unique_ptr<System>(const SystemArgs&)>;

    static size_t registerSystem(SystemConstructor systemConstructor);
    static void getSystems(const SystemArgs& args, std::vector<std::unique_ptr<System>>& result);

  private:
    Registry();
    ~Registry();

    static Registry& _get();

    std::vector<SystemConstructor> mSystems;
  };

  template<typename System>
  struct StaticRegisterSystem {
    StaticRegisterSystem() {
      mID = Registry::registerSystem([](const SystemArgs&) { //args) {
        // TODO: fix this
        return nullptr;
        //return std::make_unique<System>(args);
      });
    }

    size_t mID;
  };

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
  SystemArgs mArgs;
  const EventBuffer* mEventBuffer;
  std::unique_ptr<EventHandler> mEventHandler;
};

//Must be used in class scope of system, then in cpp. Registers it and assigns an id for lookup
//Example usage class PhysicsSystem { RegisterSystem(PhysicsSystem) }
#define RegisterSystemH(system)\
static System::StaticRegisterSystem<system> sSystemReg;
#define RegisterSystemCPP(system)\
System::StaticRegisterSystem<system> system::sSystemReg;
#define GetSystemID(system)\
system::sSystemReg.mID