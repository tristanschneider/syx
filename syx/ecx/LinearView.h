#pragma once

#include "LinearEntityRegistry.h"
#include "View.h"

namespace ecx {
  template<class EntityT>
  class EntityFactory;

  //This is unforgiving when it comes to const, a write type cannot be accessed with const T
  template<class... Components>
  struct ViewIsAllowedType {
    template<class T>
    using type = std::disjunction<std::is_same<T, Components>...>;
  };

  template<class... Components>
  class ViewedEntityChunk {
  public:
    template<class T>
    using IsAllowedType = typename ViewIsAllowedType<Components...>::type<T>;

    ViewedEntityChunk(VersionedEntityChunk chunk)
      : mChunk(std::move(chunk)) {
    }
    ViewedEntityChunk(const ViewedEntityChunk&) = default;
    ViewedEntityChunk& operator=(const ViewedEntityChunk&) = default;

    template<class T>
    std::conditional_t<std::is_const_v<T>, const std::vector<std::decay_t<T>>*, std::vector<T>*> tryGet() {
      static_assert(IsAllowedType<T>::value);
      return mChunk.tryGet<T>();
    }

    size_t size() const {
      return mChunk.size();
    }

    LinearEntity indexToEntity(size_t index) const {
      return mChunk.indexToEntity(index);
    }

    size_t entityToIndex(const LinearEntity& entity) const {
      return mChunk.entityToIndex(entity);
    }

    template<class ComponentT>
    ComponentT* tryGetComponent(const LinearEntity& entity) {
      static_assert(IsAllowedType<ComponentT>::value);
      return mChunk.tryGetComponent<ComponentT>(entity);
    }

    bool contains(const LinearEntity& entity) {
      return mChunk.contains(entity);
    }

    //Take the factory to force the system to indicate that it will be destroying entities
    void clear(EntityFactory<LinearEntity>& factory) {
      //TODO: replace with version that goes through the command buffer
      mChunk.clearEntities(*factory.mRegistry->getDefaultEntityGenerator());
    }

  private:
    VersionedEntityChunk mChunk = nullptr;
  };

  //A wrapper around access for a particular entity during iteration within a View
  template<class... Components>
  class ViewedEntity<LinearEntity, Components...> {
  public:
    ViewedEntity(VersionedEntityChunk chunk, size_t index)
      : mContainers{ chunk.tryGet<std::decay_t<Components>>()... }
      , mChunk(std::move(chunk))
      , mIndex(index) {
    }

    template<class T>
    using IsAllowedType = typename ViewIsAllowedType<Components...>::type<T>;

    template<class Component>
    Component* tryGet() {
      static_assert(IsAllowedType<Component>::value);
      auto container = tryGetContainerForComponent<Component>();
      return container ? &container->at(mIndex) : nullptr;
    }

    template<class Component>
    Component& get() {
      static_assert(IsAllowedType<Component>::value);
      return *tryGet<Component>();
    }

    template<class Component>
    bool hasComponent() {
      return tryGet<Component>() != nullptr;
    }

    LinearEntity entity() {
      return mChunk.indexToEntity(mIndex);
    }

  private:
    template<class T>
    auto tryGetContainerForComponent() {
      return std::get<std::vector<std::decay_t<T>>*>(mContainers);
    }

    //Tuple of vector of all viewable components
    std::tuple<std::vector<std::decay_t<Components>>*...> mContainers;
    //TODO: looks like this is only needed for entity, could be a LinearEntity instead of the entire chunk
    VersionedEntityChunk mChunk;
    size_t mIndex = 0;
  };

  template<class EntityT, bool ENABLE_SAFETY_CHECKS>
  class RuntimeView {
  };

  struct ViewedTypes {
    using TypeId = ecx::typeId_t<LinearEntity>;
    std::vector<TypeId> mIncludes;
    std::vector<TypeId> mExcludes;
    std::vector<TypeId> mReads;
    std::vector<TypeId> mWrites;
    std::vector<TypeId> mOptionalReads;
    std::vector<TypeId> mOptionalWrites;
  };

  template<bool ENABLE_SAFETY_CHECKS>
  class RuntimeView<LinearEntity, ENABLE_SAFETY_CHECKS> {
  public:
    struct AllowedTypes {
      std::vector<ViewedTypes::TypeId> mReads;
      std::vector<ViewedTypes::TypeId> mWrites;
    };

