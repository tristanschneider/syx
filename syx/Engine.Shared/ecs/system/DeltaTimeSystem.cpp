#include "Precompile.h"
#include "ecs/system/DeltaTimeSystem.h"

#include "ecs/component/DeltaTimeComponent.h"

namespace DeltaTimeSystemImpl {
  struct PrevTimeComponent {
    using ClockT = std::chrono::steady_clock;
    using TimePointT = std::chrono::time_point<ClockT>;

    TimePointT mPrevTime = ClockT::now();
  };

  using namespace Engine;
  void tickInit(SystemContext<EntityFactory>& context) {
    context.get<EntityFactory>().createEntityWithComponents<DeltaTimeComponent, PrevTimeComponent>();
  }

  using UpdateView = View<Write<DeltaTimeComponent>, Write<PrevTimeComponent>>;
  void tick(SystemContext<UpdateView>& context) {
    for(auto entity : context.get<UpdateView>()) {
      PrevTimeComponent& prev = entity.get<PrevTimeComponent>();
      //Update previous time and get duration
      auto now = PrevTimeComponent::ClockT::now();
      auto elapsed = now - prev.mPrevTime;
      prev.mPrevTime = now;

      //Update DeltaTimeComponent with new duration
      constexpr float msToS = 1.f / 1000.f;
      entity.get<DeltaTimeComponent>().mSeconds = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) * msToS;
    }
  }
}

std::shared_ptr<Engine::System> DeltaTimeSystem::init() {
  return ecx::makeSystem("InitDT", &DeltaTimeSystemImpl::tickInit);
}

std::shared_ptr<Engine::System> DeltaTimeSystem::update() {
  return ecx::makeSystem("UpdateDT", &DeltaTimeSystemImpl::tick);
}
