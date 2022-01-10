#pragma once

#include "EntityRegistry.h"

namespace ecx {
  template<class EntityT, class... Components>
  class EntityModifier {
  public:
    using EntityType = EntityT;
    using AllowedComponents = std::tuple<Components...>;

    EntityModifier(EntityRegistry<EntityT>& registry)
      : mRegistry(&registry) {
    }

    EntityModifier(const EntityModifier&) = default;
    EntityModifier& operator=(const EntityModifier&) = default;

    template<class Component, class... Args>
    Component& addComponent(const EntityT& entity, Args&&... args) {
      _assertAllowedComponent<Component>();
      return mRegistry->addComponent<Component>(entity, std::forward<Args>(args)...);
    }

    template<class Component>
    Component& addDeducedComponent(const EntityT& entity, Component&& component) {
      return addComponent<Component>(entity, std::move(component));
    }

    template<class Component>
    Component& getOrAddComponent(const EntityT& entity) {
      _assertAllowedComponent<Component>();
      Component* result = mRegistry->tryGetComponent<Component>(entity);
      if(!result) {
        result = &mRegistry->addComponent<Component>(entity);
      }
      return *result;
    }

    template<class Component>
    void removeComponent(const EntityT& entity) {
      _assertAllowedComponent<Component>();
      mRegistry->removeComponent<Component>(entity);
    }

  private:
    template<class T>
    void _assertAllowedComponent() const {
      static_assert(std::disjunction_v<std::is_same<T, Components>...>, "Modifier can only be used to modify specified components()");
    }

    EntityRegistry<EntityT>* mRegistry = nullptr;
  };
}