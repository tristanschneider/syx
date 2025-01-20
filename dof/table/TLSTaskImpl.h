#pragma once

#include "ITaskImpl.h"
#include "AppBuilder.h"

struct DefaultTaskGroup {
  DefaultTaskGroup(RuntimeDatabaseTaskBuilder&) {}
};
struct DefaultTaskLocals {
  DefaultTaskLocals(AppTaskArgs&) {}
};

template<class GroupT, class LocalT, class UnitT>
class TLSTaskImpl : public ITaskImpl {
public:
  struct Worker {
    //Optional as a simple way to delay construction until the init call, avoiding a default initializer requirment
    //Causes unfortunate padding, hopefully space here isn't essential
    std::optional<UnitT> unit;
    std::optional<LocalT> local;
  };

  TLSTaskImpl(std::string_view n)
    : name{ n }
  {
  }

  void setWorkerCount(size_t count) final {
    assert(count);
    workerCount = static_cast<uint8_t>(count);
    workers = std::make_unique<Worker[]>(static_cast<size_t>(workerCount));
  }

  AppTaskMetadata init(RuntimeDatabase& db) final {
    RuntimeDatabaseTaskBuilder builder{ db };
    builder.discard();
    group.emplace(builder);
    for(uint8_t i = 0; i < workerCount; ++i) {
      getWorker(i).unit.emplace(builder);
    }

    //Fill metadata with the result of all of them
    //The rest after first could copy rather than constructing from RDTB but that's annoying to write
    AppTaskWithMetadata meta = std::move(builder).finalize();
    config = std::move(meta.task.config);
    pinning = meta.task.pinning;
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
  AppTaskPinning::Variant pinning;
};

namespace TLSTask {
  template<class UnitT, class GroupT = DefaultTaskGroup, class LocalT = DefaultTaskLocals>
  std::unique_ptr<TLSTaskImpl<GroupT, LocalT, UnitT>> create(std::string_view name) {
    return std::make_unique<TLSTaskImpl<GroupT, LocalT, UnitT>>(name);
  }
}