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
  template<class T>
  struct DefaultComponentTraits {
    static TypeErasedContainer createStorage() {
      return TypeErasedContainer::create<T, std::vector>();
    }
  };

  template<class T>
  struct ComponentTraits : DefaultComponentTraits<T> {};

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

    explicit operator bool() const {
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

  //Generator for entity ids that can independently generate unique ids without directly coordinating with other generators
  class IndependentEntityGenerator {
  public:
    constexpr static inline size_t SLOT_BITS = 5;
    constexpr static inline uint32_t SLOT_MASK = uint32_t(1 << SLOT_BITS) - 1;
    static inline size_t MAX_SLOTS = size_t(1 << SLOT_BITS);

    IndependentEntityGenerator(uint16_t slot)
      : mSlot(slot) {
    }

    IndependentEntityGenerator(const IndependentEntityGenerator&) = delete;

    //Get or create new id
    LinearEntity tryPopFromFreeList() {
      if(!mFreeList.empty()) {
        LinearEntity result(mFreeList.back());
        mFreeList.pop_back();
        return result;
      }
      return {};
    }

    LinearEntity getOrCreateId() {
      LinearEntity result = tryPopFromFreeList();
      return result != LinearEntity() ? result : generateNewId();
    }

    LinearEntity generateNewId() {
      //Generate a new id and shift it into the id portion
      uint32_t id = ++mNewId;
      id = id << SLOT_BITS;
      //Add the slot to the lower portion
      id |= static_cast<uint32_t>(mSlot);
      return LinearEntity(id, 0);
    }

    //Add an id to the free list
    void pushToFreeList(const LinearEntity& entity) {
      mFreeList.push_back(entity);
    }

    uint16_t getSlot() const {
      return mSlot;
    }

  private:
    uint32_t mNewId = 0;
    uint16_t mSlot = 0;
    std::vector<LinearEntity> mFreeList;
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
      TypeErasedContainer* result = tryGetContainer(typeId<std::decay_t<T>, LinearEntity>());
      return result ? result->get<std::vector<T>>() : nullptr;
    }

    template<class T>
    const std::vector<std::decay_t<T>>* tryGet() const {
      const TypeErasedContainer* result = tryGetContainer(typeId<std::decay_t<T>, LinearEntity>());
      return result ? result->get<std::vector<T>>() : nullptr;
    }

    TypeErasedContainer* tryGetContainer(const typeId_t<LinearEntity>& type) {
      if(auto it = mComponents.find(type); it != mComponents.end()) {
        return &it->second;
      }
      return nullptr;
    }

    const TypeErasedContainer* tryGetContainer(const typeId_t<LinearEntity>& type) const {
      if(auto it = mComponents.find(type); it != mComponents.end()) {
        return &it->second;
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
      using ResultT = std::decay_t<ComponentT>;
      return static_cast<ResultT*>(tryGetComponent(entity, typeId<ResultT, LinearEntity>()));
    }

    void* tryGetComponent(const LinearEntity& entity, const typeId_t<LinearEntity>& type) {
      if(TypeErasedContainer* components = tryGetContainer(type)) {
        if(auto it = mEntityMapping.find(entity.mData.mParts.mEntityId); it != mEntityMapping.end()) {
          return components->at(it.value().mPackedId);
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
    bool migrateEntity(const LinearEntity& entity, EntityChunk& fromChunk, const typeId_t<LinearEntity>& newTypeId) {
      assert(fromChunk.mComponents.size() + 1 == mComponents.size() && "Entity should only migrate if it would have all compnents");

      if(auto it = fromChunk.mEntityMapping.find(entity.mData.mParts.mEntityId); it != fromChunk.mEntityMapping.end()) {
        const size_t fromComponentIndex = it.value().mPackedId;

        //Erase the entity from the previous chunk, this will do a swap remove, which is mirrored in the component storage below
        fromChunk.mEntityMapping.erase(it);
        mEntityMapping.insert(entity.mData.mParts.mEntityId);

        //For each component type, swap remove from `fromChunk` and copy it to `toChunk`
        for(auto& pair : fromChunk.mComponents) {
          if(auto newIt = mComponents.find(pair.first); newIt != mComponents.end()) {
            TypeErasedContainer& fromContainer = pair.second;
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
        if(auto newType = mComponents.find(newTypeId); newType != mComponents.end()) {
          newType->second.push_back();
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
          TypeErasedContainer& fromContainer = fromPair.second;
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
      using DecayT = std::decay_t<ComponentT>;
      addComponentType(typeId<DecayT, LinearEntity>(), TypeErasedContainer::create<DecayT, std::vector>());
    }

    void addComponentType(const typeId_t<LinearEntity>& id, TypeErasedContainer container) {
      assert(mEntityMapping.empty() && "Component types should only be added during creation of a chunk");
      mComponents.insert(std::make_pair(id, std::move(container)));
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
          TypeErasedContainer& container = pair.second;
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
    std::unordered_map<typeId_t<LinearEntity>, TypeErasedContainer> mComponents;
    //Set of entities in this chunk where the packed index is the same as in mComponents
    SparseSet<uint32_t> mEntityMapping;
    uint32_t mChunkId = 0;
  };

 class VersionedEntityChunk {
  public:
    VersionedEntityChunk() = default;
    VersionedEntityChunk(std::shared_ptr<EntityChunk> chunk, std::shared_ptr<EntityChunk> entityInfo)
      : mChunk(std::move(chunk))
      , mEntityInfo(std::move(entityInfo))
      , mChunkID(mChunk->getId()) {
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

    TypeErasedContainer* tryGetContainer(const typeId_t<LinearEntity>& type) {
      return mChunk->tryGetContainer(type);
    }

    const TypeErasedContainer* tryGetContainer(const typeId_t<LinearEntity>& type) const {
      return mChunk->tryGetContainer(type);
    }

    bool hasType(const typeId_t<LinearEntity> type) const {
      return mChunk->hasType(type);
    }

    uint32_t chunkID() const {
      return mChunkID;
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

    void* tryGetComponent(const LinearEntity& entity, const typeId_t<LinearEntity>& type) {
      return contains(entity) ? mChunk->tryGetComponent(entity, type) : nullptr;
    }

    bool contains(const LinearEntity& entity) const {
      return _getInfoIfContains(entity) != nullptr;
    }

    void clearEntities(IndependentEntityGenerator& generator) {
      for(const auto& entity : mChunk->getEntityMappings()) {
        if(BaseEntityComponent* info = _getInfoIfContains(entity.mSparseId)) {
          info->mChunkID = BaseEntityComponent::NO_CHUNK_ID;
          info->mVersion++;
          generator.pushToFreeList(LinearEntity(entity.mSparseId, info->mVersion));
        }
      }
      mChunk->clearEntities();
    }

    bool erase(const LinearEntity& entity, IndependentEntityGenerator& generator) {
      if(BaseEntityComponent* info = _getInfoIfContains(entity)) {
        if(mChunk->erase(entity)) {
          info->mChunkID = BaseEntityComponent::NO_CHUNK_ID;
          info->mVersion++;
          generator.pushToFreeList(LinearEntity(entity.mData.mParts.mEntityId, info->mVersion));
          return true;
        }
      }
      return false;
    }

    //Create a copy of this with all the same types in mComponents but none of the entities
    std::shared_ptr<EntityChunk> cloneEmpty() {
      return mChunk->cloneEmpty();
    }

    LinearEntity addDefaultConstructedEntity(IndependentEntityGenerator& generator) {
      //Should always return a valid entity if no existing id is requested
      return _tryAddDefaultConstructedEntity({}, generator);
    }

    std::optional<LinearEntity> tryAddDefaultConstructedEntity(const LinearEntity& entity, IndependentEntityGenerator& generator) {
      LinearEntity result = _tryAddDefaultConstructedEntity(entity, generator);
      return result ? std::make_optional(result) : std::nullopt;
    }

    //Migrate an entity from `fromChunk` to this chunk with the new component
    //Caller must ensure that the entity with the new component would have all components in the new chunk
    bool migrateEntity(const LinearEntity& entity, VersionedEntityChunk& fromChunk, const typeId_t<LinearEntity>& newType) {
      if(BaseEntityComponent* info = fromChunk._getInfoIfContains(entity); info && mChunk->migrateEntity(entity, *fromChunk.mChunk, newType)) {
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

    LinearEntity _tryAddDefaultConstructedEntity(LinearEntity entity, IndependentEntityGenerator& generator) {
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
        entity = generator.tryPopFromFreeList();
        //If that's not found, generate a new one
        if(!entity) {
          //Generate a new id. It shouldn't be taken due to the slotted approach of the generators
          //It is not necessary to check if this generated id is in the free list because
          //a new id is only generated if the free list is empty
          entity = generator.generateNewId();
          assert(!mEntityInfo->tryGetComponent<BaseEntityComponent>(entity) && "Generated ids should always be unique");
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

    std::shared_ptr<EntityChunk> mChunk;
    std::shared_ptr<EntityChunk> mEntityInfo;
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
        return *static_cast<T*>(mChunks[mChunkIndex].tryGetContainer(typeId<T, LinearEntity>())->at(mEntityIndex));
      }

    private:
      std::vector<VersionedEntityChunk> mChunks;
      size_t mChunkIndex = 0;
      size_t mEntityIndex = 0;
    };

    class ChunkIterator {
    public:
      friend class EntityRegistry<LinearEntity>;

      using difference_type = std::ptrdiff_t;
      using iterator_category = std::forward_iterator_tag;

      using InternalIt = typename std::unordered_map<uint32_t, std::shared_ptr<EntityChunk>>::iterator;

      ChunkIterator(InternalIt chunk, EntityRegistry<LinearEntity>& registry)
        : mChunk(chunk)
        , mRegistry(&registry) {
      }

      ChunkIterator(const ChunkIterator&) = default;
      ChunkIterator(ChunkIterator&&) = default;
      ChunkIterator& operator=(const ChunkIterator&) = default;
      ChunkIterator& operator=(ChunkIterator&&) = default;

      bool operator==(const ChunkIterator& rhs) const {
        return mChunk == rhs.mChunk;
      }

      bool operator!=(const ChunkIterator& rhs) const {
        return !(*this == rhs);
      }

      ChunkIterator& operator++() {
        ++mChunk;
        return *this;
      }

      ChunkIterator& operator++(int) {
        ChunkIterator result = *this;
        ++(*this);
        return result;
      }

      uint32_t chunkID() const {
        return mChunk->first;
      }

      template<class... Components>
      bool hasComponents() {
        return (mChunk->second->hasType(typeId<std::decay_t<Components>, LinearEntity>()) && ...);
      }

      std::optional<LinearEntity> tryAddDefaultConstructedEntity(const LinearEntity& entity, IndependentEntityGenerator& generator) {
        auto chunk = _getVersionedChunk();
        return chunk.tryAddDefaultConstructedEntity(entity, generator);
      }

      LinearEntity addDefaultConstructedEntity(IndependentEntityGenerator& generator) {
        auto chunk = _getVersionedChunk();
        return chunk.addDefaultConstructedEntity(generator);
      }

      void erase(const LinearEntity& entity, IndependentEntityGenerator& generator) {
        auto chunk = _getVersionedChunk();
        chunk.erase(entity, generator);
      }

      size_t size() const {
        return mChunk->second->size();
      }

      template<class T>
      std::vector<std::decay_t<T>>* tryGet() {
        return mChunk->second->tryGet<T>();
      }

      template<class T>
      const std::vector<std::decay_t<T>>* tryGet() const {
        return mChunk->second->tryGet<T>();
      }

      LinearEntity indexToEntity(size_t index) {
        auto chunk = _getVersionedChunk();
        return chunk.indexToEntity(index);
      }

      //It is assumed that the caller knows this entity version is in the chunk
      size_t entityToIndex(const LinearEntity& entity) const {
        return mChunk->second->entityToIndex(entity);
      }

      //Unsafe version caller can use if they already know the entity is valid
      template<class ComponentT>
      std::decay_t<ComponentT>* tryGetComponentUnversioned(const LinearEntity& entity) {
        return mChunk->second->tryGetComponent<ComponentT>(entity);
      }

      template<class ComponentT>
      std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
        auto chunk = _getVersionedChunk();
        return chunk.tryGetComponent<ComponentT>(entity);
      }

      bool contains(const LinearEntity& entity) {
        auto chunk = _getVersionedChunk();
        return chunk.contains(entity);
      }

    private:
      VersionedEntityChunk _getVersionedChunk() {
        return { mChunk->second, mRegistry->mEntityInfo };
      }

      InternalIt mChunk;
      EntityRegistry<LinearEntity>* mRegistry = nullptr;
    };

    EntityRegistry() {
      //Container for empty entities
      auto baseChunk = std::make_shared<EntityChunk>();
      baseChunk->addComponentType<EmptyTag>();
      mEntityInfo = std::make_shared<EntityChunk>();
      mEntityInfo->addComponentType<BaseEntityComponent>();
      mChunkTypeToChunks[LinearEntity::buildChunkId<EmptyTag>()] = std::move(baseChunk);

      //Create a "default" entity generator and use that to create the singleton
      //TODO: a bit of a waste
      mSingletonEntity = createEntityWithComponents<SingletonComponent>(*createEntityGenerator());
    }

    std::shared_ptr<IndependentEntityGenerator> createEntityGenerator() {
      auto slot = static_cast<uint16_t>(mGenerators.size());
      assert(size_t(slot) < IndependentEntityGenerator::MAX_SLOTS);
      auto result = std::make_shared<IndependentEntityGenerator>(static_cast<uint16_t>(mGenerators.size()));
      mGenerators.push_back(result);
      return result;
    }

    //A bit of a convenience hack for testing and uses from before the generator existed
    std::shared_ptr<IndependentEntityGenerator> getDefaultEntityGenerator() {
      return mGenerators[0];
    }

    LinearEntity createEntity(IndependentEntityGenerator& generator) {
      //Empty chunk should always exist
      return _getEmptyChunk().addDefaultConstructedEntity(generator);
    }

    template<class... Components>
    LinearEntity createEntityWithComponents(IndependentEntityGenerator& generator) {
      return *tryCreateEntityWithComponents<Components...>({}, generator);
    }

    template<class... Components>
    std::tuple<LinearEntity, std::reference_wrapper<Components>...> createAndGetEntityWithComponents(IndependentEntityGenerator& generator) {
      LinearEntity result = *tryCreateEntityWithComponents<Components...>({}, generator);
      ChunkIterator chunk = findChunk(LinearEntity::buildChunkId<EmptyTag, Components...>());
      assert(chunk != endChunks() && "Chunk should always exist for a new entity");
      size_t index = chunk.entityToIndex(result);
      return std::make_tuple(result, std::ref(chunk.tryGet<Components>()->at(index))...);
    }

    void destroyEntity(const LinearEntity& entity, IndependentEntityGenerator& generator) {
      if(auto chunk = findChunkFromEntity(entity); chunk != endChunks()) {
        chunk.erase(entity, generator);
      }
    }

    template<class ComponentT, class... Args>
    ComponentT& addComponent(const LinearEntity& entity, Args&&... args) {
      using DecayT = std::decay_t<ComponentT>;
      std::pair<bool, void*> result = addRuntimeComponent<&ComponentTraits<DecayT>::createStorage>(entity, typeId<DecayT, LinearEntity>());
      static DecayT empty;
      assert(result.second && "result should be valid");
      if(result.second) {
        ComponentT* r = static_cast<ComponentT*>(result.second);
        //Emplace through a different code path would be a bit faster but more complicated than it's worth
        //Construct if it was newly created
        if(result.first) {
          *r = ComponentT{std::forward<Args>(args)...};
        }
        return *r;
      }
      return empty;
    }

    //Bool indicates if it was newly added or not
    template<auto CreateStorage, std::enable_if_t<std::is_same_v<TypeErasedContainer, decltype(CreateStorage())>, void*> = nullptr>
    std::pair<bool, void*> addRuntimeComponent(const LinearEntity& entity, const typeId_t<LinearEntity>& type) {
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      if(!info || info->mVersion != entity.mData.mParts.mVersion) {
        assert(false && "Should only add components to valid entities");
        return std::pair<bool, void*>(false, nullptr);
      }

      const uint32_t oldChunk = info->mChunkID;
      const uint32_t newChunk = LinearEntity::buildChunkId(oldChunk, type);
      auto tryGetChunk = [this](uint32_t chunk) {
        auto it = mChunkTypeToChunks.find(chunk);
        return it != mChunkTypeToChunks.end() ? _getVersionedChunk(it->second) : VersionedEntityChunk();
      };

      VersionedEntityChunk fromChunk, toChunk;
      fromChunk = tryGetChunk(oldChunk);
      toChunk = tryGetChunk(newChunk);

      assert(fromChunk && "From chunk should exist");
      const bool alreadyHasType = fromChunk && fromChunk.hasType(type);
      if(alreadyHasType) {
        return std::make_pair(false, fromChunk.tryGetComponent(entity, type));
      }
      //If chunk for this component combination doesn't exist, create it
      if(!toChunk) {
        std::shared_ptr<EntityChunk> cloned = fromChunk.cloneEmpty();
        cloned->addComponentType(type, CreateStorage());

        toChunk = _addChunk(std::move(cloned), newChunk);
      }

      toChunk.migrateEntity(entity, fromChunk, type);
      TypeErasedContainer* components = toChunk.tryGetContainer(type);
      const size_t componentsSize = components->size();
      void* result = componentsSize > 0 ? components->at(componentsSize - 1) : nullptr;
      return std::make_pair(true, result);
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
      return findChunkFromEntity(entity) != endChunks();
    }

    //Added for convenience but callers should prefer using chunks directly if queries may share chunks
    template<class ComponentT>
    std::decay_t<ComponentT>* tryGetComponent(const LinearEntity& entity) {
      auto chunk = findChunkFromEntity(entity);
      return chunk != endChunks() ? chunk.tryGetComponent<ComponentT>(entity) : nullptr;
    }

    template<class ComponentT>
    ComponentT& getComponent(const LinearEntity& entity) {
      return *tryGetComponent<ComponentT>(entity);
    }

    template<class ComponentT>
    bool hasComponent(const LinearEntity& entity) {
      return tryGetComponent<ComponentT>(entity) != nullptr;
    }

    ChunkIterator beginChunks() {
      return { mChunkTypeToChunks.begin(), *this };
    }

    ChunkIterator findChunk(uint32_t chunkId) {
      return { mChunkTypeToChunks.find(chunkId), *this };
    }

    ChunkIterator findChunkFromEntity(const LinearEntity& entity) {
      const BaseEntityComponent* info = mEntityInfo->tryGetComponent<const BaseEntityComponent>(entity);
      if(!info || info->mVersion != entity.mData.mParts.mVersion) {
        return endChunks();
      }

      auto chunk = findChunk(info->mChunkID);
      //Not strictly necessary to check contains due to BaseEntityComponent lookup above but maybe good for safety
      return chunk.contains(entity) ? chunk : endChunks();
    }

    ChunkIterator endChunks() {
      return { mChunkTypeToChunks.end(), *this };
    }

    template<class... Components>
    ChunkIterator getOrCreateChunk() {
      //Need to include the empty tag to end up with the same chunks as when built one by one
      auto chunkId = LinearEntity::buildChunkId<EmptyTag, Components...>();
      if(ChunkIterator chunk = findChunk(chunkId); chunk != endChunks()) {
        return chunk;
      }

      auto newChunk = std::make_shared<EntityChunk>();
      newChunk->addComponentType<EmptyTag>();
      (newChunk->addComponentType<Components>(), ...);
      return { mChunkTypeToChunks.insert(std::make_pair(chunkId, newChunk)).first, *this };
    }

    //Get the chunk that has all the same components as the provided one plus ComponentT
    template<class ComponentT>
    ChunkIterator getOrCreateChunkAddedComponent(ChunkIterator chunk) {
      assert(!chunk.hasComponents<ComponentT>());
      const uint32_t newId = LinearEntity::buildChunkId<ComponentT>(chunk.chunkID());
      if(auto found = findChunk(newId); found != endChunks()) {
        assert(found.hasComponents<ComponentT>());
        return found;
      }

      auto newChunk = chunk.mChunk->second->cloneEmpty();
      newChunk->addComponentType<ComponentT>();
      return { mChunkTypeToChunks.insert(std::make_pair(newId, newChunk)).first, *this };
    }

    //Get the chunk that has all the same components as the provided one minus ComponentT
    template<class ComponentT>
    ChunkIterator getOrCreateChunkRemovedComponent(ChunkIterator chunk) {
      assert(chunk.hasComponents<ComponentT>());
      const uint32_t newId = LinearEntity::buildChunkId<ComponentT>(chunk.chunkID());
      if(auto found = findChunk(newId); found != endChunks()) {
        assert(!found.hasComponents<ComponentT>());
        return found;
      }

      auto newChunk = chunk.mChunk->second->cloneEmpty();
      newChunk->removeComponentType<ComponentT>();
      return { mChunkTypeToChunks.insert(std::make_pair(newId, newChunk)).first, *this };
    }

    //TODO: can go somewhere else and use the exposed iterators
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
      auto foundChunk = findChunkFromEntity(entity);
      if(foundChunk == endChunks()) {
        return end<ComponentT>();
      }

      std::vector<VersionedEntityChunk> chunkIds;
      std::array<typeId_t<LinearEntity>, 0> empty;
      std::array<typeId_t<LinearEntity>, 1> query{ typeId<ComponentT, LinearEntity>() };

      getAllChunksSatisfyingConditions(chunkIds, query, empty, empty);
      auto foundIt = std::find_if(chunkIds.begin(), chunkIds.end(), [&foundChunk](const VersionedEntityChunk& chunk) {
        return chunk.chunkID() == foundChunk.chunkID();
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
    template<class... Components>
    std::optional<LinearEntity> tryCreateEntityWithComponents(const LinearEntity& desiredId, IndependentEntityGenerator& generator) {
      ChunkIterator chunk = getOrCreateChunk<Components...>();
      return desiredId ? chunk.tryAddDefaultConstructedEntity(desiredId, generator) : std::make_optional(chunk.addDefaultConstructedEntity(generator));
    }

    std::optional<LinearEntity> tryCreateEntity(const LinearEntity& desiredId, IndependentEntityGenerator& generator) {
      return _getEmptyChunk().tryAddDefaultConstructedEntity(desiredId, generator);
    }

    ChunkIterator _getEmptyChunk() {
      return findChunk(LinearEntity::buildChunkId<EmptyTag>());
    }

    VersionedEntityChunk _addChunk(std::shared_ptr<EntityChunk> toAdd, uint32_t chunkId) {
      mChunkTypeToChunks.insert(std::make_pair(chunkId, toAdd));
      return _getVersionedChunk(std::move(toAdd));
    }

    VersionedEntityChunk _getVersionedChunk(std::shared_ptr<EntityChunk> rawChunk) {
      return VersionedEntityChunk(std::move(rawChunk), mEntityInfo);
    }

    std::unordered_map<uint32_t, std::shared_ptr<EntityChunk>> mChunkTypeToChunks;
    std::vector<std::shared_ptr<IndependentEntityGenerator>> mGenerators;
    std::shared_ptr<EntityChunk> mEntityInfo;
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