#pragma once

struct AppTaskArgs;
struct IRandom;
struct StableElementMappings;
class RuntimeDatabase;

namespace Events {
  struct EventsImpl;
};

namespace Tasks {
  struct ILocalSchedulerFactory;
  struct ILocalScheduler;
}

struct ThreadLocalData {
  static ThreadLocalData& get(AppTaskArgs& args);

  RuntimeDatabase* statEffects{};
  Events::EventsImpl* events{};
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
    Events::EventsImpl* events,
    StableElementMappings* mappings,
    Tasks::ILocalSchedulerFactory* schedulerFactory
  );
  ~ThreadLocals();

  ThreadLocalData get(size_t thread);
  size_t getThreadCount() const;

  std::unique_ptr<details::ThreadLocalsImpl> data;
};