#include <Precompile.h>
#include <time/TimeModule.h>

#include <IAppModule.h>
#include <RuntimeDatabase.h>
#include <TLSTaskImpl.h>
#include <TableName.h>

namespace TimeModule {
  struct ConfigRow : SharedRow<Time::TimeConfig> {};
  struct SimTimeRow : SharedRow<Time::TimeTransform> {};

  //Recomputes time transforms to propagate potential changes to config
  struct UpdateTime {
    void init(RuntimeDatabaseTaskBuilder& task) {
      simTime = task.query<SimTimeRow>().tryGetSingletonElement();
      simTimeConfig = task.query<const ConfigRow>().tryGetSingletonElement();
      assert(simTime && simTimeConfig);
    }

    static void updateTime(Time::TimeTransform& time, const Time::TimeConfig& config) {
      time.timeScale = config.timeScale;
      time.dt = config.targetTicksPerSecond ? 1.f / static_cast<float>(config.targetTicksPerSecond) : 0.f;
      time.dt *= time.timeScale;
    }

    void execute() {
      updateTime(*simTime, *simTimeConfig);
    }

    Time::TimeTransform* simTime{};
    const Time::TimeConfig* simTimeConfig{};
  };

  class ModuleImpl : public IAppModule {
  public:
    void createDatabase(RuntimeDatabaseArgs& args) {
      std::invoke([] {
        StorageTableBuilder table;
        table.addRows<
          ConfigRow,
          SimTimeRow
        >().setTableName({ "Time" });
        return table;
      }).finalize(args);
    }

    void update(IAppBuilder& builder) {
      builder.submitTask(TLSTask::create<UpdateTime>("Time"));
    }
  };

  Time::TimeConfig* getSimTimeConfigMutable(RuntimeDatabaseTaskBuilder& task) {
    return task.query<ConfigRow>().tryGetSingletonElement();
  }

  const Time::TimeTransform* getSimTime(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const SimTimeRow>().tryGetSingletonElement();
  }

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<ModuleImpl>();
  }
}