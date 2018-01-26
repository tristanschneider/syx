#pragma once

class App;
class IWorkerPool;
class Task;
class EventListener;

#define SYSTEM_EVENT_HANDLER(eventType, handler) mListener->registerEventHandler(Event::typeId<eventType>(), [this](const Event& e) {\
    handler(static_cast<const eventType&>(e));\
  });

class System {
public:
  class Registry {
  public:
    using SystemConstructor = std::function<std::unique_ptr<System>(App&)>;

    static size_t registerSystem(SystemConstructor systemConstructor);
    static void getSystems(App& app, std::vector<std::unique_ptr<System>>& result);

  private:
    Registry();
    ~Registry();

    static Registry& _get();

    std::vector<SystemConstructor> mSystems;
  };

  template<typename System>
  struct StaticRegisterSystem {
    StaticRegisterSystem() {
      mID = Registry::registerSystem([](App& app) {
        return std::make_unique<System>(app);
      });
    }

    size_t mID;
  };

  System(App& app);
  virtual ~System();

  virtual void init() {}
  virtual void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {}
  virtual void uninit() {}

  EventListener* getListener();

protected:
  App& mApp;
  std::unique_ptr<EventListener> mListener;
};

//Must be used in class scope of system, then in cpp. Registers it and assigns an id for lookup
//Example usage class PhysicsSystem { RegisterSystem(PhysicsSystem) }
#define RegisterSystemH(system)\
static System::StaticRegisterSystem<system> sSystemReg;
#define RegisterSystemCPP(system)\
System::StaticRegisterSystem<system> system::sSystemReg;
#define GetSystemID(system)\
system::sSystemReg.mID