    RuntimeView(EntityRegistry<LinearEntity> registry, const ViewedTypes& viewed) {
      std::vector<ViewedTypes::TypeId> optionals;
      std::vector<ViewedTypes::TypeId> requirements;
      requirements.reserve(viewed.mReads.size() + viewed.mWrites.size() + viewed.mIncludes.size());
      requirements.insert(requirements.end(), viewed.mReads.begin(), viewed.mReads.end());
      requirements.insert(requirements.end(), viewed.mWrites.begin(), viewed.mWrites.end());
      requirements.insert(requirements.end(), viewed.mIncludes.begin(), viewed.mIncludes.end());

      optionals.reserve(viewed.mOptionalReads.size() + viewed.mOptionalWrites.size());
      optionals.insert(optionals.begin(), viewed.mOptionalReads.begin(), viewed.mOptionalReads.end());
      optionals.insert(optionals.begin(), viewed.mOptionalWrites.begin(), viewed.mOptionalWrites.end());

      registry.getAllChunksSatisfyingConditions(mChunks, requirements, viewed.mExcludes, optionals);

      mViewed.mWrites.reserve(viewed.mOptionalWrites.size() + viewed.mWrites.size());
      mViewed.mWrites.insert(mViewed.mWrites.end(), viewed.mWrites.begin(), viewed.mWrites.end());
      mViewed.mWrites.insert(mViewed.mWrites.end(), viewed.mOptionalWrites.begin(), viewed.mOptionalWrites.end());

      //All allowed write types plus the read types: everything allowed to access with write access
      mViewed.mReads.reserve(viewed.mOptionalReads.size() + viewed.mReads.size() + mViewed.mWrites.size());
      mViewed.mReads.insert(mViewed.mReads.end(), viewed.mReads.begin(), viewed.mReads.end());
      mViewed.mReads.insert(mViewed.mReads.end(), viewed.mOptionalReads.begin(), viewed.mOptionalReads.end());
      mViewed.mReads.insert(mViewed.mReads.end(), mViewed.mWrites.begin(), mViewed.mWrites.end());

      //Sort to allow binary search
      std::sort(mViewed.mReads.begin(), mViewed.mReads.end());
      std::sort(mViewed.mWrites.begin(), mViewed.mWrites.end());
    }

    RuntimeView(RuntimeView&&) noexcept = default;
    RuntimeView(const RuntimeView&) = default;
    RuntimeView& operator=(const RuntimeView&) = default;
    RuntimeView& operator=(RuntimeView&&) noexcept = default;

    class It {
    public:
      using value_type = LinearEntity;
      using difference_type = std::ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;
      using iterator_category = std::forward_iterator_tag;

      using ChunkIt = std::vector<VersionedEntityChunk>::iterator;

      It(ChunkIt chunkIt, ChunkIt end, size_t entityIndex, const AllowedTypes& allowedTypes)
        : mChunkIt(chunkIt)
        , mEndIt(end)
        , mEntityIndex(entityIndex)
        , mAllowedTypes(&allowedTypes) {
      }

      It(const It&) = default;
      It& operator=(const It&) = default;

      It& operator++() {
        ++mEntityIndex;
        while(mChunkIt != mEndIt && mEntityIndex >= (*mChunkIt).size()) {
          ++mChunkIt;
          mEntityIndex = 0;
        }
        return *this;
      }

      It& operator++(int) {
        It result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const It& rhs) const {
        return mChunkIt == rhs.mChunkIt && mEntityIndex == rhs.mEntityIndex;
      }

      bool operator!=(const It& rhs) const {
        return !(*this == rhs);
      }

      value_type operator*() {
        return entity();
      }

      LinearEntity entity() const {
        return mChunkIt->indexToEntity(mEntityIndex);
      }

