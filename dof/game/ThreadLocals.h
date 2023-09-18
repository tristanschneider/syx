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
  ThreadLocals(size_t size);
  ~ThreadLocals();

  ThreadLocalData get(size_t thread);
  size_t getThreadCount() const;

  std::unique_ptr<details::ThreadLocalsImpl> data;
};