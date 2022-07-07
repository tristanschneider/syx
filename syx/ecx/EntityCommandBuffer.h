#pragma once

#include "CommandBuffer.h"

namespace ecx {
  struct EntityDestroyTag {};
  //Wrapper around command buffer that ensures access to only the indicated types. Used for system dependencies
  template<class EntityT, class... Components>
  class EntityCommandBuffer {
  public:
    EntityCommandBuffer(CommandBuffer<EntityT>& buffer)
      : mBuffer(&buffer) {
    }
    EntityCommandBuffer(const EntityCommandBuffer&) = default;
    EntityCommandBuffer& operator=(const EntityCommandBuffer&) = default;

    template<class T>
    using IsAllowedTypeT = std::disjunction<std::is_same<std::decay_t<T>, std::decay_t<Components>>...>;
    template<class T>
    constexpr static inline bool IsAllowedType = IsAllowedTypeT<T>::value;
    template<class... Args>
    constexpr static inline bool AreAllowedTypes = std::conjunction_v<IsAllowedTypeT<Args>...>;
    constexpr static inline bool HasDestroyCapability = std::disjunction_v<std::is_same<EntityDestroyTag, Components>...>;

    template<class... Components>
    auto createAndGetEntityWithComponents() {
      static_assert(AreAllowedTypes<Components...>);
      return mBuffer->createAndGetEntityWithComponents<Components...>();
    }

    void destroyEntity(const LinearEntity& entity) {
      static_assert(HasDestroyCapability);
      return mBuffer->destroyEntity(entity);
    }

    template<class Component>
    Component& addComponent(const LinearEntity& entity) {
      static_assert(AreAllowedTypes<Component>);
      return mBuffer->addComponent<Component>(entity);
    }

    template<class Component>
    void removeComponent(const LinearEntity& entity) {
      static_assert(AreAllowedTypes<Component>);
      return mBuffer->removeComponent<Component>(entity);
    }

  private:
    CommandBuffer<EntityT>* mBuffer = nullptr;
  };
};