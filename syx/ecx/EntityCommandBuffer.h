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

    void destroyEntity(const EntityT& entity) {
      static_assert(HasDestroyCapability);
      return mBuffer->destroyEntity(entity);
    }

    template<class Component>
    Component& addComponent(const EntityT& entity) {
      static_assert(AreAllowedTypes<Component>);
      return mBuffer->addComponent<Component>(entity);
    }

    template<class Component>
    void removeComponent(const EntityT& entity) {
      static_assert(AreAllowedTypes<Component>);
      mBuffer->removeComponent<Component>(entity);
    }

  private:
    CommandBuffer<EntityT>* mBuffer = nullptr;
  };

  struct CommandBufferTypes {
    using TypeId = ecx::typeId_t<LinearEntity>;
    std::vector<TypeId> mTypes;
    bool mAllowDestroyEntity = false;
  };

  template<class, bool>
  class RuntimeEntityCommandBuffer {};

  template<bool ENABLE_SAFETY_CHECKS>
  class RuntimeEntityCommandBuffer<LinearEntity, ENABLE_SAFETY_CHECKS> {
  public:
    RuntimeEntityCommandBuffer(CommandBuffer<LinearEntity>& buffer, CommandBufferTypes allowedTypes)
      : mBuffer(&buffer) {
      setAllowedTypes(std::move(allowedTypes));
    }
    RuntimeEntityCommandBuffer(const RuntimeEntityCommandBuffer&) = default;
    RuntimeEntityCommandBuffer& operator=(const RuntimeEntityCommandBuffer&) = default;

    auto createEntity() {
      return std::get<0>(mBuffer->createAndGetEntityWithComponents<>());
    }

    void destroyEntity(const LinearEntity& entity) {
      if constexpr(ENABLE_SAFETY_CHECKS) {
        if(!mAllowedTypes.mAllowDestroyEntity) {
          return;
        }
      }
      return mBuffer->destroyEntity(entity);
    }

    template<class Component>
    Component* addComponent(const LinearEntity& entity) {
      return _isAllowedType<Component>() ? &mBuffer->addComponent<Component>(entity) : nullptr;
    }

    template<class Component>
    void removeComponent(const LinearEntity& entity) {
      if (_isAllowedType<Component>()) {
        mBuffer->removeComponent<Component>(entity);
      }
    }

    const CommandBufferTypes& getAllowedTypes() {
      return mAllowedTypes;
    }

    void setAllowedTypes(CommandBufferTypes types) {
      mAllowedTypes = std::move(types);
      //Sort so binary search is possible, remove duplicates
      std::sort(mAllowedTypes.mTypes.begin(), mAllowedTypes.mTypes.end());
      mAllowedTypes.mTypes.erase(std::unique(mAllowedTypes.mTypes.begin(), mAllowedTypes.mTypes.end()), mAllowedTypes.mTypes.end());
    }

  private:
    template<class T>
    bool _isAllowedType() const {
      if constexpr(ENABLE_SAFETY_CHECKS) {
        return std::lower_bound(mAllowedTypes.mTypes.begin(), mAllowedTypes.mTypes.end(), typeId<std::decay_t<T>, LinearEntity>()) != mAllowedTypes.mTypes.end();
      }
      else {
        return true;
      }
    }

    CommandBuffer<LinearEntity>* mBuffer = nullptr;
    CommandBufferTypes mAllowedTypes;
  };
};