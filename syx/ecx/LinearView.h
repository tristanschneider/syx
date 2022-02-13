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

    ViewedEntityChunk(EntityChunk& chunk)
      : mChunk(&chunk) {
    }
    ViewedEntityChunk(const ViewedEntityChunk&) = default;
    ViewedEntityChunk& operator=(const ViewedEntityChunk&) = default;

    template<class T>
    std::conditional_t<std::is_const_v<T>, const std::vector<std::decay_t<T>>*, std::vector<T>*> tryGet() {
      static_assert(IsAllowedType<T>::value);
      return mChunk->tryGet<T>();
    }

    size_t size() const {
      return mChunk->size();
    }

    LinearEntity indexToEntity(size_t index) const {
      return mChunk->indexToEntity(index);
    }

    size_t entityToIndex(const LinearEntity& entity) const {
      return mChunk->entityToIndex(entity);
    }

    template<class ComponentT>
    ComponentT* tryGetComponent(const LinearEntity& entity) {
      static_assert(IsAllowedType<ComponentT>::value);
      return mChunk->tryGetComponent<ComponentT>(entity);
    }

    bool contains(const LinearEntity& entity) {
      return mChunk->contains(entity);
    }

    //Take the factory to force the system to indicate that it will be destroying entities
    void clear(EntityFactory<LinearEntity>&) {
      mChunk->clearEntities();
    }

  private:
    EntityChunk* mChunk = nullptr;
  };

  //A wrapper around access for a particular entity during iteration within a View
  template<class... Components>
  class ViewedEntity<LinearEntity, Components...> {
  public:
    ViewedEntity(EntityChunk& chunk, size_t index)
      : mContainers{ chunk.tryGet<std::decay_t<Components>>()... }
      , mChunk(&chunk)
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
      return mChunk->indexToEntity(mIndex);
    }

  private:
    template<class T>
    auto tryGetContainerForComponent() {
      return std::get<std::vector<std::decay_t<T>>*>(mContainers);
    }

    //Tuple of vector of all viewable components
    std::tuple<std::vector<std::decay_t<Components>>*...> mContainers;
    EntityChunk* mChunk = nullptr;
    size_t mIndex = 0;
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

      using ChunkIt = std::vector<std::shared_ptr<EntityChunk>>::iterator;

      It(ChunkIt chunkIt, ChunkIt end, size_t entityIndex)
        : mChunkIt(chunkIt)
        , mEndIt(end)
        , mEntityIndex(entityIndex) {
      }

      It(const It&) = default;
      It& operator=(const It&) = default;

      It& operator++() {
        ++mEntityIndex;
        while(mChunkIt != mEndIt && mEntityIndex >= (*mChunkIt)->size()) {
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
        return value_type(**mChunkIt, mEntityIndex);
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

      using RawIt = std::vector<std::shared_ptr<EntityChunk>>::iterator;

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
        return value_type(**mChunkIt);
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
      return It(std::find_if(mChunks.begin(), mChunks.end(), [](const std::shared_ptr<EntityChunk>& chunk) {
        return chunk->size() != 0;
      }), mChunks.end(), 0);
    }

    It end() {
      return It(mChunks.end(), mChunks.end(), size_t(0));
    }

    It find(const LinearEntity& entity) {
      auto it = std::find_if(mChunks.begin(), mChunks.end(), [&entity](const std::shared_ptr<EntityChunk>& chunk) {
        return chunk->contains(entity);
      });
      return it != mChunks.end() ? It(it, mChunks.end(), (*it)->entityToIndex(entity)) : It(it, mChunks.end(), size_t(0));
    }

    ChunkIt chunksBegin() {
      //Ensure begin is pointing at a valid entity by skipping empty chunks
      return ChunkIt(std::find_if(mChunks.begin(), mChunks.end(), [](const std::shared_ptr<EntityChunk>& chunk) {
        return chunk->size() != 0;
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

  private:
    std::vector<std::shared_ptr<EntityChunk>> mChunks;
    //Count of chunks in the registry the last time the view was computed. Used to invalidate cached views
    size_t mCachedChunkCount = 0;
  };
}