      template<class T>
      T* tryGet() {
        if constexpr(ENABLE_SAFETY_CHECKS) {
          if constexpr(std::is_const_v<T>) {
            //TODO: report access violation somehow?
            if(std::lower_bound(mAllowedTypes->mReads.begin(), mAllowedTypes->mReads.end(), ecx::typeId<std::decay_t<T>, ecx::DefaultTypeCategory>()) == mAllowedTypes->mReads.end()) {
              return nullptr;
            }
          }
          else if(std::lower_bound(mAllowedTypes->mWrites.begin(), mAllowedTypes->mWrites.end(), ecx::typeId<std::decay_t<T>, ecx::DefaultTypeCategory>()) == mAllowedTypes->mWrites.end()) {
            return nullptr;
          }
        }
        auto* container = mChunkIt->tryGet<std::decay_t<T>>();
        return container ? &container->at(mEntityIndex) : nullptr;
      }

    private:
      //Current chunk iterator
      ChunkIt mChunkIt;
      ChunkIt mEndIt;
      //Index of current entity in current chunk
      size_t mEntityIndex = 0;
      const AllowedTypes* mAllowedTypes = nullptr;
    };

    It begin() {
      //Ensure begin is pointing at a valid entity by skipping empty chunks
      return It(std::find_if(mChunks.begin(), mChunks.end(), [](const VersionedEntityChunk& chunk) {
        return chunk.size() != 0;
      }), mChunks.end(), 0, mViewed);
    }

    It find(const LinearEntity& entity) {
      //TODO: inefficient. Could use chunk id to look up particular chunk, but then how to get the iterator of it?
      for(auto it = mChunks.begin(); it != mChunks.end(); ++it) {
        if(it->contains(entity)) {
          return It(it, mChunks.begin(), it->entityToIndex(entity), mViewed);
        }
      }
      return end();
    }

    It end() {
      return It(mChunks.end(), mChunks.end(), size_t(0), mViewed);
    }

  private:
    std::vector<VersionedEntityChunk> mChunks;
    //Count of chunks in the registry the last time the view was computed. Used to invalidate cached views
    size_t mCachedChunkCount = 0;
    AllowedTypes mViewed;
  };

  //A combination of registry iterators to allow viewing entities that satisfy conditions as specified by the tags above
  template<class... Args>
  class View<LinearEntity, Args...> {
  public:
    using ViewTraits = typename ViewDeducer::ViewTraits<Args...>;
    template<class... Args>
    using ViewedEntityT = ViewedEntity<LinearEntity, Args...>;
    using SelfT = View<LinearEntity, Args...>;

    class It {
    public:
      using value_type = typename ViewTraits::template ApplyAllowedTypes<ViewedEntityT>::type;
      using difference_type = std::ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;
      using iterator_category = std::forward_iterator_tag;

      using ChunkIt = std::vector<VersionedEntityChunk>::iterator;

      It(ChunkIt chunkIt, ChunkIt end, size_t entityIndex)
        : mChunkIt(chunkIt)
        , mEndIt(end)
        , mEntityIndex(entityIndex) {
      }

      It(const It&) = default;
      It& operator=(const It&) = default;

      It& operator++() {
        ++mEntityIndex;
        while(mChunkIt != mEndIt && mEntityIndex >= (*mChunkIt).size()) {
          ++mChunkIt;
          mEntityIndex = 0;
        }
        return *this;
      }

      It& operator++(int) {
        It result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const It& rhs) const {
        return mChunkIt == rhs.mChunkIt && mEntityIndex == rhs.mEntityIndex;
      }

      bool operator!=(const It& rhs) const {
        return !(*this == rhs);
      }

      value_type operator*() {
        return value_type(*mChunkIt, mEntityIndex);
      }

    private:
      //Current chunk iterator
      ChunkIt mChunkIt;
      ChunkIt mEndIt;
      //Index of current entity in current chunk
      size_t mEntityIndex = 0;
    };

    class ChunkIt {
    public:
      using value_type = typename ViewTraits::template ApplyAllowedTypes<ViewedEntityChunk>::type;
      using difference_type = std::ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;
      using iterator_category = std::forward_iterator_tag;

      using RawIt = std::vector<VersionedEntityChunk>::iterator;

      ChunkIt(RawIt chunkIt)
        : mChunkIt(chunkIt) {
      }

      ChunkIt(const ChunkIt&) = default;
      ChunkIt& operator=(const ChunkIt&) = default;

      ChunkIt& operator++() {
        ++mChunkIt;
        return *this;
      }

      ChunkIt& operator++(int) {
        It result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const ChunkIt& rhs) const {
        return mChunkIt == rhs.mChunkIt;
      }

