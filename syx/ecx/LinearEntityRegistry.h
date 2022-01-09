#pragma once

#include "EntityRegistry.h"
#include <numeric>
#include <optional>
#include <shared_mutex>

//This type of registry stores components in chunks by entity type, where an entity's type is determined
//by the combination of components it has. This allows all components within a chunk to be contiguous in
//memory. This allows for more efficient iteration at the cost of more expensive addition and removal of
//components, since changing the components on an entity causes them to be moved to a different chunk.
namespace ecx {
  struct LinearEntity {
    LinearEntity(uint64_t rawId = 0)
      : mData{ rawId } {
    }

    LinearEntity(uint32_t id, uint32_t type) {
      mData.mParts.mEntityId = id;
      mData.mParts.mChunkId = type;
    }

    LinearEntity(const LinearEntity&) = default;
    LinearEntity& operator=(const LinearEntity&) = default;

    template<class ComponentT>
    static uint32_t buildChunkId() {
      return buildChunkId<ComponentT>(0);
    }

    template<class ComponentT>
    static uint32_t buildChunkId(uint32_t current) {
      return buildChunkId(current, typeId<std::decay_t<ComponentT>, LinearEntity>());
    }

    static uint32_t buildChunkId(const typeId_t<LinearEntity>& type) {
      return buildChunkId(0, type);
    }

    static uint32_t buildChunkId(uint32_t current, const typeId_t<LinearEntity>& type) {
      const uint32_t result = current ^ static_cast<uint32_t>(type);
      assert(!current || result != current && "Chunk ID must have changed or ID generation method needs to be updated");
      return result;
    }

    template<class ComponentT>
    static uint32_t removeFromChunkId(uint32_t current) {
      return buildChunkId<ComponentT>(current);
    }

    bool operator==(const LinearEntity& rhs) const {
      //Entity id is the actual identity, chunk id helps in look ups but doesn't need to be compared
      return mData.mParts.mEntityId == rhs.mData.mParts.mEntityId;
    }

    bool operator!=(const LinearEntity& rhs) const {
      return !(*this == rhs);
    }

    bool operator<(const LinearEntity& rhs) const {
      return mData.mRawId < rhs.mData.mRawId;
    }

    union {
      uint64_t mRawId;
      struct {
        uint32_t mEntityId;
        uint32_t mChunkId;
      } mParts;
    } mData;
  };

  template<>
  struct typeIdCategoryTraits<LinearEntity> {
    inline static size_t idGen = 0;

    template<class T>
    static size_t getId() {
      //Hash to spread out type ids so buildChunkId doesn't collide
      static const size_t result = std::hash<size_t>()(idGen++);
      return result;
    }
  };

  class EntityChunk {
  public:
    template<class T>
    std::vector<std::decay_t<T>>* tryGet() {
      if(auto it = mComponents.find(typeId<std::decay_t<T>, LinearEntity>()); it != mComponents.end()) {
        return it->second.get<std::decay_t<T>>();
      }
      return nullptr;
    }

    bool hasType(const typeId_t<LinearEntity> type) const {
      return mComponents.find(type) != mComponents.end();
    }

    size_t size() const {
      return mEntityMapping.size();
    }

    LinearEntity indexToEntity(size_t index) const {
      auto it = mEntityMapping.reverseLookup(index);
      return it != mEntityMapping.end() ? LinearEntity(it.value().mSparseId, mChunkId) : LinearEntity(0, 0);
    }

    size_t entityToIndex(const LinearEntity& entity) const {
      auto it = mEntityMapping.find(entity.mData.mParts.mEntityId);
      return it != mEntityMapping.end() ? static_cast<size_t>(it.value().mPackedId) : std::numeric_limits<size_t>::max();
    }

    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      if(std::vector<std::decay_t<ComponentT>>* components = tryGet<std::decay_t<ComponentT>>()) {
        auto it = mEntityMapping.find(entity.mData.mParts.mEntityId);
        return it != mEntityMapping.end() ? &components->at(it.value().mPackedId) : nullptr;
      }
      return nullptr;
    }

