#pragma once

#include "ITaskImpl.h"
#include "AppBuilder.h"

struct DefaultTaskGroup {
  void init() {}
};

template<class T, class... Args>
concept ArgsConstructable = requires(T& t, Args&&... args) { t.init(std::forward<Args>(args)...); };

//UnitT is initialized by invoking `UnitT::init()` with various argument types.
//Below are the supported argument types, of which multiple are allowed
//Allowed constructors for UnitT, which supports varying numbers of arguments in the order:
//RuntimeDatabaseTaskBuilder, AppTaskArgs, GroupT
//Allowed single argument constructors
template<class T>
concept UnitTaskArgs = ArgsConstructable<T, RuntimeDatabaseTaskBuilder&>;
template<class T>
concept UnitLocalArgs = ArgsConstructable<T, AppTaskArgs&>;
template<class T, class GroupT>
concept UnitGroupArgs = ArgsConstructable<T, GroupT&>;
template<class T, class... Args>
concept UnitArgsArgs = ArgsConstructable<T, Args...>;
template<class T>
concept UnitEmptyArgs = ArgsConstructable<T>;

template<class T, class GroupT, class... Args>
concept UnitAnyConstrutable = 
  UnitTaskArgs<T> ||
  UnitLocalArgs<T> ||
  UnitGroupArgs<T, GroupT> ||
  UnitArgsArgs<T, Args...> ||
  UnitEmptyArgs<T>;

//Allowed constructors for GroupT
template<class T>
concept GroupTaskArgs = ArgsConstructable<T, RuntimeDatabaseTaskBuilder&>;
template<class T, class... Args>
concept GroupArgsArgs = ArgsConstructable<T, Args...>;
template<class T>
concept GroupEmptyArgs = ArgsConstructable<T>;

template<class T, class... Args>
concept AnyGroupConstructable =
  GroupTaskArgs<T> ||
  GroupArgsArgs<T, Args...> ||
  GroupEmptyArgs<T>;

template<class T, class... Args>
concept ArgsExecutable = requires(T& t, Args&&... args) { t.execute(std::forward<Args>(args)...); };

//Allowed forms of `UnitT::execute`, arguments optional and allowed in the order:
//AppTaskArgs, GroupT
template<class T>
concept UnitEmptyExe = ArgsExecutable<T>;
template<class T>
concept UnitLocalExe = ArgsExecutable<T, AppTaskArgs&>;
template<class T, class GroupT>
concept UnitGroupExe = ArgsExecutable<T, GroupT&>;
template<class T, class GroupT>
concept UnitLocalGroup = ArgsExecutable<T, AppTaskArgs&, GroupT&>;

template<class T, class GroupT>
concept UnitAnyExecutable =
  UnitEmptyExe<T> ||
  UnitLocalExe<T> ||
  UnitGroupExe<T, GroupT> ||
  UnitLocalGroup<T, GroupT>;

template<class T, class GroupT>
concept AnyTaskUnit = UnitAnyConstrutable<T, GroupT> && UnitAnyExecutable<T, GroupT>;
template<class GroupT, class... Args>
concept AnyTaskGroup = AnyGroupConstructable<GroupT, Args...>;

namespace TlsDispatch {
  template<class GroupT, class... Args>
  void initGroup([[maybe_unused]] GroupT& group, [[maybe_unused]] RuntimeDatabaseTaskBuilder& builder, [[maybe_unused]] Args&&... args) {
    if constexpr(GroupArgsArgs<GroupT, Args...>) {
      group.init(std::forward<Args>(args)...);
    }
    if constexpr(GroupTaskArgs<GroupT>) {
      group.init(builder);
    }
    if constexpr(GroupEmptyArgs<GroupT>) {
      group.init();
    }
  }

  template<class GroupT, class Args, size_t... I>
  void initGroupWithArgs(GroupT& group, RuntimeDatabaseTaskBuilder& builder, Args&& args, std::index_sequence<I...>) {
    initGroup(group, builder, std::move(std::get<I>(args))...);
  }

  template<class GroupT, class... Args>
  void initGroupWithArgs(GroupT& group, RuntimeDatabaseTaskBuilder& builder, std::tuple<Args...>&& args) {
    initGroupWithArgs(group, builder, args, std::make_index_sequence<sizeof...(Args)>{});
  }

