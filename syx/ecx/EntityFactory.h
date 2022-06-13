#pragma once

#include "LinearEntityRegistry.h"

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
    std::tuple<LinearEntity, std::reference_wrapper<Components>...> createAndGetEntityWithComponents() {
      return mRegistry->createAndGetEntityWithComponents<Components...>();
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

  template<class... Components>
  class ViewedEntityChunk;

  //TODO: this is a hack for now to allow previous uses of EntityFactory to work before full adoption of CommandBuffer
  template<>
  class EntityFactory<LinearEntity> {
  public:
    template<class...>
    friend class ViewedEntityChunk;

    EntityFactory(EntityRegistry<LinearEntity>& registry)
      : mRegistry(&registry) {
    }

    EntityFactory(const EntityFactory&) = default;
    EntityFactory& operator=(const EntityFactory&) = default;

    LinearEntity createEntity() {
      return mRegistry->createEntity(*mRegistry->getDefaultEntityGenerator());
    }

    template<class... Components>
    std::tuple<LinearEntity, std::reference_wrapper<Components>...> createAndGetEntityWithComponents() {
      return mRegistry->createAndGetEntityWithComponents<Components...>(*mRegistry->getDefaultEntityGenerator());
    }

    template<class... Components>
    LinearEntity createEntityWithComponents() {
      return mRegistry->createEntityWithComponents<Components...>(*mRegistry->getDefaultEntityGenerator());
    }

    void destroyEntity(const LinearEntity& entity) {
      mRegistry->destroyEntity(entity, *mRegistry->getDefaultEntityGenerator());
    }

  private:
    EntityRegistry<LinearEntity>* mRegistry = nullptr;
  };
}