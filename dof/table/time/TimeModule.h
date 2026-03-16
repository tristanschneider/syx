#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <generics/Ratio.h>

class IAppModule;
class RuntimeDatabaseTaskBuilder;

namespace Time {
  //Caller can request this via getSimTimeConfigMutable and set either value.
  //The final `TimeTransform` values will be a combination of the target tick rate and time scale.
  struct TimeConfig {
    static inline int32_t DEFAULT_RATE = 60;

    //The rate that the app is ticked at which everything else is based on
    math::Ratio32 globalRate{ 1, DEFAULT_RATE };
    //Target tick rates of the various subsystems updated within the global rate and therefore cannot exceed it
    math::Ratio32 simRate{ 1, DEFAULT_RATE };
    math::Ratio32 physRate{ 1, DEFAULT_RATE };
    math::Ratio32 renderRate{ 1, DEFAULT_RATE };
    //When enabled, this causes TimeSlice::isNewTick to remain true even when time is frozen.
    //SecondsToTicks will be zero meaning nothing meaningfully advances, but this can assist in debugging a "frozen" state.
    bool enableFreezeTick{};
  };

  //Represents the time slice for a given simulation update
  struct TimeSlice {
    //Scalar to convert from x/s to x/t where s = seconds and t = ticks
    float secondsToTicks{};
    //Scalar in range [0,1] indicating the current substeps between the most frequent of simulation or physics updates, whichever is faster:
    //With physics at 1/20 and simulation at 1/10 and rendering at 1/60 the substeps would be based on 1/20, meaning 1/3, 2/3, 3/3.
    float blendFactor{};
    //True when the simulation rate has elapsed since the last tick. Since ticks are invoked at the global rate,
    //anything wanting to run at a different rate should either use the blend factor or gate the logic on isNewTick.
    //Assuming the blend factor would be 0 or 1 would not be reliable in the case of missed frames.
    bool isNewTick{};
  };

  struct TimeTransform;
  using TransformSlice = TimeSlice(TimeTransform::*);

  struct TimeTransform {
    TimeSlice simulation, physics, rendering;
  };

  constexpr TransformSlice physicsSlice() { return &TimeTransform::physics; }
  constexpr TransformSlice simulationSlice() { return &TimeTransform::simulation; }
  constexpr TransformSlice renderingSlice() { return &TimeTransform::rendering; }
}

//Anything in the simulation doing operations over time should use an exposed TimeTransform to ensure they behave as expected when the time scale changes.
//Current implementation uses a fixed simulation rate and only updates based on the simulation scale.
namespace TimeModule {
  Time::TimeConfig* getTimeConfigMutable(RuntimeDatabaseTaskBuilder& task);
  const Time::TimeTransform* getTime(RuntimeDatabaseTaskBuilder& task);
  const Time::TimeSlice* getTimeSlice(RuntimeDatabaseTaskBuilder& task, Time::TransformSlice slice);
  inline const Time::TimeSlice* getSimTime(RuntimeDatabaseTaskBuilder& task) { return getTimeSlice(task, Time::simulationSlice()); }
  inline const Time::TimeSlice* getPhyTime(RuntimeDatabaseTaskBuilder& task) { return getTimeSlice(task, Time::physicsSlice()); }
  inline const Time::TimeSlice* getRenTime(RuntimeDatabaseTaskBuilder& task) { return getTimeSlice(task, Time::renderingSlice()); }

  std::unique_ptr<IAppModule> createModule();
}