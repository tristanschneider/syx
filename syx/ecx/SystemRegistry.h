#pragma once

#include "EntityRegistry.h"
#include "System.h"

namespace ecx {
  template<class EntityT>
  class SystemRegistry {
  public:
    void registerSystem(std::unique_ptr<ISystem<EntityT>> system) {
      mSystems.push_back(std::move(system));
    }

    void tick(EntityRegistry<EntityT>& registry) const {
      for(auto&& system : mSystems) {
        system->tick(registry);
      }
    }

  private:
    std::vector<std::unique_ptr<ISystem<EntityT>>> mSystems;
  };
};