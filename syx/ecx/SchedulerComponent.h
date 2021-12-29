#pragma once

#include "Scheduler.h"

//Exposes a way for systems to get access to the SchedulerExecutor
//Systems should view this as const to avoid unnecessary order dependencies
namespace ecx {
  template<class SchedulerT>
  class SchedulerComponent {
  public:
    template<class EntityT, template<class> class Rest>
    static EntityT entityTypeFromScheduler(const Scheduler<EntityT, Rest>&);

    using EntityT = decltype(entityTypeFromScheduler(std::declval<SchedulerT>()));

    using ViewT = View<EntityT, Read<SchedulerComponent<SchedulerT>>>;

    SchedulerComponent() = default;
    SchedulerComponent(std::shared_ptr<SchedulerT> scheduler)
      : mScheduler(std::move(scheduler)) {
    }

    SchedulerComponent(SchedulerComponent&&) = default;
    SchedulerComponent& operator=(SchedulerComponent&&) = default;

    SchedulerExecutor<SchedulerT> createExecutor() const {
      return create::schedulerExecutor(*mScheduler);
    }

    //Caller is responsible for only calling this if the scheduler component exists in the registry
    template<class EntityT, class... Args>
    static SchedulerExecutor<SchedulerT> createExecutorFromContext(SystemContext<EntityT, Args...>& context) {
      return (*context.get<View<EntityT, Read<SchedulerComponent<SchedulerT>>>>().begin()).get<const SchedulerComponent<SchedulerT>>().createExecutor();
    }

  private:
    //Mutable to allow systems to view this as const even though they're queueing work
    //This is to prevent the scheduler from adding order dependencies for accessing this component
    mutable std::shared_ptr<SchedulerT> mScheduler;
  };

  template<class EntityT, class SchedulerT>
  static void addSingletonSchedulerComponent(EntityRegistry<EntityT>& registry, std::shared_ptr<SchedulerT> scheduler) {
    registry.addComponent<SchedulerComponent<SchedulerT>>(registry.getSingleton(), std::move(scheduler));
  }
}