    bool contains(const LinearEntity& entity) {
      return mEntityMapping.find(entity.mData.mParts.mEntityId) != mEntityMapping.end();
    }

    void clearEntities() {
      mEntityMapping.clear();
      for(auto& pair : mComponents) {
        pair.second.clear();
      }
    }

    void erase(const LinearEntity& entity) {
      if(auto it = mEntityMapping.find(entity.mData.mParts.mEntityId); it != mEntityMapping.end()) {
        //This swap removes the entity, do the same with component storage below
        mEntityMapping.erase(it);

        for(auto& pair : mComponents) {
          TypeErasedContainer<std::vector>& container = pair.second;
          const size_t indexToRemove = it.value().mPackedId;
          //Swap remove
          const size_t lastIndex = container.size() - 1;
          if(indexToRemove != lastIndex) {
            container.swap(indexToRemove, lastIndex);
          }
          container.pop_back();
        }
      }
    }

    //Create a copy of this with all the same types in mComponents but none of the entities
    std::shared_ptr<EntityChunk> cloneEmpty() {
      auto result = std::make_shared<EntityChunk>();
      result->mComponents.reserve(mComponents.size());
      for(const auto& pair : mComponents) {
        result->mComponents.insert(std::make_pair(pair.first, pair.second.createEmptyCopy()));
      }
      return result;
    }

    void addDefaultConstructedEntity(const LinearEntity& entity) {
      //Add entity mapping, packed ID corresponds to size of mComponents containers
      mEntityMapping.insert(entity.mData.mParts.mEntityId);
      for(auto& pair : mComponents) {
        //Default construct all component types, their index is the same as the packed index from mEntityMapping
        pair.second.push_back();
      }
    }

    //Migrate an entity from `fromChunk` to this chunk with the new component
    //Caller must ensure that the entity with the new component would have all components in the new chunk
    template<class NewComponent>
    void migrateEntity(const LinearEntity& entity, EntityChunk& fromChunk, NewComponent&& newComponent) {
      assert(fromChunk.mComponents.size() + 1 == mComponents.size() && "Entity should only migrate if it would have all compnents");

      if(auto it = fromChunk.mEntityMapping.find(entity.mData.mParts.mEntityId); it != fromChunk.mEntityMapping.end()) {
        const size_t fromComponentIndex = it.value().mPackedId;

        //Erase the entity from the previous chunk, this will do a swap remove, which is mirrored in the component storage below
        fromChunk.mEntityMapping.erase(it);
        mEntityMapping.insert(entity.mData.mParts.mEntityId);

        //For each component type, swap remove from `fromChunk` and copy it to `toChunk`
        for(auto& pair : fromChunk.mComponents) {
          if(auto newIt = mComponents.find(pair.first); newIt != mComponents.end()) {
            TypeErasedContainer<std::vector>& fromContainer = pair.second;
            //Move component to end of destination container
            fromContainer.moveIntoFromIndex(fromComponentIndex, newIt->second);
            //Swap remove moved element
            fromContainer.swap(fromComponentIndex, fromContainer.size() - 1);
            fromContainer.pop_back();
          }
          else {
            assert(false && "New chunk should have all of the component types of the previous chunk");
          }
        }

        //Add the new type
        using NewDecayT = std::decay_t<NewComponent>;
        if(auto newType = mComponents.find(typeId<NewDecayT, LinearEntity>()); newType != mComponents.end()) {
          std::vector<NewDecayT>* container = newType->second.get<NewDecayT>();
          container->push_back(std::move(newComponent));
        }
        else {
          assert(false && "Caller should enshure that new component exists");
        }
      }
    }

