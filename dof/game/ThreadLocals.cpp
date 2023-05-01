#include "Precompile.h"
#include "ThreadLocals.h"

#include "stat/AllStatEffects.h"

namespace details {
  struct ThreadData {
    StatEffectDBOwned statEffects;
  };

  struct ThreadLocalsImpl {
    //No desire for contiguous memory of the data itself as that would only encourage false sharing
    std::vector<std::unique_ptr<ThreadData>> threads;
  };
};

ThreadLocals::ThreadLocals(size_t size)
  : data(std::make_unique<details::ThreadLocalsImpl>()) {
  data->threads.resize(size);
  for(size_t i = 0; i < size; ++i) {
    auto t = std::make_unique<details::ThreadData>();
    StatEffect::initGlobals(t->statEffects.db);

    data->threads[i] = std::move(t);
  }
}

ThreadLocals::~ThreadLocals() = default;

ThreadLocalData ThreadLocals::get(size_t thread) {
  assert(data->threads.size() > thread);
  auto& t = data->threads[thread];
  return {
    &t->statEffects
  };
}

size_t ThreadLocals::getThreadCount() const {
  return data->threads.size();
}
