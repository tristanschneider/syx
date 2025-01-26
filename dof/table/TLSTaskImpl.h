#pragma once

#include "ITaskImpl.h"
#include "AppBuilder.h"

struct DefaultTaskGroup {
  DefaultTaskGroup(RuntimeDatabaseTaskBuilder&) {}
};
struct DefaultTaskLocals {
  DefaultTaskLocals(AppTaskArgs&) {}
};

template<class GroupT, class LocalT, class UnitT, class... Args>
class TLSTaskImpl : public ITaskImpl {
public:
  struct Worker {
    //Optional as a simple way to delay construction until the init call, avoiding a default initializer requirment
    //Causes unfortunate padding, hopefully space here isn't essential
    std::optional<UnitT> unit;
    std::optional<LocalT> local;
  };

  TLSTaskImpl(std::string_view n, Args&&... args)
    : name{ n }
    , argsTuple{ std::forward<Args>(args)... }
  {
  }

  void setWorkerCount(size_t count) final {
    assert(count);
    workerCount = static_cast<uint8_t>(count);
    workers = std::make_unique<Worker[]>(static_cast<size_t>(workerCount));
  }

  template<size_t... I>
  void initGroup(RuntimeDatabaseTaskBuilder& builder, std::index_sequence<I...>) {
    group.emplace(builder, std::move(std::get<I>(argsTuple))...);
  }

  AppTaskMetadata init(RuntimeDatabase& db) final {
    RuntimeDatabaseTaskBuilder builder{ db };
    builder.discard();
    initGroup(builder, std::make_index_sequence<sizeof...(Args)>());
    for(uint8_t i = 0; i < workerCount; ++i) {
      getWorker(i).unit.emplace(builder);
    }

    //Fill metadata with the result of all of them
    //The rest after first could copy rather than constructing from RDTB but that's annoying to write
    AppTaskWithMetadata meta = std::move(builder).finalize();
    meta.data.name = name;
    return meta.data;
  }

  void initThreadLocal(AppTaskArgs& args) final {
    assert(args.threadIndex < static_cast<size_t>(workerCount));
    getWorker(args.threadIndex).local.emplace(args);
  }

  void execute(AppTaskArgs& args) final {
    Worker& worker = getWorker(args.threadIndex);
    assert(worker.local && worker.unit && group);
    if constexpr(!std::is_same_v<DefaultTaskGroup, GroupT> && !std::is_same_v<DefaultTaskLocals, LocalT>) {
      worker.unit->execute(*group, *worker.local, args);
    }
    else if constexpr(!std::is_same_v<DefaultTaskGroup, GroupT>) {
      worker.unit->execute(*group, args);
    }
    else if constexpr(!std::is_same_v<DefaultTaskLocals, LocalT>) {
      worker.unit->execute(*worker.local, args);
    }
    else {
      worker.unit->execute(args);
    }
  }

  std::shared_ptr<AppTaskConfig> getConfig() final {
    return config;
  }

  std::shared_ptr<AppTaskConfig> getOrAddConfig() {
    if(!config) {
      config = std::make_shared<AppTaskConfig>();
    }
    return config;
  }

  void setPinning(AppTaskPinning::Variant p) {
    pinning = p;
  }

  AppTaskPinning::Variant getPinning() final {
    return pinning;
  }

  Worker& getWorker(size_t i) {
    return workers[i];
  }

  std::string_view name;
  std::unique_ptr<Worker[]> workers;
  std::optional<GroupT> group;
  uint8_t workerCount{};
  std::shared_ptr<AppTaskConfig> config;
  std::vector<std::shared_ptr<AppTaskConfig>> childConfigs;
  AppTaskPinning::Variant pinning;
  std::tuple<Args...> argsTuple;
};

namespace TLSTask {
  template<class UnitT, class GroupT = DefaultTaskGroup, class LocalT = DefaultTaskLocals>
  std::unique_ptr<TLSTaskImpl<GroupT, LocalT, UnitT>> create(std::string_view name) {
    return std::make_unique<TLSTaskImpl<GroupT, LocalT, UnitT>>(name);
  }

  template<class UnitT, class GroupT, class LocalT, class... Args>
  auto createWithArgs(std::string_view name, Args&&... args) {
    return std::make_unique<TLSTaskImpl<GroupT, LocalT, UnitT, Args...>>(name, std::forward<Args>(args)...);
  }
}