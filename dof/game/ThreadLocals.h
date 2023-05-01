#pragma once

struct StatEffectDBOwned;

struct ThreadLocalData {
  StatEffectDBOwned* statEffects{};
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