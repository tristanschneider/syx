#include "Precompile.h"
#include "ThreadLocals.h"

#include "AppBuilder.h"
#include "Random.h"
#include "stat/AllStatEffects.h"
#include "ILocalScheduler.h"

namespace details {
  RuntimeDatabaseArgs buildStatDB(StableElementMappings* mappings) {
    RuntimeDatabaseArgs args{
      .mappings = mappings
    };
    StatEffect::createDatabase(args);
    return args;
  }

  struct ThreadData {
    ThreadData(StableElementMappings* mappings)
      : statEffects{ buildStatDB(mappings) }
    {
    }

    RuntimeDatabase statEffects;
    std::unique_ptr<IRandom> random = Random::twister();
    std::unique_ptr<Tasks::ILocalScheduler> scheduler;
  };

  struct ThreadLocalsImpl {
    //No desire for contiguous memory of the data itself as that would only encourage false sharing
    std::vector<std::unique_ptr<ThreadData>> threads;
    Events::EventsImpl* events{};
    StableElementMappings* mappings{};
  };
};

ThreadLocals::ThreadLocals(size_t size,
  Events::EventsImpl* events,
  StableElementMappings* mappings,
  Tasks::ILocalSchedulerFactory* schedulerFactory
)
  : data(std::make_unique<details::ThreadLocalsImpl>()) {
  data->threads.resize(size);
  data->events = events;
  data->mappings = mappings;
  for(size_t i = 0; i < size; ++i) {
    auto t = std::make_unique<details::ThreadData>(mappings);
    if(schedulerFactory) {
      t->scheduler = schedulerFactory->create();
    }
    data->threads[i] = std::move(t);
  }
}

ThreadLocals::~ThreadLocals() = default;

ThreadLocalData& ThreadLocalData::get(AppTaskArgs& args) {
  //Assumed to be provided by GameScheduler.cpp
  return *static_cast<ThreadLocalData*>(args.threadLocal);
}

ThreadLocalData ThreadLocals::get(size_t thread) {
  assert(data->threads.size() > thread);
  auto& t = data->threads[thread];
  return {
    &t->statEffects,
    data->events,
    data->mappings,
    t->random.get(),
    t->scheduler.get()
  };
}

size_t ThreadLocals::getThreadCount() const {
  return data->threads.size();
}