  //Unit is initialized in two steps due to the different availability of RuntimeDatabaseTaskBuilder vs AppTaskArgs.
  //Either way, both steps are always invoked before the tasks execute
  template<class UnitT, class GroupT, class... Args>
  void initUnitStepOne([[maybe_unused]] UnitT& unit, [[maybe_unused]] RuntimeDatabaseTaskBuilder& t, [[maybe_unused]] GroupT& g, [[maybe_unused]] Args&&... rest) {
    if constexpr(UnitArgsArgs<UnitT, Args...>) {
      unit.init(std::forward<Args>(rest)...);
    }
    if constexpr(UnitTaskArgs<UnitT>) {
      unit.init(t);
    }
    if constexpr(UnitGroupArgs<UnitT, GroupT>) {
      unit.init(g);
    }
  }

  template<class UnitT>
  void initUnitStepTwo([[maybe_unused]] UnitT& unit, [[maybe_unused]] AppTaskArgs& args) {
    if constexpr(UnitLocalArgs<UnitT>) {
      unit.init(args);
    }
    if constexpr(UnitEmptyArgs<UnitT>) {
      unit.init();
    }
  }

  template<class GroupT, UnitEmptyExe Exe>
  void execute(Exe& exe, GroupT&, AppTaskArgs&) {
    exe.execute();
  }

  template<class GroupT, UnitLocalExe Exe>
  void execute(Exe& exe, GroupT&, AppTaskArgs& args) {
    exe.execute(args);
  }

  template<class GroupT, UnitGroupExe<GroupT> Exe>
  void execute(Exe& exe, GroupT& group, AppTaskArgs&) {
    exe.execute(group);
  }

  template<class GroupT, UnitLocalGroup<GroupT> Exe>
  void execute(Exe& exe, GroupT& group, AppTaskArgs& args) {
    exe.execute(args, group);
  }
};

template<class GroupT, class UnitT, class... Args>
requires AnyGroupConstructable<GroupT, Args...> && AnyTaskUnit<UnitT, GroupT>
class TLSTaskImpl : public ITaskImpl {
public:
  struct Worker {
    UnitT unit;
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

  AppTaskMetadata init(RuntimeDatabase& db) final {
    RuntimeDatabaseTaskBuilder builder{ db };
    builder.discard();
    TlsDispatch::initGroupWithArgs(group, builder, std::move(argsTuple));
    for(uint8_t i = 0; i < workerCount; ++i) {
      TlsDispatch::initUnitStepOne(getWorker(i).unit, builder, group);
    }

    //Fill metadata with the result of all of them
    //The rest after first could copy rather than constructing from RDTB but that's annoying to write
    AppTaskWithMetadata meta = std::move(builder).finalize();
    meta.data.name = name;
    if (std::holds_alternative<AppTaskPinning::None>(pinning)) {
      pinning = meta.task.pinning;
    }
    return meta.data;
  }

  void initThreadLocal(AppTaskArgs& args) final {
    assert(args.threadIndex < static_cast<size_t>(workerCount));
    TlsDispatch::initUnitStepTwo(getWorker(args.threadIndex).unit, args);
  }

  void execute(AppTaskArgs& args) final {
    Worker& worker = getWorker(args.threadIndex);
    TlsDispatch::execute(worker.unit, group, args);
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
  GroupT group;
  uint8_t workerCount{};
  std::shared_ptr<AppTaskConfig> config;
  std::vector<std::shared_ptr<AppTaskConfig>> childConfigs;
  AppTaskPinning::Variant pinning;
  std::tuple<Args...> argsTuple;
};

namespace TLSTask {
  template<class UnitT, class GroupT = DefaultTaskGroup>
  requires AnyTaskUnit<UnitT, GroupT> && AnyTaskGroup<GroupT>
  auto create(std::string_view name) {
    return std::make_unique<TLSTaskImpl<GroupT, UnitT>>(name);
  }

  template<class UnitT, class GroupT, class... Args>
  requires AnyTaskUnit<UnitT, GroupT> && AnyTaskGroup<GroupT, Args...>
  auto createWithArgs(std::string_view name, Args&&... args) {
    return std::make_unique<TLSTaskImpl<GroupT, UnitT, Args...>>(name, std::forward<Args>(args)...);
  }
}