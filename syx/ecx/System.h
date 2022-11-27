#pragma once

#include "AnyTuple.h"
#include "EntityCommandBuffer.h"
#include "EntityFactory.h"
#include "EntityModifier.h"
#include "EntityRegistry.h"
#include "FunctionTraits.h"
#include "TypeList.h"
#include "View.h"

namespace ecx {
  struct ThreadLocalContextTag {};
  using ThreadLocalContext = AnyTuple<ThreadLocalContextTag>;

  //Describes access patterns for registry
  template<class EntityT>
  struct SystemInfo {
    using IDT = typeId_t<EntityT>;

    template<class T>
    static void typeId() {
      return typeId<std::decay_t<T>(), EntityT>();
    }

    //In ascending order of strictness. Values at the top of the list contain all the types of the lower ones
    //Components whose existence is checked (Include<> Exclude<>)
    std::vector<typeId_t<EntityT>> mExistenceTypes;
    //Components that are read from (Read<>)
    std::vector<typeId_t<EntityT>> mReadTypes;
    //Components that are written to (Write<>)
    std::vector<typeId_t<EntityT>> mWriteTypes;
    //Components that are created or destroyed (EntityModifier<>)
    std::vector<typeId_t<EntityT>> mFactoryTypes;
    //Components created or destroyed through a command buffer
    std::vector<typeId_t<EntityT>> mCommandBufferTypes;
    //If this can create or destroy entities (EntityFactory<>)
    bool mIsBlocking = false;
    bool mIsCommandProcessor = false;
    //If it uses the command buffer to destroy entities
    bool mDeferDestroysEntities = false;
    //Name of the system
    std::string mName;
    //Thread index that this system must run on. Empty if any is fine
    std::optional<size_t> mThreadRequirement;
  };

  //List of Views, Factories, and entity factory. Values must be specified here for them to be accessible in get
  template<class EntityT, class... Accessors>
  class SystemContext {
  public:
    using EntityType = EntityT;

    //Deduction for TupleT which is a tuple of all the views in `Accessors`
    struct AllViewsTuple {
      template<class T>
      struct IsView : public std::false_type {};
      template<class... Args>
      struct IsView<View<Args...>> : public std::true_type {};

      template<class... Args>
      using ViewTypeListT = decltype(combine(std::declval<std::conditional_t<IsView<Args>::value, TypeList<Args>, TypeList<>>>()...));

      using ViewTypeList = ViewTypeListT<Accessors...>;

      using TupleT = decltype(changeType<std::tuple>(std::declval<ViewTypeList>()));

      template<class T>
      using HasType = decltype(typeListContains<T>(ViewTypeList()));

      //Reserve space in the ThreadLocalContext for all possible views
      template<class... TupleArgs>
      static void _reserve(ThreadLocalContext& context, TypeList<TupleArgs...>) {
        if constexpr(sizeof...(TupleArgs) > 0) {
          context.reserve<TupleArgs...>();
        }
      }

      static void reserve(ThreadLocalContext& context) {
        return _reserve(context, ViewTypeList{});
      }
    };
    using ViewTuple = typename AllViewsTuple::TupleT;

    SystemContext(EntityRegistry<EntityT>& registry, ThreadLocalContext& localContext)
      : mRegistry(&registry)
      , mThreadLocalContext(&localContext) {
      //Reserve space in the context for all views this system needs to ensure no container growth happens during tick
      AllViewsTuple::reserve(*mThreadLocalContext);
    }
    SystemContext(const SystemContext&) = default;
    SystemContext& operator=(const SystemContext&) = default;

    //Get values declared by the context
    template<class T>
    std::conditional_t<AllViewsTuple::HasType<T>::value, T&, T> get() {
      static_assert(std::disjunction_v<std::is_same<T, Accessors>...>, "Type must be declared by the context to be accessible");
      return DeduceGet<T>::get(*mRegistry, *mThreadLocalContext);
    }

    static SystemInfo<EntityT> buildInfo() {
      SystemInfo<EntityT> result;
      (typename InfoBuilder<Accessors>::build(result), ...);
      _removeDuplicates(result.mExistenceTypes);
      _removeDuplicates(result.mFactoryTypes);
      _removeDuplicates(result.mFactoryTypes);
      _removeDuplicates(result.mReadTypes);
      _removeDuplicates(result.mWriteTypes);
      return result;
    }

  private:
    template<class T, class dummy = void>
    struct DeduceGet {
      static T get(EntityRegistry<EntityT>& registry, ThreadLocalContext&) {
        return T(registry);
      }
    };

    template<class... Args>
    struct DeduceGet<EntityCommandBuffer<EntityT, Args...>> {
      static EntityCommandBuffer<EntityT, Args...> get(EntityRegistry<EntityT>&, ThreadLocalContext& ctx) {
        return { ctx.getOrCreate<CommandBuffer<EntityT>>() };
      }
    };

    template<class... Args>
    struct DeduceGet<View<Args...>, std::enable_if_t<AllViewsTuple::HasType<View<Args...>>::value>> {
      static View<Args...>& get(EntityRegistry<EntityT>& registry, ThreadLocalContext& storage) {
        //Get the old stored one
        auto& oldView = storage.emplace<View<Args...>>(registry);
        //Try to recycle the previous view
        oldView = View<Args...>::recycleView(std::move(oldView), registry);
        return oldView;
      }
    };

    static void _removeDuplicates(std::vector<typeId_t<EntityT>>& info) {
      std::sort(info.begin(), info.end());
      info.erase(std::unique(info.begin(), info.end()), info.end());
    }

