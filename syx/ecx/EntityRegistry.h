#pragma once

#include "SparseSet.h"
#include "TypeErasedContainer.h"
#include "TypeId.h"

namespace ecx {
  //Registry using EntityT for entity id storage, expected to be some integral type
  template<class EntityT>
  class EntityRegistry {
  public:
    template<class T>
    class It {
    public:
      using value_type = T;
      using difference_type = std::ptrdiff_t;
      using pointer = T*;
      using reference = T&;
      using iterator_category = std::forward_iterator_tag;

      It(typename EntityRegistry<EntityT>::ComponentPool* pool, typename SparseSet<EntityT>::Iterator sparseIt)
        : mPool(pool)
        , mSparseIt(sparseIt) {
      }

      It(const It&) = default;
      It& operator=(const It&) = default;

      It& operator++() {
        ++mSparseIt;
        return *this;
      }

      It& operator++(int) {
        auto result = mSparseIt;
        ++(*this);
        return It(mPool, result);
      }

      bool operator==(const It& rhs) const {
        return mSparseIt == rhs.mSparseIt;
      }

      bool operator!=(const It& rhs) const {
        return !(*this == rhs);
      }

      T& operator*() {
        return component();
      }

      T* operator->() {
        return &component();
      }

      EntityT entity() const {
        return mSparseIt.value().mSparseId;
      }

      T& component() {
        auto slot = static_cast<size_t>(mSparseIt.value().mPackedId);
        return mPool->mComponents->get<DecayType<T>>()->at(slot);
      }

    private:
      typename EntityRegistry<EntityT>::ComponentPool* mPool = nullptr;
      typename SparseSet<EntityT>::Iterator mSparseIt;
    };

    //Decay a provided type to avoid unexpected behavior for const, reference, and pointer
    //There are probably still ways to bypass this and get weird results but this should cover most cases
    template<class T>
    using DecayType = std::remove_pointer_t<std::decay_t<T>>;

    EntityT createEntity() {
      //TODO: free list would be more efficient, this will never re-use old pages
      EntityT result = ++mIdGen;
      mEntities.insert(result);
      return result;
    }

    void destroyEntity(EntityT entity) {
      //Make sure it exists in the first place
      if(auto it = mEntities.find(entity); it != mEntities.end()) {
        //Remove from all component pools
        for(ComponentPool& pool : mComponentPools) {
          _removeComponent(pool, entity);
        }

        //Remove from global list
        mEntities.erase(it);
      }
    }

    bool isValid(EntityT entity) const {
      auto it = mEntities.find(entity);
      return it != mEntities.end();
    }

    //TODO: optimization for zero size components
    template<class ComponentT, class... Args>
    ComponentT& addComponent(EntityT entity, Args&&... args) {
      using CompT = DecayType<ComponentT>;
      assert(mEntities.find(entity) != mEntities.end() && "Entity should exist");

      ComponentPool& pool = _getPool<CompT>();
      const EntityT newSlot = pool.mEntities.insert(entity).mPackedId;
      std::vector<CompT>& storage = *pool.mComponents->get<CompT>();
      CompT& newComponent = _getOrResize(storage, static_cast<size_t>(newSlot));
      newComponent = CompT(std::forward<Args>(args)...);
      return newComponent;
    }

    template<class ComponentT>
    void removeComponent(EntityT entity) {
      _removeComponent(_getPool<ComponentT>(), entity);
    }

    template<class ComponentT>
    DecayType<ComponentT>* tryGetComponent(EntityT entity) {
      ComponentPool& pool = _getPool<ComponentT>();
      if(auto it = pool.mEntities.find(entity); it != pool.mEntities.end()) {
        const auto slot = static_cast<size_t>(it.value().mPackedId);
        return &pool.mComponents->get<DecayType<ComponentT>>()->at(slot);
      }
      return nullptr;
    }

    template<class ComponentT>
    ComponentT& getComponent(EntityT entity) {
      return *tryGetComponent<ComponentT>(entity);
    }

    template<class ComponentT>
    bool hasComponent(EntityT entity) {
      return tryGetComponent<ComponentT>(entity) != nullptr;
    }

    template<class ComponentT>
    It<ComponentT> begin() {
      ComponentPool& pool = _getPool<ComponentT>();
      return It<ComponentT>(&pool, pool.mEntities.begin());
    }

    template<class ComponentT>
    It<ComponentT> end() {
      ComponentPool& pool = _getPool<ComponentT>();
      return It<ComponentT>(&pool, pool.mEntities.end());
    }

    template<class ComponentT>
    It<ComponentT> find(EntityT entity) {
      ComponentPool& pool = _getPool<ComponentT>();
      return It<ComponentT>(&pool, pool.mEntities.find(entity));
    }

    template<class ComponentT>
    size_t size() {
      return _getPool<ComponentT>().mComponents->size();
    }

  private:
    struct ComponentPool {
      std::unique_ptr<TypeErasedContainer<std::vector>> mComponents;
      SparseSet<EntityT> mEntities;
    };

    static void _removeComponent(ComponentPool& pool, const EntityT& entity) {
      if(auto componentIt = pool.mEntities.find(entity); componentIt != pool.mEntities.end()) {
        //Removal will do a swap remove in the sparse set, do the same for the components
        const EntityT prevSlot = componentIt.value().mPackedId;
        if(auto newComponentIt = pool.mEntities.erase(componentIt); newComponentIt != pool.mEntities.end()) {
          pool.mComponents->swap(static_cast<size_t>(prevSlot), pool.mComponents->size() - 1);
        }
        pool.mComponents->pop_back();
      }
    }

    template<class Container>
    static typename Container::reference _getOrResize(Container& container, size_t index) {
      if(container.size() <= index) {
        container.resize(index + 1);
      }
      return container[index];
    }

    template<class T>
    ComponentPool& _getPool() {
      using Type = DecayType<T>;
      auto id = typeId<Type, decltype(*this)>();
      ComponentPool& pool = _getOrResize(mComponentPools, static_cast<size_t>(id));
      if(!pool.mComponents) {
        pool.mComponents = std::make_unique<TypeErasedContainer<std::vector>>(TypeErasedContainer<std::vector>::create<Type>());
      }
      return pool;
    }

    SparseSet<EntityT> mEntities;
    std::vector<ComponentPool> mComponentPools;
    EntityT mIdGen = 0;
  };
}