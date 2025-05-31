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
  virtual void clearEvents(IAppBuilder&) {}
};

class CompositeAppModule : public IAppModule {
public:
  CompositeAppModule(std::vector<std::unique_ptr<IAppModule>> m)
    : modules{ std::move(m) }
  {
  }

  void createDatabase(RuntimeDatabaseArgs& args) override {
    for(auto&& m : modules) {
      m->createDatabase(args);
    }
  }

  void createDependentDatabase(RuntimeDatabaseArgs& args) override {
    for(auto&& m : modules) {
      m->createDependentDatabase(args);
    }
  }

  void initScheduler(IAppBuilder& builder, const ThreadLocalDatabaseFactory& factory) override {
    for(auto&& m : modules) {
      m->initScheduler(builder, factory);
    }
  }

  void init(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->init(builder);
    }
  }

  void dependentInit(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->dependentInit(builder);
    }
  }

  void update(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->update(builder);
    }
  }

  void preProcessEvents(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->preProcessEvents(builder);
    }
  }

  void processEvents(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->processEvents(builder);
    }
  }

  void postProcessEvents(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->postProcessEvents(builder);
    }
  }

  void clearEvents(IAppBuilder& builder) override {
    for(auto&& m : modules) {
      m->clearEvents(builder);
    }
  }

private:
  std::vector<std::unique_ptr<IAppModule>> modules;
};