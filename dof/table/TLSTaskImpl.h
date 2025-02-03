#pragma once

#include "ITaskImpl.h"
#include "AppBuilder.h"

struct DefaultTaskGroup {
  DefaultTaskGroup(RuntimeDatabaseTaskBuilder&) {}
};
struct DefaultTaskLocals {
  DefaultTaskLocals(AppTaskArgs&) {}
};

template<class T, class... Args>
concept RuntimeTaskConstructable = requires(RuntimeDatabaseTaskBuilder& task, Args&&... args) {
  T{ task, std::forward<Args>(args)... };
};

template<class T, class... Args>
concept TaskWorkGroup = RuntimeTaskConstructable<T, Args...>;
template<class T>
concept TaskLocal = requires(AppTaskArgs& args) {
  T{ args };
};
template<class T, class Group, class Local>
concept GroupLocalExecutable = requires(T& t, Group& group, Local& local, AppTaskArgs& args) {
  t.execute(group, local, args);
};
template<class T, class Group, class Local>
concept GroupExecutable = requires(T& t, Group& group, Local& local, AppTaskArgs& args) {
  t.execute(group, args);
};
template<class T, class Group, class Local>
concept LocalExecutable = requires(T& t, Group& group, Local& local, AppTaskArgs& args) {
  t.execute(local, args);
};
template<class T, class Group, class Local>
concept ArgsExecutable = requires(T& t, Group& group, Local& local, AppTaskArgs& args) {
  t.execute(args);
};
template<class T, class Group, class Local>
concept TaskUnit = (RuntimeTaskConstructable<T> || RuntimeTaskConstructable<T, Group&>) && (
  GroupLocalExecutable<T, Group, Local> ||
  GroupExecutable<T, Group, Local> ||
  LocalExecutable<T, Group, Local> ||
  ArgsExecutable<T, Group, Local>
);

template<class GroupT, class LocalT, class UnitT, class... Args>
requires TaskWorkGroup<GroupT, Args...> && TaskLocal<LocalT> && TaskUnit<UnitT, GroupT, LocalT>
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

  template<class T> requires RuntimeTaskConstructable<typename T::value_type>
  void initUnit(T& unit, RuntimeDatabaseTaskBuilder& builder) {
    unit.emplace(builder);
  }

  template<class T> requires RuntimeTaskConstructable<typename T::value_type, GroupT>
  void initUnit(T& unit, RuntimeDatabaseTaskBuilder& builder) {
    unit.emplace(builder, *group);
  }

  AppTaskMetadata init(RuntimeDatabase& db) final {
    RuntimeDatabaseTaskBuilder builder{ db };
    builder.discard();
    initGroup(builder, std::make_index_sequence<sizeof...(Args)>());
    for(uint8_t i = 0; i < workerCount; ++i) {
      initUnit(getWorker(i).unit, builder);
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

  template<GroupLocalExecutable<GroupT, LocalT> Exe>
  void executeImpl(Exe& e, Worker& worker, AppTaskArgs& args) {
    e.execute(*group, *worker.local, args);
  }

  template<GroupExecutable<GroupT, LocalT> Exe>
  void executeImpl(Exe& e, Worker&, AppTaskArgs& args) {
    e.execute(*group, args);
  }

  template<LocalExecutable<GroupT, LocalT> Exe>
  void executeImpl(Exe& e, Worker& worker, AppTaskArgs& args) {
    e.execute(*worker.local, args);
  }

  template<ArgsExecutable<GroupT, LocalT> Exe>
  void executeImpl(Exe& e, Worker&, AppTaskArgs& args) {
    e.execute(args);
  }

  void execute(AppTaskArgs& args) final {
    Worker& worker = getWorker(args.threadIndex);
    assert(worker.local && worker.unit && group);
    executeImpl(*worker.unit, worker, args);
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
  requires TaskUnit<UnitT, GroupT, LocalT> && TaskWorkGroup<GroupT> && TaskLocal<LocalT>
  auto create(std::string_view name) {
    static_assert(TaskUnit<UnitT, GroupT, LocalT>);
    return std::make_unique<TLSTaskImpl<GroupT, LocalT, UnitT>>(name);
  }


  template<class UnitT, class GroupT, class LocalT, class... Args>
  requires TaskUnit<UnitT, GroupT, LocalT> && TaskWorkGroup<GroupT, Args...> && TaskLocal<LocalT>
  auto createWithArgs(std::string_view name, Args&&... args) {
    return std::make_unique<TLSTaskImpl<GroupT, LocalT, UnitT, Args...>>(name, std::forward<Args>(args)...);
  }
}