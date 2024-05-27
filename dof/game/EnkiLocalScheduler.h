#pragma once

struct Scheduler;
struct ThreadLocalData;

namespace Tasks {
  struct ILocalSchedulerFactory;
  using GetThreadLocal = std::function<ThreadLocalData(size_t)>;

  std::unique_ptr<ILocalSchedulerFactory> createEnkiSchedulerFactory(Scheduler& scheduler, GetThreadLocal&& getThread);
};