#pragma once

class IAppBuilder;
struct RuntimeDatabaseArgs;
struct IDatabase;

using ThreadLocalDatabaseFactory = std::function<std::unique_ptr<IDatabase>()>;

class IAppModule {
public:
  virtual ~IAppModule() = default;

  //Add the desired tables to the database
  virtual void createDatabase(RuntimeDatabaseArgs&) {}
  //Second pass if a module wants to add tables due to the existence of other tables from other modules
  virtual void createDependentDatabase(RuntimeDatabaseArgs&) {}

  //Single threaded initialization of the scheduler itself. Minimize work here.
  virtual void initScheduler(IAppBuilder&, const ThreadLocalDatabaseFactory&) {}
  //Multi-threaded initialization intended for setting defaults and perhaps singletons
  virtual void init(IAppBuilder&) {}
  virtual void dependentInit(IAppBuilder&) {}

  virtual void update(IAppBuilder&) {}

  virtual void preProcessEvents(IAppBuilder&) {}
  virtual void processEvents(IAppBuilder&) {}
  virtual void postProcessEvents(IAppBuilder&) {}
};