#pragma once

#include <chrono>
#include <functional>
#include <memory>

class IAppModule;
class RuntimeDatabaseTaskBuilder;

namespace Time {
  struct TimeTransform {
    //Scalar to transform value per second to value per tick with timescale included: m/s -> m/t
    constexpr float getSecondsToTicks() const {
      return dt;
    }

    //Scalar to transform value per tick to value per tick with timescale included
    constexpr float getScaledTicks() const {
      return timeScale;
    }

    template<class T>
    constexpr T secondsToTicks(const T& v) const {
      return v * getSecondsToTicks();
    }

    template<class T>
    constexpr T valueInTicks(const T& v) const {
      return v * getScaledTicks();
    }

    float timeScale{ 1.f };
    float dt{};
  };

  struct TimeConfig {
    uint32_t targetTicksPerSecond{ 60 };
    float timeScale{ 1.f };
  };
}

//Anything in the simulation doing operations over time should use an exposed TimeTransform to ensure they behave as expected when the time scale changes.
//Current implementation uses a fixed simulation rate and only updates based on the simulation scale.
namespace TimeModule {
  Time::TimeConfig* getSimTimeConfigMutable(RuntimeDatabaseTaskBuilder& task);
  const Time::TimeTransform* getSimTime(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IAppModule> createModule();
}