      bool operator!=(const ChunkIt& rhs) const {
        return !(*this == rhs);
      }

      value_type operator*() {
        return value_type(*mChunkIt);
      }

    private:
      RawIt mChunkIt;
    };

    template<class... Deps>
    struct DependencyDeduce {
      DependencyDeduce() {
        build();
      }

      void build() {
        (build(Deps{}), ...);
      }

      template<class T>
      void build(Include<T>) { mIncludes.push_back(typeId<std::decay_t<T>, LinearEntity>()); }
      template<class T>
      void build(Read<T>) { mIncludes.push_back(typeId<std::decay_t<T>, LinearEntity>()); }
      template<class T>
      void build(Write<T>) { mIncludes.push_back(typeId<std::decay_t<T>, LinearEntity>()); }
      template<class T>
      void build(Exclude<T>) { mExcludes.push_back(typeId<std::decay_t<T>, LinearEntity>()); }
      template<class T>
      void build(OptionalRead<T>) { mOptionals.push_back(typeId<std::decay_t<T>, LinearEntity>()); }
      template<class T>
      void build(OptionalWrite<T>) { mOptionals.push_back(typeId<std::decay_t<T>, LinearEntity>()); }

      std::vector<typeId_t<LinearEntity>> mIncludes;
      std::vector<typeId_t<LinearEntity>> mExcludes;
      std::vector<typeId_t<LinearEntity>> mOptionals;
    };

    static View recycleView(View&& view, EntityRegistry<LinearEntity>& registry) {
      //If cached view is still valid, re-use it
      if(view.mCachedChunkCount == registry.chunkCount()) {
        return std::move(view);
      }
      //Cached view is invalid, compute a new one
      return View(registry);
    }

    //Can't be used this way but allows default construction in templates to compile
    View() = default;

    View(EntityRegistry<LinearEntity>& registry)
      : mCachedChunkCount(registry.chunkCount()) {
      DependencyDeduce<Args...> dependencies;
      registry.getAllChunksSatisfyingConditions(mChunks, dependencies.mIncludes, dependencies.mExcludes, dependencies.mOptionals);
    }
    View(const View&) = default;
    View(View&&) = default;
    View& operator=(const View&) = default;
    View& operator=(View&&) = default;

    It begin() {
      //Ensure begin is pointing at a valid entity by skipping empty chunks
      return It(std::find_if(mChunks.begin(), mChunks.end(), [](const VersionedEntityChunk& chunk) {
        return chunk.size() != 0;
      }), mChunks.end(), 0);
    }

    It end() {
      return It(mChunks.end(), mChunks.end(), size_t(0));
    }

    It find(const LinearEntity& entity) {
      auto it = std::find_if(mChunks.begin(), mChunks.end(), [&entity](const VersionedEntityChunk& chunk) {
        return chunk.contains(entity);
      });
      return it != mChunks.end() ? It(it, mChunks.end(), (*it).entityToIndex(entity)) : It(it, mChunks.end(), size_t(0));
    }

    ChunkIt chunksBegin() {
      //Ensure begin is pointing at a valid entity by skipping empty chunks
      return ChunkIt(std::find_if(mChunks.begin(), mChunks.end(), [](const VersionedEntityChunk& chunk) {
        return chunk.size() != 0;
      }));
    }

    ChunkIt chunksEnd() {
      return mChunks.end();
    }

    //Wrapper to allow range based for loops using for(auto&& chunk : view.chunks())
    struct Chunks {
      ChunkIt begin() {
        return mSelf->chunksBegin();
      }

      ChunkIt end() {
        return mSelf->chunksEnd();
      }

      SelfT* mSelf;
    };

    Chunks chunks() {
      return Chunks{ this };
    }

    //Intended for use with the singleton entity where only one is expected
    std::optional<typename It::value_type> tryGetFirst() {
      auto it = begin();
      return it != end() ? std::make_optional(*it) : std::nullopt;
    }

    LinearEntity tryGetFirstEntity() {
      auto it = begin();
      return it != end() ? it.entity() : LinearEntity();
    }

  private:
    std::vector<VersionedEntityChunk> mChunks;
    //Count of chunks in the registry the last time the view was computed. Used to invalidate cached views
    size_t mCachedChunkCount = 0;
  };
}