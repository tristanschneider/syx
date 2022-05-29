#pragma once

#include "EntityRegistry.h"
#include "System.h"

namespace ecx {
  template<class EntityT>
  class SystemRegistry {
  public:
    void registerSystem(std::shared_ptr<ISystem<EntityT>> system) {
      mSystems.push_back(std::move(system));
    }

    void tick(EntityRegistry<EntityT>& registry) {
      for(auto&& system : mSystems) {
        system->tick(registry, mLocalContext);
      }
    }

  private:
    std::vector<std::shared_ptr<ISystem<EntityT>>> mSystems;
    ThreadLocalContext mLocalContext;
  };
};