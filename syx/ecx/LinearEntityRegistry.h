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
  //Base component all entities have
  struct BaseEntityComponent {
    uint32_t mVersion = 0;
    uint32_t mChunkID = NO_CHUNK_ID;

    //The id if this entity is not in any chunk
    static inline constexpr uint32_t NO_CHUNK_ID = std::numeric_limits<uint32_t>::max();
  };

  struct LinearEntity {
    LinearEntity(uint64_t rawId = 0)
      : mData{ rawId } {
    }

    LinearEntity(uint32_t id, uint32_t version) {
      mData.mParts.mEntityId = id;
      mData.mParts.mVersion = version;
    }

    LinearEntity(const LinearEntity&) = default;
    LinearEntity& operator=(const LinearEntity&) = default;

    template<class... Components>
    static uint32_t buildChunkId() {
      uint32_t current = 0;
      ((current = buildChunkId<Components>(current)), ...);
      return current;
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

    operator bool() const {
      return *this != LinearEntity();
    }

    bool operator==(const LinearEntity& rhs) const {
      return mData.mRawId == rhs.mData.mRawId;
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
        uint32_t mVersion;
      } mParts;
    } mData;
  };

  struct LinearEntityGenerator {
    //Sequential ID generator
    uint32_t mEntityGen = 0;
    //Entity info chunk is partitioned array of entity info with all free ids first. This is one past the end index of that free partition
    //That means if it's 0 there are no free elements
    size_t mFreeListEndIndex = 0;
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

    template<class T>
    const std::vector<std::decay_t<T>>* tryGet() const {
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

    //Swap the order of these two entities by index in the storage
    void swap(const LinearEntity& a, const LinearEntity& b) {
      //Swap entity mapping lookup
      //End is either returned in both or none, so only check A for end
      if(auto [itB, itA] = mEntityMapping.swap(a.mData.mParts.mEntityId, b.mData.mParts.mEntityId); itA != mEntityMapping.end()) {
        //Swap storage
        const size_t newIndexB = itB.value().mPackedId;
        const size_t newIndexA = itA.value().mPackedId;
        for(auto& storage : mComponents) {
          storage.second.swap(newIndexA, newIndexB);
        }
      }
    }

    LinearEntity indexToEntity(size_t index) const {
      if(auto it = mEntityMapping.reverseLookup(index); it != mEntityMapping.end()) {
        return LinearEntity(it.value().mSparseId, {});
      }
      return {};
    }

    //It is assumed that the caller knows this entity version is in the chunk
    size_t entityToIndex(const LinearEntity& entity) const {
      auto it = mEntityMapping.find(entity.mData.mParts.mEntityId);
      return it != mEntityMapping.end() ? static_cast<size_t>(it.value().mPackedId) : std::numeric_limits<size_t>::max();
    }

    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      if(std::vector<std::decay_t<ComponentT>>* components = tryGet<std::decay_t<ComponentT>>()) {
        if(auto it = mEntityMapping.find(entity.mData.mParts.mEntityId); it != mEntityMapping.end()) {
          return &components->at(it.value().mPackedId);
        }
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

    bool erase(const LinearEntity& entity) {
      if(auto it = mEntityMapping.find(entity.mData.mParts.mEntityId); it != mEntityMapping.end()) {
        _eraseAt(it);
        return true;
      }
      return false;
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
    bool migrateEntity(const LinearEntity& entity, EntityChunk& fromChunk, NewComponent&& newComponent) {
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
        return true;
      }
      return false;
    }

    //Migrate an entity to this chunk with as many or less components on the entity
    bool migrateEntity(const LinearEntity& entity, EntityChunk& fromChunk) {
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
        return true;
      }
      return false;
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

    const SparseSet<uint32_t>& getEntityMappings() {
      return mEntityMapping;
    }

    uint32_t getId() const {
      return mChunkId;
    }

  private:
    uint32_t _computeChunkId() const {
      std::optional<uint32_t> result;
      for(const auto& pair : mComponents) {
        result = result ? LinearEntity::buildChunkId(*result, pair.first) : LinearEntity::buildChunkId(pair.first);
      }
      return result.value_or(0);
    }

    void _eraseAt(SparseSet<uint32_t>::Iterator it) {
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

    //Map of component type to vector of those components
    //Each vector should be the same size since all entities have the same set of components
    std::unordered_map<typeId_t<LinearEntity>, TypeErasedContainer<std::vector>> mComponents;
    //Set of entities in this chunk where the packed index is the same as in mComponents
    SparseSet<uint32_t> mEntityMapping;
    uint32_t mChunkId = 0;
  };

 class VersionedEntityChunk {
  public:
    VersionedEntityChunk() = default;
    VersionedEntityChunk(std::shared_ptr<EntityChunk> chunk, std::shared_ptr<EntityChunk> entityInfo, LinearEntityGenerator& entityGenerator)
      : mChunk(std::move(chunk))
      , mEntityInfo(std::move(entityInfo))
      , mChunkID(mChunk->getId())
      , mEntityGenerator(&entityGenerator) {
    }
    VersionedEntityChunk(VersionedEntityChunk&&) = default;
    VersionedEntityChunk(const VersionedEntityChunk&) = default;
    VersionedEntityChunk& operator=(const VersionedEntityChunk&) = default;
    VersionedEntityChunk& operator=(VersionedEntityChunk&&) = default;

    bool operator==(const EntityChunk& rhs) const {
      return mChunk.get() == &rhs;
    }

    bool operator==(const VersionedEntityChunk& rhs) const {
      return mChunk.get() == rhs.mChunk.get();
    }

    operator bool() const {
      return mChunk && mEntityInfo;
    }

    template<class T>
    std::vector<std::decay_t<T>>* tryGet() {
      return mChunk->tryGet<T>();
    }

    template<class T>
    const std::vector<std::decay_t<T>>* tryGet() const {
      return mChunk->tryGet<T>();
    }

    bool hasType(const typeId_t<LinearEntity> type) const {
      return mChunk->hasType(type);
    }

    size_t size() const {
      return mChunk->size();
    }

    LinearEntity indexToEntity(size_t index) const {
      //Look up in chunk which only fills in entityId
      if(LinearEntity result = mChunk->indexToEntity(index); result != LinearEntity{}) {
        //Look up the entity info which again only looks up entityId
        if(const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(result)) {
          //Fill in version
          return LinearEntity(result.mData.mParts.mEntityId, info->mVersion);
        }
      }
      return {};
    }

    //It is assumed that the caller knows this entity version is in the chunk
    size_t entityToIndex(const LinearEntity& entity) const {
      return mChunk->entityToIndex(entity);
    }

    //Unsafe version caller can use if they already know the entity is valid
    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponentUnversioned(const LinearEntity& entity) {
      return mChunk->tryGetComponent<ComponentT>(entity);
    }

    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      return contains(entity) ? mChunk->tryGetComponent<ComponentT>(entity) : nullptr;
    }

    bool contains(const LinearEntity& entity) const {
      return _getInfoIfContains(entity) != nullptr;
    }

    void clearEntities() {
      for(const auto& entity : mChunk->getEntityMappings()) {
        if(BaseEntityComponent* info = _getInfoIfContains(entity.mSparseId)) {
          info->mChunkID = BaseEntityComponent::NO_CHUNK_ID;
          info->mVersion++;
        }
        _addToFreeList(LinearEntity(entity.mSparseId, {}));
      }
      mChunk->clearEntities();
    }

    bool erase(const LinearEntity& entity) {
      if(BaseEntityComponent* info = _getInfoIfContains(entity)) {
        if(mChunk->erase(entity)) {
          info->mChunkID = BaseEntityComponent::NO_CHUNK_ID;
          info->mVersion++;
          _addToFreeList(entity);
          return true;
        }
      }
      return false;
    }

    //Create a copy of this with all the same types in mComponents but none of the entities
    std::shared_ptr<EntityChunk> cloneEmpty() {
      return mChunk->cloneEmpty();
    }

    LinearEntity addDefaultConstructedEntity() {
      //Should always return a valid entity if no existing id is requested
      return _tryAddDefaultConstructedEntity({});
    }

    std::optional<LinearEntity> tryAddDefaultConstructedEntity(const LinearEntity& entity) {
      LinearEntity result = _tryAddDefaultConstructedEntity(entity);
      return result ? std::make_optional(result) : std::nullopt;
    }

    //Migrate an entity from `fromChunk` to this chunk with the new component
    //Caller must ensure that the entity with the new component would have all components in the new chunk
    template<class NewComponent>
    bool migrateEntity(const LinearEntity& entity, VersionedEntityChunk& fromChunk, NewComponent&& newComponent) {
      if(BaseEntityComponent* info = fromChunk._getInfoIfContains(entity); info && mChunk->migrateEntity<NewComponent>(entity, *fromChunk.mChunk, std::forward<NewComponent&&>(newComponent))) {
        info->mChunkID = mChunkID;
        return true;
      }
      return false;
    }

    //Migrate an entity to this chunk with as many or less components on the entity
    bool migrateEntity(const LinearEntity& entity, VersionedEntityChunk& fromChunk) {
      if(BaseEntityComponent* info = fromChunk._getInfoIfContains(entity); info && mChunk->migrateEntity(entity, *fromChunk.mChunk)) {
        info->mChunkID = mChunkID;
        return true;
      }
      return false;
    }

  private:
    const BaseEntityComponent* _getInfoIfContains(const LinearEntity& entity) const {
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      return info && info->mChunkID == mChunkID && info->mVersion == entity.mData.mParts.mVersion ? info : nullptr;
    }

    BaseEntityComponent* _getInfoIfContains(const LinearEntity& entity) {
      BaseEntityComponent* info = mEntityInfo->tryGetComponent<BaseEntityComponent>(entity);
      return info && info->mChunkID == mChunkID && info->mVersion == entity.mData.mParts.mVersion ? info : nullptr;
    }

    LinearEntity _tryAddDefaultConstructedEntity(LinearEntity entity) {
      BaseEntityComponent* info = nullptr;
      //Check validity of requested ID
      if(entity) {
        entity.mData.mParts.mVersion = 0;
        //Only allow requested versions of 0 because otherwise it opens up more confusing edge cases than it's worth
        info = mEntityInfo->tryGetComponent<BaseEntityComponent>(entity);
        //If the entity already exists and is in a chunk then the ID is taken, exit
        if(info && info->mChunkID != BaseEntityComponent::NO_CHUNK_ID) {
          return {};
        }
      }
      else {
        //Try to find an id on the free list first
        entity = _tryPopFromFreeList();
        //If that's not found, generate a new one
        if(!entity) {
          //Generate new ids until one is found that isn't taken
          //It is not necessary to check if this generated id is in the free list because
          //a new id is only generated if the free list is empty
          do {
            entity = LinearEntity(++mEntityGenerator->mEntityGen, 0);
            info = mEntityInfo->tryGetComponent<BaseEntityComponent>(entity);
          }
          while(info && info->mChunkID != BaseEntityComponent::NO_CHUNK_ID);
        }
        else {
          //Try to populate info so all entity id generation approaches result in `info` if it exists
          info = mEntityInfo->tryGetComponent<BaseEntityComponent>(entity);
        }
      }

      //At this point an available entity id has been found and `info` populated if it exists
      //If this is a new entity id, add the info for it
      if(!info) {
        mEntityInfo->addDefaultConstructedEntity(entity);
        info = mEntityInfo->tryGetComponent<BaseEntityComponent>(entity);
      }

      //Now entity id is available and info is generated, update info to point at this chunk
      info->mChunkID = mChunkID;
      //The version of the free list entity was incremented upon freeing, so this is now the new version
      entity.mData.mParts.mVersion = info->mVersion;

      //Now that info and entity are up to date, add the stored component data
      mChunk->addDefaultConstructedEntity(entity);
      return entity;
    }

    //Caller is expected to have incremented the version of the BaseENtityInfo this will be pointing at
    void _addToFreeList(const LinearEntity& entity) {
      //Swap this entity that was not in the free list to one past the end, then increment the end,
      //meaning this entity is the new end of the list
      if(const LinearEntity endEntity = mEntityInfo->indexToEntity(mEntityGenerator->mFreeListEndIndex); entity.mData.mParts.mEntityId != endEntity.mData.mParts.mEntityId) {
        mEntityInfo->swap(entity, endEntity);
      }
      mEntityGenerator->mFreeListEndIndex++;
    }

    LinearEntity _tryPopFromFreeList() {
      if(mEntityGenerator->mFreeListEndIndex > 0) {
        //Move end of list back for element we're about to take
        const size_t newIndex = --(mEntityGenerator->mFreeListEndIndex);
        //Look up version
        const BaseEntityComponent& newInfo = mEntityInfo->tryGet<const BaseEntityComponent>()->at(newIndex);
        //Fill in reverse lookup id and version
        return LinearEntity(mEntityInfo->indexToEntity(newIndex).mData.mParts.mEntityId, newInfo.mVersion);
      }
      return {};
    }

    std::shared_ptr<EntityChunk> mChunk;
    std::shared_ptr<EntityChunk> mEntityInfo;
    LinearEntityGenerator* mEntityGenerator = nullptr;
    //Pulled out of the mChunk to avoid having to access the pointer when not needed
    uint32_t mChunkID = 0;
  };

  template<>
  class EntityRegistry<LinearEntity> {
  public:
    struct EmptyTag {};

    //Iterator for a single component type. Chunks should be iterated over directly for more complex queries
    //For simple single type queries this is fine, although it requires allocations to create
    //Partly only here for parity with basic EntityRegistry type
    //TODO: remove allocations by evaluating chunks during iteration instead of upfront
    template<class T>
    class It {
    public:
      It(std::vector<VersionedEntityChunk> chunks, size_t entityIndex, size_t chunkIndex)
        : mChunks(std::move(chunks))
        , mEntityIndex(entityIndex)
        , mChunkIndex(chunkIndex) {
      }

      It& operator++() {
        ++mEntityIndex;
        if(mEntityIndex >= mChunks[mChunkIndex].size()) {
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
        return mChunks[mChunkIndex].indexToEntity(mEntityIndex);
      }

      T& component() {
        return mChunks[mChunkIndex].tryGet<T>()->at(mEntityIndex);
      }

    private:
      std::vector<VersionedEntityChunk> mChunks;
      size_t mChunkIndex = 0;
      size_t mEntityIndex = 0;
    };

    EntityRegistry() {
      //Container for empty entities
      auto baseChunk = std::make_shared<EntityChunk>();
      baseChunk->addComponentType<EmptyTag>();
      mEntityInfo = std::make_shared<EntityChunk>();
      mEntityInfo->addComponentType<BaseEntityComponent>();
      mChunkTypeToChunks[LinearEntity::buildChunkId<EmptyTag>()] = std::move(baseChunk);

      mSingletonEntity = createEntityWithComponents<SingletonComponent>();
    }

    LinearEntity createEntity() {
      //Empty chunk should always exist
      return _getEmptyChunk().addDefaultConstructedEntity();
    }

    template<class... Components>
    LinearEntity createEntityWithComponents() {
      return *tryCreateEntityWithComponents<Components...>({});
    }

    template<class... Components>
    std::tuple<LinearEntity, std::reference_wrapper<Components>...> createAndGetEntityWithComponents() {
      LinearEntity result = *tryCreateEntityWithComponents<Components...>({});
      VersionedEntityChunk chunk = _getChunk(LinearEntity::buildChunkId<EmptyTag, Components...>());
      assert(chunk && "Chunk should always exist for a new entity");
      size_t index = chunk.entityToIndex(result);
      return std::make_tuple(result, std::ref(chunk.tryGet<Components>()->at(index))...);
    }

    template<class... Components>
    std::optional<LinearEntity> tryCreateEntityWithComponents(const LinearEntity& desiredId) {
      //Need to include the empty tag to end up with the same chunks as when built one by one
      auto chunkId = LinearEntity::buildChunkId<EmptyTag, Components...>();
      VersionedEntityChunk chunk = _getChunk(chunkId);
      if(!chunk) {
        auto newChunk = std::make_shared<EntityChunk>();
        newChunk->addComponentType<EmptyTag>();
        (newChunk->addComponentType<Components>(), ...);
        chunk = _addChunk(std::move(newChunk), chunkId);
      }

      return desiredId ? chunk.tryAddDefaultConstructedEntity(desiredId) : std::make_optional(chunk.addDefaultConstructedEntity());
    }

    std::optional<LinearEntity> tryCreateEntity(const LinearEntity& desiredId) {
      return _getEmptyChunk().tryAddDefaultConstructedEntity(desiredId);
    }

    void destroyEntity(const LinearEntity& entity) {
      if(auto chunk = _tryGetChunkForEntity(entity)) {
        chunk.erase(entity);
      }
    }

    template<class ComponentT, class... Args>
    ComponentT& addComponent(const LinearEntity& entity, Args&&... args) {
      using DecayT = std::decay_t<ComponentT>;
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      if(!info || info->mVersion != entity.mData.mParts.mVersion) {
        assert(false && "Should only add components to valid entities");
        static ComponentT empty;
        return empty;
      }

      const uint32_t oldChunk = info->mChunkID;
      const uint32_t newChunk = LinearEntity::buildChunkId<ComponentT>(oldChunk);
      auto tryGetChunk = [this](uint32_t chunk) {
        auto it = mChunkTypeToChunks.find(chunk);
        return it != mChunkTypeToChunks.end() ? _getVersionedChunk(it->second) : VersionedEntityChunk();
      };

      VersionedEntityChunk fromChunk, toChunk;
      fromChunk = tryGetChunk(oldChunk);
      toChunk = tryGetChunk(newChunk);

      assert(fromChunk && "From chunk should exist");
      const bool alreadyHasType = fromChunk && fromChunk.hasType(ecx::typeId<ComponentT, ecx::LinearEntity>());
      if(alreadyHasType) {
        return *fromChunk.tryGetComponent<ComponentT>(entity);
      }
      //If chunk for this component combination doesn't exist, create it
      if(!toChunk) {
        std::shared_ptr<EntityChunk> cloned = fromChunk.cloneEmpty();
        cloned->addComponentType<DecayT>();

        toChunk = _addChunk(std::move(cloned), newChunk);
      }

      toChunk.migrateEntity(entity, fromChunk, ComponentT{std::forward<Args>(args)...});
      std::vector<DecayT>* components = toChunk.tryGet<DecayT>();
      return components->back();
    }

    template<class ComponentT>
    void removeComponent(const LinearEntity& entity) {
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      if(!info || info->mVersion != entity.mData.mParts.mVersion) {
        assert(false && "Should only remove components from valid entities");
        return;
      }
      const uint32_t oldChunk = info->mChunkID;
      const uint32_t newChunk = LinearEntity::removeFromChunkId<ComponentT>(oldChunk);
      auto tryGetChunk = [this](uint32_t chunk) {
        auto it = mChunkTypeToChunks.find(chunk);
        return it != mChunkTypeToChunks.end() ? _getVersionedChunk(it->second) : VersionedEntityChunk();
      };

      VersionedEntityChunk fromChunk, toChunk;
      fromChunk = tryGetChunk(oldChunk);
      toChunk = tryGetChunk(newChunk);

      const bool hasType = fromChunk && fromChunk.hasType(typeId<ComponentT, LinearEntity>());
      if(!hasType) {
        //It's already gone, return
        return;
      }
      assert(fromChunk && "From chunk should exist");
      //If chunk for this component combination doesn't exist, create it
      if(!toChunk) {
        std::shared_ptr<EntityChunk> cloned = fromChunk.cloneEmpty();
        cloned->removeComponentType<ComponentT>();

        toChunk = _addChunk(std::move(cloned), newChunk);
      }

      toChunk.migrateEntity(entity, fromChunk);
    }

    template<class... Components>
    void removeComponentsFromAllEntities() {
      //Iterate over all non-empty chunks that have the components
      for(const auto& pair : mChunkTypeToChunks) {
        VersionedEntityChunk fromChunk = _getVersionedChunk(pair.second);
        if(!fromChunk.size()) {
          continue;
        }
        const uint32_t fromID = pair.first;
        uint32_t toID = fromID;
        //Remove the components from the chunk ID, which means subtracting them if in the chunk
        ((toID = (fromChunk.hasType(ecx::typeId<Components, ecx::LinearEntity>()) ? LinearEntity::removeFromChunkId<Components>(toID) : toID)), ...);

        //If the chunk ID changed it means it had one of the components to remove
        if(fromID != toID) {
          //Get or create the destination chunk
          VersionedEntityChunk toChunk;
          if(auto foundIt = mChunkTypeToChunks.find(toID); foundIt != mChunkTypeToChunks.end()) {
            toChunk = _getVersionedChunk(foundIt->second);
          }
          else {
            //Create the desired chunk
            auto newChunk = pair.second->cloneEmpty();
            (newChunk->removeComponentType<Components>(), ...);

            toChunk = _addChunk(std::move(newChunk), toID);
          }

          //Migrate all entities now that the destination chunk has been found
          while(fromChunk.size()) {
            toChunk.migrateEntity(fromChunk.indexToEntity(0), fromChunk);
          }
        }
      }
    }

    bool isValid(const LinearEntity& entity) {
      return bool(_tryGetChunkForEntity(entity));
    }

    //Added for convenience but callers should prefer using chunks directly if queries may share chunks
    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      auto chunk = _tryGetChunkForEntity(entity);
      return chunk ? chunk.tryGetComponent<ComponentT>(entity) : nullptr;
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

        results.push_back(_getVersionedChunk(pair.second));
      }
    }

    //TODO: Very inefficient, should either cache queries or prefer direct use of chunks
    template<class ComponentT>
    It<ComponentT> begin() {
      std::vector<VersionedEntityChunk> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      auto foundIt = std::find_if(chunkIds.begin(), chunkIds.end(), [](const VersionedEntityChunk& chunk) {
        return chunk.size() > 0;
      });
      if(foundIt == chunkIds.end()) {
        return end<ComponentT>();
      }
      const size_t chunkIndex = static_cast<size_t>(foundIt - chunkIds.begin());

      return It<ComponentT>(std::move(chunkIds), 0, chunkIndex);
    }

    template<class ComponentT>
    It<ComponentT> end() {
      std::vector<VersionedEntityChunk> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      const size_t chunkCount = chunkIds.size();

      return It<ComponentT>(std::move(chunkIds), 0, chunkCount);
    }

    template<class ComponentT>
    It<ComponentT> find(const LinearEntity& entity) {
      auto foundChunk = _tryGetChunkForEntity(entity);
      if(!foundChunk) {
        return end<ComponentT>();
      }

      std::vector<VersionedEntityChunk> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      auto foundIt = std::find_if(chunkIds.begin(), chunkIds.end(), [&foundChunk](const VersionedEntityChunk& chunk) {
        return chunk == foundChunk;
      });
      if(foundIt == chunkIds.end()) {
        return end<ComponentT>();
      }
      const size_t chunkIndex = static_cast<size_t>(foundIt - chunkIds.begin());
      const size_t entityIndex = foundChunk.entityToIndex(entity);

      return It<ComponentT>(std::move(chunkIds), entityIndex, chunkIndex);
    }

    LinearEntity getSingleton() const {
      return mSingletonEntity;
    }

    size_t size() {
      return mEntityInfo->size();
    }

    template<class ComponentT>
    size_t size() {
      std::vector<VersionedEntityChunk> chunks;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<std::decay_t<ComponentT>, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunks, query, empty, empty);

      return std::accumulate(chunks.begin(), chunks.end(), size_t(0), [](size_t cur, const VersionedEntityChunk& chunk) {
        return cur + chunk.size();
      });
    }

    size_t chunkCount() const {
      return mChunkTypeToChunks.size();
    }

  private:
    VersionedEntityChunk _getEmptyChunk() {
      return _getChunk(LinearEntity::buildChunkId<EmptyTag>());
    }

    VersionedEntityChunk _getChunk(uint32_t chunkId) {
      auto it = mChunkTypeToChunks.find(chunkId);
      return it != mChunkTypeToChunks.end() ? _getVersionedChunk(it->second) : VersionedEntityChunk();
    }

    VersionedEntityChunk _tryGetChunkForEntity(const LinearEntity& entity) {
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      if(!info || info->mVersion != entity.mData.mParts.mVersion) {
        return {};
      }

      auto it = mChunkTypeToChunks.find(info->mChunkID);
      if(it != mChunkTypeToChunks.end()) {
        //Not strictly necessary to check contains due to BaseEntityComponent lookup above but maybe good for safety
        if(it->second->contains(entity)) {
          return _getVersionedChunk(it->second);
        }
      }
      return {};
    }

    VersionedEntityChunk _addChunk(std::shared_ptr<EntityChunk> toAdd, uint32_t chunkId) {
      mChunkTypeToChunks.insert(std::make_pair(chunkId, toAdd));
      return _getVersionedChunk(std::move(toAdd));
    }

    VersionedEntityChunk _getVersionedChunk(std::shared_ptr<EntityChunk> rawChunk) {
      return VersionedEntityChunk(std::move(rawChunk), mEntityInfo, mEntityGenerator);
    }

    std::unordered_map<uint32_t, std::shared_ptr<EntityChunk>> mChunkTypeToChunks;
    std::shared_ptr<EntityChunk> mEntityInfo;
    LinearEntityGenerator mEntityGenerator;
    LinearEntity mSingletonEntity;
  };
}

namespace std {
  template<>
  struct hash<ecx::LinearEntity> {
    size_t operator()(const ecx::LinearEntity& rhs) const {
      return std::hash<uint64_t>()(rhs.mData.mRawId);
    }
  };
}