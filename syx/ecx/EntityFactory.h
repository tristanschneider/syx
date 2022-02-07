#pragma once

#include "EntityRegistry.h"

namespace ecx {
  //Wrapper around creation and destruction of entities in an entity registry
  //Intended to be used with system contexts who don't have direct access to registries
  template<class EntityT>
  class EntityFactory {
  public:
    EntityFactory(EntityRegistry<EntityT>& registry)
      : mRegistry(&registry) {
    }

    EntityFactory(const EntityFactory&) = default;
    EntityFactory& operator=(const EntityFactory&) = default;

    EntityT createEntity() {
      return mRegistry->createEntity();
    }

    std::optional<EntityT> tryCreateEntity(const EntityT& entity) {
      return mRegistry->tryCreateEntity(entity);
    }

    template<class... Components>
    std::optional<EntityT> tryCreateEntityWithComponents(const EntityT& entity) {
      return mRegistry->tryCreateEntityWithComponents<Components...>(entity);
    }

    template<class... Components>
    EntityT createEntityWithComponents() {
      return mRegistry->createEntityWithComponents<Components...>();
    }

    void destroyEntity(const EntityT& entity) {
      mRegistry->destroyEntity(entity);
    }

  private:
    EntityRegistry<EntityT>* mRegistry = nullptr;
  };
}