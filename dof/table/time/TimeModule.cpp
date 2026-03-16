#include <Precompile.h>
#include <time/TimeModule.h>

#include <IAppModule.h>
#include <RuntimeDatabase.h>
#include <TLSTaskImpl.h>
#include <TableName.h>

namespace TimeModule {
  struct TimeAccumulator {
    //Ratio where 1 indicates this tick rate is ready to tick as specified by the given TimeConfig for this part of the simulation.
    math::Ratio32 overflow;
  };

  struct TimeData {
    Time::TimeTransform transform;
    TimeAccumulator simAcc, physAcc, renderAcc;
  };
  struct ConfigRow : SharedRow<Time::TimeConfig> {};
  struct SimTimeRow : SharedRow<TimeData> {};

  //Recomputes time transforms to propagate potential changes to config
  struct UpdateTime {
    void init(RuntimeDatabaseTaskBuilder& task) {
      data = task.query<SimTimeRow>().tryGetSingletonElement();
      simTimeConfig = task.query<const ConfigRow>().tryGetSingletonElement();
      assert(data && simTimeConfig);
    }

    //Advance the given time slice by accumulating dt into the given accumulator and
    //determining if a new tick should be processed.
    //Blend factor is computed for the time slice as progress towards next tick.
    //Rendering overrides this by making the blend factor relative to the most frequent other slice.
    static void updateSlice(
      const math::Ratio32& dt,
      const math::Ratio32& targetRate,
      Time::TimeSlice& slice,
      TimeAccumulator& acc,
      bool enableFreezeTick
    ) {
      if (targetRate.nonzero()) {
        acc.overflow += dt / targetRate;
      }
      //If a full tick is available, subtract it and flag a new tick
      //Only subtract a maximum of one by putting back any beyond that.
      if(int32_t availableTicks = acc.overflow.popWhole()) {
        //Since only one tick is processed at a time, put back any above 1
        acc.overflow += math::Ratio32{ --availableTicks };
        slice.isNewTick = true;
        slice.secondsToTicks = static_cast<float>(targetRate);
      }
      //If no ticks are available, reset the flag, and set multiplier to zero.
      //This means it is valid to either gate logic on the flag or to always multiply by secondsToTicks
      else {
        slice.isNewTick = enableFreezeTick;
        slice.secondsToTicks = 0;
      }

      //If overflow is greater than one then the simulation is behind the target tick rate and will be doing several catch-up ticks.
      //In that case, none of them should interpolate, meaning clamping to 1.
      slice.blendFactor = std::max(1.f, static_cast<float>(acc.overflow));
    }

    void execute() {
      //Assume app is always managing to tick at this rate.
      //TODO: account for frame dips, probably by polling time here
      const math::Ratio32& dt{ simTimeConfig->globalRate };
      updateSlice(dt, simTimeConfig->simRate, data->transform.simulation, data->simAcc, simTimeConfig->enableFreezeTick);
      updateSlice(dt, simTimeConfig->physRate, data->transform.physics, data->physAcc, simTimeConfig->enableFreezeTick);
      updateSlice(dt, simTimeConfig->renderRate, data->transform.rendering, data->renderAcc, simTimeConfig->enableFreezeTick);

      //Blend rendering based on how long between the more frequent of simulation and physics
      data->transform.rendering.blendFactor = simTimeConfig->simRate < simTimeConfig->physRate ? data->transform.simulation.blendFactor : data->transform.physics.blendFactor;
    }

    TimeData* data{};
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

  Time::TimeConfig* getTimeConfigMutable(RuntimeDatabaseTaskBuilder& task) {
    return task.query<ConfigRow>().tryGetSingletonElement();
  }

  const Time::TimeTransform* getTime(RuntimeDatabaseTaskBuilder& task) {
    const TimeData* data = task.query<const SimTimeRow>().tryGetSingletonElement();
    return data ? &data->transform : nullptr;
  }

  const Time::TimeSlice* getTimeSlice(RuntimeDatabaseTaskBuilder& task, Time::TransformSlice slice) {
    return &(getTime(task)->*slice);
  }

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<ModuleImpl>();
  }
}