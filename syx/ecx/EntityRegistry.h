#pragma once

#include "AnyTuple.h"
#include <optional>
#include "RuntimeTraits.h"
#include "SparseSet.h"
#include "TypeErasedContainer.h"
#include "TypeId.h"

namespace ecx {
  template<class EntityT>
  struct StorageInfo {
    typeId_t<EntityT> mType;
    TypeErasedContainer(*mCreateStorage)(void*) = nullptr;
    const IRuntimeTraits* mComponentTraits = nullptr;
    void* mStorageData = nullptr;
  };

  template<class T>
  struct DefaultComponentTraits {
    template<class EntityT>
    static StorageInfo<EntityT> getStorageInfo() {
      StorageInfo<EntityT> info;
      info.mType = typeId<T, EntityT>();
      info.mComponentTraits = &BasicRuntimeTraits<T>::singleton();
      info.mCreateStorage = &createStorage;
      info.mStorageData = nullptr;
      return info;
    }

    static TypeErasedContainer createStorage(void*) {
      return TypeErasedContainer::create<T, std::vector>();
    }
  };

  template<class T>
  struct ComponentTraits : DefaultComponentTraits<T> {};

  struct ComponentPoolTupleTag {};
  using ComponentPoolTuple = AnyTuple<ComponentPoolTupleTag>;
  struct LinearEntity;
  //A component on the singleton entity so that it can be viewed
  struct SingletonComponent {};
  //Registry using EntityT for entity id storage, expected to be some integral type
  template<class EntityT>
  class EntityRegistry {
    struct ComponentPool {
      std::unique_ptr<TypeErasedContainer> mComponents;
      SparseSet<EntityT> mEntities;
      ComponentPoolTuple mSharedComponents;
      StorageInfo<EntityT> mInfo;
    };

  public:
    static_assert(!std::is_same_v<LinearEntity, EntityT>, "LinearEntity uses a specialized registry, check your include order for LinearEntityRegistry.h");

    //Decay a provided type to avoid unexpected behavior for const, reference, and pointer
    //There are probably still ways to bypass this and get weird results but this should cover most cases
    template<class T>
    using DecayType = std::remove_pointer_t<std::decay_t<T>>;

    using TypeIDCategory = EntityT;
    using TypeID = typeId_t<TypeIDCategory>;

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

      size_t packedID() const {
        return mSparseIt.value().mPackedId;
      }

      T& component() {
        auto slot = static_cast<size_t>(mSparseIt.value().mPackedId);
        return mPool->mComponents->get<std::vector<DecayType<T>>>()->at(slot);
      }

    private:
      typename EntityRegistry<EntityT>::ComponentPool* mPool = nullptr;
      typename SparseSet<EntityT>::Iterator mSparseIt;
    };

    class PoolIt {
    public:
      using difference_type = std::ptrdiff_t;
      using iterator_category = std::forward_iterator_tag;

      using InternalIt = typename std::vector<typename EntityRegistry<EntityT>::ComponentPool>::iterator;

      PoolIt(InternalIt pool)
        : mPool(pool) {
      }

      PoolIt(const PoolIt&) = default;
      PoolIt(PoolIt&&) = default;
      PoolIt& operator=(const PoolIt&) = default;
      PoolIt& operator=(PoolIt&&) = default;

      bool operator==(const PoolIt& rhs) const {
        return mPool == rhs.mPool;
      }

      bool operator!=(const PoolIt& rhs) const {
        return !(*this == rhs);
      }

      PoolIt& operator++() {
        ++mPool;
        return *this;
      }

      PoolIt& operator++(int) {
        PoolIt result = *this;
        ++(*this);
        return result;
      }

      size_t size() const {
        return mPool->mEntities.size();
      }

      const SparseSet<EntityT>& getEntities() const {
        return mPool->mEntities;
      }

      ComponentPoolTuple& getSharedComponents() {
        return mPool->mSharedComponents;
      }

      template<class ComponentT, class... Args>
      ComponentT& addComponent(EntityT entity, Args&&... args) {
        using CompT = DecayType<ComponentT>;
        CompT temp{ std::forward<Args>(args)... };
        return *static_cast<ComponentT*>(addComponent(entity, &temp, typename ComponentTraits<CompT>::getStorageInfo<EntityT>()));
      }