    //Migrate an entity to this chunk with as many or less components on the entity
    void migrateEntity(const LinearEntity& entity, EntityChunk& fromChunk) {
      assert(fromChunk.mComponents.size() >= mComponents.size() && "Entity should only migrate if it would have all compnents");

      if(auto it = fromChunk.mEntityMapping.find(entity.mData.mParts.mEntityId); it != fromChunk.mEntityMapping.end()) {
        const size_t fromComponentIndex = it.value().mPackedId;

        //Erase the entity from the previous chunk, this will do a swap remove, which is mirrored in the component storage below
        fromChunk.mEntityMapping.erase(it);
        mEntityMapping.insert(entity.mData.mParts.mEntityId);

        //For each component type, swap remove from `fromChunk` and copy it to `toChunk`
        for(auto& fromPair : fromChunk.mComponents) {
          TypeErasedContainer<std::vector>& fromContainer = fromPair.second;
          if(auto toIt = mComponents.find(fromPair.first); toIt != mComponents.end()) {
            //Move component to end of destination container
            fromContainer.moveIntoFromIndex(fromComponentIndex, toIt->second);
          }
          //Even if this type is not in the new chunk but the value still needs to be swap removed in the old chunk
          fromContainer.swap(fromComponentIndex, fromContainer.size() - 1);
          fromContainer.pop_back();
        }
      }
    }

    //Add this component type to the chunk, should only be used while initially creating a chunk,
    //once it's in use it shouldn't be used as it could result in not all entities in a chunk having all components
    template<class ComponentT>
    void addComponentType() {
      assert(mEntityMapping.empty() && "Component types should only be added during creation of a chunk");
      using DecayT = std::decay_t<ComponentT>;
      mComponents.insert(std::make_pair(typeId<DecayT, LinearEntity>(), TypeErasedContainer<std::vector>::create<DecayT>()));
      mChunkId = _computeChunkId();
    }

    template<class ComponentT>
    void removeComponentType() {
      assert(mEntityMapping.empty() && "Component types should only be removed during creation of a chunk");
      if(auto it = mComponents.find(typeId<std::decay_t<ComponentT>, LinearEntity>()); it != mComponents.end()) {
        mComponents.erase(it);
      }
      mChunkId = _computeChunkId();
    }

    template<class Callback>
    void foreachType(const Callback& callback) {
      for(const auto& pair : mComponents) {
        callback(pair.first);
      }
    }

  private:
    uint32_t _computeChunkId() const {
      std::optional<uint32_t> result;
      for(const auto& pair : mComponents) {
        result = result ? LinearEntity::buildChunkId(*result, pair.first) : LinearEntity::buildChunkId(pair.first);
      }
      return result.value_or(0);
    }

