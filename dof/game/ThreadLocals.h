#pragma once

struct AppTaskArgs;
struct IRandom;
struct StableElementMappings;
class RuntimeDatabase;
struct IDatabase;
class IDBEvents;

namespace Tasks {
  struct ILocalSchedulerFactory;
  struct ILocalScheduler;
}

using ThreadLocalDatabaseFactory = std::function<std::unique_ptr<IDatabase>()>;

struct ThreadLocalData {
  RuntimeDatabase* statEffects{};
  IDBEvents* events{};
  //This is a hack to get at the mappings from apptaskargs
  //At the moment it's not really a problem since having access to this doesn't affect task scheduling
  StableElementMappings* mappings{};
  IRandom* random{};
  Tasks::ILocalScheduler* scheduler{};
};

namespace details {
  struct ThreadLocalsImpl;
};

struct ThreadLocals {
  //TODO: this initialization and abstraction is confusing. ownership should probably be up at the app level rather than in the db
  ThreadLocals(size_t size,
    IDBEvents* events,
    StableElementMappings* mappings,
    Tasks::ILocalSchedulerFactory* schedulerFactory,
    const ThreadLocalDatabaseFactory& dbf
  );
  ~ThreadLocals();

  ThreadLocalData get(size_t thread);
  size_t getThreadCount() const;

  std::unique_ptr<details::ThreadLocalsImpl> data;
};