      void* addComponent(EntityT entity, void* arg, const StorageInfo<EntityT>& storage) {
        ComponentPool& pool = *mPool;
        const EntityT newSlot = pool.mEntities.insert(entity).mPackedId;
        const size_t poolSize = pool.mComponents->size();
        if(poolSize <= newSlot) {
          //TODO: probably bad for rapid small growths
          pool.mComponents->resize(newSlot + 1);
        }
        void* newComponent = pool.mComponents->at(newSlot);
        if(arg) {
          storage.mComponentTraits->moveAssign(arg, newComponent);
        }
        return newComponent;
      }

      template<class ComponentT>
      void removeComponent(EntityT entity) {
        _removeComponent(*mPool, entity);
      }

      template<class ComponentT>
      DecayType<ComponentT>* tryGetComponent(EntityT entity) {
        //TODO: make this call the runtime version without losing out on type safety check
        ComponentPool& pool = *mPool;
        if(auto it = pool.mEntities.find(entity); it != pool.mEntities.end()) {
          const auto slot = static_cast<size_t>(it.value().mPackedId);
          return &pool.mComponents->get<std::vector<DecayType<ComponentT>>>()->at(slot);
        }
        return nullptr;
      }

      void* tryGetComponent(EntityT entity) {
        ComponentPool& pool = *mPool;
        if(auto it = pool.mEntities.find(entity); it != pool.mEntities.end()) {
          const auto slot = static_cast<size_t>(it.value().mPackedId);
          return pool.mComponents->at(slot);
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
        ComponentPool& pool = *mPool;
        return It<ComponentT>(&pool, pool.mEntities.begin());
      }

      template<class ComponentT>
      It<ComponentT> end() {
        ComponentPool& pool = *mPool;
        return It<ComponentT>(&pool, pool.mEntities.end());
      }

      template<class ComponentT>
      It<ComponentT> find(EntityT entity) {
        ComponentPool& pool = *mPool;
        return It<ComponentT>(&pool, pool.mEntities.find(entity));
      }

      template<class ComponentT>
      size_t size() {
        auto& components = mPool->mComponents;
        return components ? components->size() : size_t(0);
      }

      void clear() {
        if(mPool->mComponents) {
          mPool->mComponents->clear();
        }
        mPool->mEntities.clear();
      }

      const StorageInfo<EntityT> getStorageInfo() const {
        return mPool->mInfo;
      }

      void* at(size_t packedId) {
        return mPool->mComponents->at(packedId);
      }

    private:
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

      InternalIt mPool;
    };

    EntityRegistry() {
      addComponent<SingletonComponent>(mSingletonEntity);
    }

    EntityRegistry(EntityRegistry&&) = default;
    EntityRegistry& operator=(EntityRegistry&&) = default;

    EntityT createEntity() {
      //TODO: free list would be more efficient, this will never re-use old pages
      EntityT result = ++mIdGen;
      mEntities.insert(result);
      return result;
    }

    std::optional<EntityT> tryCreateEntity(const EntityT& desiredId) {
      if(mEntities.find(desiredId) == mEntities.end()) {
        mEntities.insert(desiredId);
        return std::make_optional(desiredId);
      }
      return std::nullopt;
    }

    void destroyEntity(EntityT entity) {
      //Remove all components except nothing, so all of them
      if(auto it = _removeAllComponentsExcept<>(entity); it != mEntities.end()) {
        //Remove from global list
        mEntities.erase(it);
      }
    }

    template<class... Components>
    void removeAllComponentsExcept(const EntityT& entity) {
      _removeAllComponentsExcept<Components...>(entity);
    }

    void clear() {
      mEntities.clear();
      for(auto pool = poolsBegin(); pool != poolsEnd(); ++pool) {
        pool.clear();
      }
    }

    bool isValid(EntityT entity) const {
      auto it = mEntities.find(entity);
      return it != mEntities.end();
    }

    template<class ComponentT, class... Args>
    ComponentT& addComponent(EntityT entity, Args&&... args) {
      using CompT = DecayType<ComponentT>;
      assert(mEntities.find(entity) != mEntities.end() && "Entity should exist");
      return _getPool<CompT>().addComponent<CompT>(entity, std::forward<Args>(args)...);
    }

    void* addRuntimeComponent(EntityT entity, void* arg, const StorageInfo<EntityT>& storage) {
      assert(mEntities.find(entity) != mEntities.end() && "Entity should exist");

      return _getPool(storage).addComponent(entity, arg, storage);
    }

    template<class ComponentT>
    void removeComponent(EntityT entity) {
      _getPool<ComponentT>().removeComponent<ComponentT>(entity);
    }

    template<class ComponentT>
    DecayType<ComponentT>* tryGetComponent(EntityT entity) {
      return _getPool<ComponentT>().tryGetComponent<ComponentT>(entity);
    }

