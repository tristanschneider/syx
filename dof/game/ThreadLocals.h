#pragma once

struct AppTaskArgs;
struct StatEffectDBOwned;

namespace Events {
  struct EventsImpl;
};

struct ThreadLocalData {
  static ThreadLocalData& get(AppTaskArgs& args);

  StatEffectDBOwned* statEffects{};
  Events::EventsImpl* events{};
};

namespace details {
  struct ThreadLocalsImpl;
};

struct ThreadLocals {
  //TODO: this initialization and abstraction is confusing. ownership should probably be up at the app level rather than in the db
  ThreadLocals(size_t size, Events::EventsImpl* events);
  ~ThreadLocals();

  ThreadLocalData get(size_t thread);
  size_t getThreadCount() const;

  std::unique_ptr<details::ThreadLocalsImpl> data;
};