    //Map of component type to vector of those components
    //Each vector should be the same size since all entities have the same set of components
    std::unordered_map<typeId_t<LinearEntity>, TypeErasedContainer<std::vector>> mComponents;
    //Set of entities in this chunk where the packed index is the same as in mComponents
    SparseSet<uint32_t> mEntityMapping;
    uint32_t mChunkId = 0;
  };

  template<>
  class EntityRegistry<LinearEntity> {
  public:
    //Fake component type to serve as component for default entity chunk for componentless entities
    struct EmptyTag {};

    //Iterator for a single component type. Chunks should be iterated over directly for more complex queries
    //For simple single type queries this is fine, although it requires allocations to create
    //Partly only here for parity with basic EntityRegistry type
    //TODO: remove allocations by evaluating chunks during iteration instead of upfront
    template<class T>
    class It {
    public:
      It(std::vector<std::shared_ptr<EntityChunk>> chunks, size_t entityIndex, size_t chunkIndex)
        : mChunks(std::move(chunks))
        , mEntityIndex(entityIndex)
        , mChunkIndex(chunkIndex) {
      }

      It& operator++() {
        ++mEntityIndex;
        if(mEntityIndex >= mChunks[mChunkIndex]->size()) {
          ++mChunkIndex;
          mEntityIndex = 0;
        }
        return *this;
      }

      It& operator++(int) {
        auto result = mSparseIt;
        ++(*this);
        return It(mPool, result);
      }

      bool operator==(const It& rhs) const {
        //Not entirely accurate for incompatible iterator types but checking that would be expensive
        return mChunkIndex == rhs.mChunkIndex && mEntityIndex == rhs.mEntityIndex && mChunks.size() == rhs.mChunks.size();
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

      LinearEntity entity() const {
        return mChunks[mChunkIndex]->indexToEntity(mEntityIndex);
      }

      T& component() {
        return mChunks[mChunkIndex]->tryGet<T>()->at(mEntityIndex);
      }

    private:
      std::vector<std::shared_ptr<EntityChunk>> mChunks;
      size_t mChunkIndex = 0;
      size_t mEntityIndex = 0;
    };

    EntityRegistry() {
      //Container for empty entities
      auto emptyChunk = std::make_shared<EntityChunk>();
      emptyChunk->addComponentType<EmptyTag>();
      mChunkTypeToChunks[LinearEntity::buildChunkId<EmptyTag>()] = std::move(emptyChunk);

      mSingletonEntity = createEntity();
      addComponent<SingletonComponent>(mSingletonEntity);
    }

    LinearEntity createEntity() {
      LinearEntity result(++mIDGen, LinearEntity::buildChunkId<EmptyTag>());
      //Empty chunk should always exist
      _getEmptyChunk()->addDefaultConstructedEntity(result);
      return result;
    }

    void destroyEntity(const LinearEntity& entity) {
      if(auto chunk = _tryGetChunkForEntity(entity).second) {
        chunk->erase(entity);
      }
    }

    template<class ComponentT, class... Args>
    ComponentT& addComponent(LinearEntity entity, Args&&... args) {
      using DecayT = std::decay_t<ComponentT>;
      entity = updateEntity(entity);
      const uint32_t oldChunk = entity.mData.mParts.mChunkId;
      const uint32_t newChunk = LinearEntity::buildChunkId<ComponentT>(oldChunk);
      auto tryGetChunk = [this](uint32_t chunk) -> std::shared_ptr<EntityChunk> {
        auto it = mChunkTypeToChunks.find(chunk);
        return it != mChunkTypeToChunks.end() ? it->second : nullptr;
      };

      std::shared_ptr<EntityChunk> fromChunk, toChunk;
      {
        std::shared_lock<std::shared_mutex> lock(mChunkMutex);
        fromChunk = tryGetChunk(oldChunk);
        toChunk = tryGetChunk(newChunk);
      }

      assert(fromChunk && "From chunk should exist");
      //If chunk for this component combination doesn't exist, create it
      if(!toChunk) {
        std::shared_ptr<EntityChunk> cloned = fromChunk->cloneEmpty();
        cloned->addComponentType<DecayT>();

        toChunk = _addChunk(std::move(cloned), newChunk);
      }

      toChunk->migrateEntity(entity, *fromChunk, ComponentT(std::forward<Args>(args)...));
      std::vector<DecayT>* components = toChunk->tryGet<DecayT>();
      return components->back();
    }

    template<class ComponentT>
    void removeComponent(LinearEntity entity) {
      entity = updateEntity(entity);
      const uint32_t oldChunk = entity.mData.mParts.mChunkId;
      const uint32_t newChunk = LinearEntity::removeFromChunkId<ComponentT>(oldChunk);
      auto tryGetChunk = [this](uint32_t chunk) -> std::shared_ptr<EntityChunk> {
        auto it = mChunkTypeToChunks.find(chunk);
        return it != mChunkTypeToChunks.end() ? it->second : nullptr;
      };

      std::shared_ptr<EntityChunk> fromChunk, toChunk;
      {
        std::shared_lock<std::shared_mutex> lock(mChunkMutex);
        fromChunk = tryGetChunk(oldChunk);
        toChunk = tryGetChunk(newChunk);
      }

      assert(fromChunk && "From chunk should exist");
      //If chunk for this component combination doesn't exist, create it
      if(!toChunk) {
        std::shared_ptr<EntityChunk> cloned = fromChunk->cloneEmpty();
        cloned->removeComponentType<ComponentT>();

        toChunk = _addChunk(std::move(cloned), newChunk);
      }

      toChunk->migrateEntity(entity, *fromChunk);
    }

    EntityChunk* tryGetChunkForEntity(const LinearEntity& entity) {
      return _tryGetChunkForEntity(entity).second.get();
    }

    bool isValid(const LinearEntity& entity) {
      return _tryGetChunkForEntity(entity).second != nullptr;
    }

    //Added for convenience but callers should prefer using chunks directly if queries may share chunks
    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      auto chunk = _tryGetChunkForEntity(entity).second;
      return chunk ? chunk->tryGetComponent<ComponentT>(entity) : nullptr;
    }

    template<class ComponentT>
    ComponentT& getComponent(const LinearEntity& entity) {
      return *tryGetComponent<ComponentT>(entity);
    }

    template<class ComponentT>
    bool hasComponent(const LinearEntity& entity) {
      return tryGetComponent<ComponentT>(entity) != nullptr;
    }

    //ResultStore is a container of std::shared_ptr<EntityChunk>, the other containers are of typeId<LinearEntity>
    template<class ResultStore, class IncludeT, class ExcludeT, class OptionalT>
    void getAllChunksSatisfyingConditions(ResultStore& results, const IncludeT& includes, const ExcludeT& excludes, const OptionalT& optionals) {
      std::shared_lock<std::shared_mutex> lock(mChunkMutex);
      for(auto& pair : mChunkTypeToChunks) {
        const std::shared_ptr<EntityChunk>& chunk = pair.second;
        auto chunkHasType = [&chunk](const typeId_t<LinearEntity>& include) {
          return chunk->hasType(include);
        };

        if(!std::all_of(includes.begin(), includes.end(), chunkHasType)) {
          continue;
        }
        if(std::any_of(excludes.begin(), excludes.end(), chunkHasType)) {
          continue;
        }
        //Optionals normally don't matter as long as include/exclude are satisfied, but if view is only optionals, only include entities that satisfy at least one of the optionals
        if(includes.empty() && excludes.empty() && !optionals.empty()) {
          if(std::none_of(includes.begin(), includes.end(), chunkHasType)) {
            continue;
          }
        }

        results.push_back(pair.second);
      }
    }

    //TODO: Very inefficient, should either cache queries or prefer direct use of chunks
    template<class ComponentT>
    It<ComponentT> begin() {
      std::vector<std::shared_ptr<EntityChunk>> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      auto foundIt = std::find_if(chunkIds.begin(), chunkIds.end(), [](const std::shared_ptr<EntityChunk>& chunk) {
        return chunk->size() > 0;
      });
      if(foundIt == chunkIds.end()) {
        return end<ComponentT>();
      }
      const size_t chunkIndex = static_cast<size_t>(foundIt - chunkIds.begin());

      return It<ComponentT>(std::move(chunkIds), 0, chunkIndex);
    }

    template<class ComponentT>
    It<ComponentT> end() {
      std::vector<std::shared_ptr<EntityChunk>> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      const size_t chunkCount = chunkIds.size();

      return It<ComponentT>(std::move(chunkIds), 0, chunkCount);
    }

    template<class ComponentT>
    It<ComponentT> find(const LinearEntity& entity) {
      auto foundChunk = tryGetChunkForEntity(entity);
      if(!foundChunk) {
        return end<ComponentT>();
      }

      std::vector<std::shared_ptr<EntityChunk>> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      auto foundIt = std::find_if(chunkIds.begin(), chunkIds.end(), [&foundChunk](const std::shared_ptr<EntityChunk>& chunk) {
        return chunk.get() == foundChunk;
      });
      if(foundIt == chunkIds.end()) {
        return end<ComponentT>();
      }
      const size_t chunkIndex = static_cast<size_t>(foundIt - chunkIds.begin());
      const size_t entityIndex = foundChunk->entityToIndex(entity);

      return It<ComponentT>(std::move(chunkIds), entityIndex, chunkIndex);
    }

    size_t entityTypeCount() const {
      return mChunkTypeToChunks.size();
    }

    LinearEntity getSingleton() const {
      return mSingletonEntity;
    }

    //A hack to get around unstable entity ids when adding and removign components
    //TODO: what's a better way to avoid this problem?
    LinearEntity updateEntity(const LinearEntity& entity) {
      auto chunk = _tryGetChunkForEntity(entity);
      return chunk.second ? LinearEntity(entity.mData.mParts.mEntityId, chunk.first) : LinearEntity(0);
    }

    size_t size() {
      std::shared_lock<std::shared_mutex> lock(mChunkMutex);
      return std::accumulate(mChunkTypeToChunks.begin(), mChunkTypeToChunks.end(), size_t(0), [](size_t cur, const auto& pair) {
        return cur + pair.second->size();
      });
    }

    template<class ComponentT>
    size_t size() {
      std::vector<std::shared_ptr<EntityChunk>> chunks;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<std::decay_t<ComponentT>, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunks, query, empty, empty);

      return std::accumulate(chunks.begin(), chunks.end(), size_t(0), [](size_t cur, const std::shared_ptr<EntityChunk>& chunk) {
        return cur + chunk->size();
      });
    }

    size_t chunkCount() const {
      return mChunkTypeToChunks.size();
    }

  private:
    std::shared_ptr<EntityChunk> _getEmptyChunk() {
      return mChunkTypeToChunks.at(LinearEntity::buildChunkId<EmptyTag>());
    }

    std::pair<uint32_t, std::shared_ptr<EntityChunk>> _tryGetChunkForEntity(const LinearEntity& entity) {
      std::shared_lock<std::shared_mutex> lock(mChunkMutex);
      auto it = mChunkTypeToChunks.find(entity.mData.mParts.mChunkId);
      if(it != mChunkTypeToChunks.end()) {
        if(it->second->contains(entity)) {
          return std::make_pair(it->first, it->second);
        }
      }
      //If the entity type got out of date, try searching all chunks
      for(auto& pair : mChunkTypeToChunks) {
        if(pair.second->contains(entity)) {
          return pair;
        }
      }
      //If it's in neither of the above it's an invalid entity
      return {};
    }

    std::shared_ptr<EntityChunk> _addChunk(std::shared_ptr<EntityChunk> toAdd, uint32_t chunkId) {
      //Upgrade to unique lock because container size needs to change
      std::unique_lock<std::shared_mutex> uniqueLock(mChunkMutex);
      //Make sure the chunk wasn't recreated while acquiring the lock
      if(auto found = mChunkTypeToChunks.find(chunkId); found != mChunkTypeToChunks.end()) {
        return found->second;
      }

      toAdd->foreachType([this, &toAdd](const typeId_t<LinearEntity>& componentType) {
        mComponentTypeToChunks.insert(std::make_pair(static_cast<uint32_t>(componentType), toAdd));
      });

      mChunkTypeToChunks.insert(std::make_pair(chunkId, toAdd));
      return toAdd;
    }

    //TODO: might not need this
    std::unordered_multimap<uint32_t, std::shared_ptr<EntityChunk>> mComponentTypeToChunks;
    std::unordered_map<uint32_t, std::shared_ptr<EntityChunk>> mChunkTypeToChunks;
    //Scheduler is responsible for ensuring components of the same type won't be created at the same time
    //This is only needed to ensure that the container isn't growing from unrelated types while another is reading
    //In this way modifying a chunk is fine with shared access, but not adding a chunk
    std::shared_mutex mChunkMutex;
    uint32_t mIDGen = 0;
    LinearEntity mSingletonEntity;
  };
}