    //Specialization to deduce accessor types
    template<class T>
    struct InfoBuilder {
      static void build(SystemInfo<EntityT>& info) {
        static_assert(sizeof(T) == -1, "Accessor should have specialized below");
      }
    };

    //EntityModifier deduction
    template<class... Components>
    struct InfoBuilder<EntityModifier<EntityT, Components...>> {
      static void build(SystemInfo<EntityT>& info) {
        auto toAdd = { typeId<Components, EntityT>()... };
        info.mExistenceTypes.insert(info.mExistenceTypes.end(), toAdd);
        info.mReadTypes.insert(info.mReadTypes.end(), toAdd);
        info.mWriteTypes.insert(info.mWriteTypes.end(), toAdd);
        info.mFactoryTypes.insert(info.mFactoryTypes.end(), toAdd);
      }
    };

    template<class... Components>
    struct InfoBuilder<EntityCommandBuffer<EntityT, Components...>> {
      static void build(SystemInfo<EntityT>& info) {
        if(EntityCommandBuffer<EntityT, Components...>::HasDestroyCapability) {
          info.mDeferDestroysEntities = true;
        }
        info.mCommandBufferTypes.insert(info.mCommandBufferTypes.end(), { typeId<Components, EntityT>()... });
      }
    };

    //EntityFactory deduction
    template<>
    struct InfoBuilder<EntityFactory<EntityT>> {
      static void build(SystemInfo<EntityT>& info) {
        info.mIsBlocking = true;
      }
    };

    //View deduction
    template<template<class...> class ViewT, class... ViewArgs>
    struct InfoBuilder<ViewT<EntityT, ViewArgs...>> {
      template<class T>
      static void _addOne(SystemInfo<EntityT>&, T) {
        static_cast(sizeof(T) == -1, "One of the overloads below should have been chosen");
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, Read<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
        info.mReadTypes.push_back(typeId<T, EntityT>());
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, Write<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
        info.mReadTypes.push_back(typeId<T, EntityT>());
        info.mWriteTypes.push_back(typeId<T, EntityT>());
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, Include<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, Exclude<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, OptionalRead<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
        info.mReadTypes.push_back(typeId<T, EntityT>());
      }

      template<class T>
      static void _addOne(SystemInfo<EntityT>& info, OptionalWrite<T>) {
        info.mExistenceTypes.push_back(typeId<T, EntityT>());
        info.mReadTypes.push_back(typeId<T, EntityT>());
        info.mWriteTypes.push_back(typeId<T, EntityT>());
      }

      static void build(SystemInfo<EntityT>& info) {
        //Default construct an object of each ViewArg type for the sake of using overload resolution to deduce the type
        (_addOne(info, ViewArgs{}), ...);
      }
    };

    EntityRegistry<EntityT>* mRegistry = nullptr;
    ThreadLocalContext* mThreadLocalContext = nullptr;

  public:
    //Get all accessors at once. Down here since it relies on DeduceGet definition
    std::tuple<decltype(DeduceGet<Accessors>::get(std::declval<EntityRegistry<EntityT>&>(), std::declval<ThreadLocalContext&>()))...> get() {
      return { DeduceGet<Accessors>::get(*mRegistry, *mThreadLocalContext)... };
    }
  };

  //Base system interface intended for storing registered systems.
  //Implementations are expected to go through the System below, not inhert directly from this
  template<class EntityT>
  struct ISystem {
    virtual ~ISystem() = default;
    virtual void tick(EntityRegistry<EntityT>& registry, ThreadLocalContext& localContext) const = 0;
    //Hack for convenience in tests
    void tick(EntityRegistry<EntityT>& registry) {
      ThreadLocalContext context;
      tick(registry, context);
    }
    virtual SystemInfo<EntityT> getInfo() const = 0;
  };

  //Wrapper that scopes access of registry down to what is specified by the context
  //Implementation goes in derived _tick
  template<class Context, class EntityT>
  struct System : public ISystem<EntityT> {
    void tick(EntityRegistry<EntityT>& registry, ThreadLocalContext& threadContext) const final {
      Context context(registry, threadContext);
      _tick(context);
    }

    SystemInfo<EntityT> getInfo() const override {
      return typename Context::buildInfo();
    }

    virtual void _tick(Context& context) const = 0;
  };

  //Create a system from a function that takes the desired context: [](Context<uint32_t, View<Read<int>>, EntityFactory>& context) { ... }
  template<class Fn>
  auto makeSystem(std::string name, Fn fn, std::optional<size_t> threadRequirement = {}) {
    //The function's first argument should be the context type. Member functions not supported
    using ContextType = std::decay_t<typename FunctionTraits<Fn>::argument<0>::type>;
    using EntityT = typename ContextType::EntityType;

    struct FnSystem : public System<ContextType, EntityT> {
      FnSystem(Fn fn, std::string name, std::optional<size_t> thread)
        : mFn(std::move(fn))
        , mName(std::move(name))
        , mThread(thread) {
      }

      void _tick(ContextType& context) const override {
        mFn(context);
      }

      SystemInfo<EntityT> getInfo() const final {
        SystemInfo<EntityT> info = System<ContextType, EntityT>::getInfo();
        info.mName = mName;
        info.mThreadRequirement = mThread;
        return info;
      }

      Fn mFn;
      std::string mName;
      std::optional<size_t> mThread;
    };

    return std::make_unique<FnSystem>(fn, std::move(name), threadRequirement);
  }
}