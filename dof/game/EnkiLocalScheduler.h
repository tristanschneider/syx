#pragma once

struct Scheduler;

namespace Tasks {
  struct ILocalSchedulerFactory;

  std::unique_ptr<ILocalSchedulerFactory> createEnkiSchedulerFactory(Scheduler& scheduler);
};