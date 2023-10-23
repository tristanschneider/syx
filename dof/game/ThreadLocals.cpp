#include "Precompile.h"
#include "ThreadLocals.h"

#include "AppBuilder.h"
#include "stat/AllStatEffects.h"

namespace details {
  struct ThreadData {
    StatEffectDatabase statEffects;
  };

  struct ThreadLocalsImpl {
    //No desire for contiguous memory of the data itself as that would only encourage false sharing
    std::vector<std::unique_ptr<ThreadData>> threads;
    Events::EventsImpl* events{};
    StableElementMappings* mappings{};
  };
};

ThreadLocals::ThreadLocals(size_t size, Events::EventsImpl* events, StableElementMappings* mappings)
  : data(std::make_unique<details::ThreadLocalsImpl>()) {
  data->threads.resize(size);
  data->events = events;
  data->mappings = mappings;
  for(size_t i = 0; i < size; ++i) {
    auto t = std::make_unique<details::ThreadData>();
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
    data->mappings
  };
}

size_t ThreadLocals::getThreadCount() const {
  return data->threads.size();
}