    void* tryGetComponent(EntityT entity, const StorageInfo<EntityT>& storage) {
      return _getPool(storage).tryGetComponent(entity);
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
      auto pool = _tryGetPool<ComponentT>();
      return pool != poolsEnd() ? pool.begin<ComponentT>() : createEmptyIt<ComponentT>();
    }

    template<class ComponentT>
    It<ComponentT> end() {
      auto pool = _tryGetPool<ComponentT>();
      return pool != poolsEnd() ? pool.end<ComponentT>() : createEmptyIt<ComponentT>();
    }

    template<class ComponentT>
    It<ComponentT> find(EntityT entity) {
      auto pool = _tryGetPool<ComponentT>();
      return pool != poolsEnd() ? pool.find<ComponentT>(entity) : createEmptyIt<ComponentT>();
    }

    template<class ComponentT>
    size_t size() {
      auto pool = _tryGetPool<ComponentT>();
      return pool != poolsEnd() ? pool.size<ComponentT>() : size_t(0);
    }

    template<class ComponentT>
    PoolIt findPool() {
      return findPool(typeId<DecayType<ComponentT>, TypeIDCategory>());
    }

    PoolIt findPool(const TypeID& type) {
      size_t index = static_cast<size_t>(type);
      return PoolIt(mComponentPools.size() > index ? mComponentPools.begin() + index : mComponentPools.end());
    }

    template<class ComponentT>
    PoolIt getOrCreatePool() {
      return getOrCreatePool(typename ComponentTraits<DecayType<ComponentT>>::getStorageInfo());
    }

    PoolIt getOrCreatePool(const StorageInfo<EntityT>& storage) {
      size_t index = static_cast<size_t>(storage.mType);
      _getOrResize(mComponentPools, index, storage);
      return { mComponentPools.begin() + index };
    }

    PoolIt poolsBegin() {
      return { mComponentPools.begin() };
    }

    PoolIt poolsEnd() {
      return { mComponentPools.end() };
    }

    size_t size() const {
      return mEntities.size();
    }

    EntityT getSingleton() const {
      return mSingletonEntity;
    }

  private:
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

    template<class... Components>
    auto _removeAllComponentsExcept(const EntityT& entity) {
      auto it = mEntities.find(entity);
      if(it != mEntities.end()) {
        //Remove from all component pools
        for(ComponentPool& pool : mComponentPools) {
          if constexpr(sizeof...(Components) > 0) {
            //If this is an ignored type, skip it
            if(((typeId<std::decay_t<Components>, TypeErasedTag>() == pool.mComponents->type()) || ...)) {
              continue;
            }
          }
          _removeComponent(pool, entity);
        }
      }
      return it;
    }

    //Create an empty iterator for if there is no pool to point at should only need comparison since begin != end should immediately exit
    template<class ComponentT>
    It<ComponentT> createEmptyIt() {
      return { nullptr, mEntities.end() };
    }

    ComponentPool& _getOrResize(std::vector<ComponentPool>& container, size_t index, const StorageInfo<EntityT>& info) {
      if(container.size() <= index) {
        container.resize(index + 1);
        ComponentPool& result = container[index];
        result.mComponents = std::make_unique<TypeErasedContainer>(info.mCreateStorage(info.mStorageData));
        result.mInfo = info;
        return result;
      }
      return container[index];
    }

    template<class T>
    PoolIt _tryGetPool() {
      using Type = DecayType<T>;
      const size_t index = static_cast<size_t>(typeId<Type, TypeIDCategory>());
      return mComponentPools.size() > index ? mComponentPools.begin() + index : mComponentPools.end();
    }

    PoolIt _tryGetPool(const TypeID& type) {
      const size_t index = static_cast<size_t>(type);
      return mComponentPools.size() > index ? mComponentPools.begin() + index : mComponentPools.end();
    }

    template<class T>
    PoolIt _getPool() {
      return _getPool(typename ComponentTraits<DecayType<T>>::getStorageInfo<EntityT>());
    }

    PoolIt _getPool(const StorageInfo<EntityT>& info) {
      ComponentPool& pool = _getOrResize(mComponentPools, static_cast<size_t>(info.mType), info);
      if(!pool.mComponents) {
        pool.mComponents = std::make_unique<TypeErasedContainer>(info.mCreateStorage(info.mStorageData));
      }
      return { mComponentPools.begin() + static_cast<size_t>(info.mType) };
    }

    SparseSet<EntityT> mEntities;
    std::vector<ComponentPool> mComponentPools;
    EntityT mIdGen = 0;
    EntityT mSingletonEntity = createEntity();
  };
}