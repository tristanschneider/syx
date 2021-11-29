#pragma once

#include "EntityFactory.h"
#include "EntityModifier.h"
#include "EntityRegistry.h"
#include "FunctionTraits.h"
#include "View.h"

namespace ecx {
  //Describes access patterns for registry
  struct SystemInfo {
    //In ascending order of strictness. Values at the top of the list contain all the types of the lower ones
    //Components whose existence is checked (Include<> Exclude<>)
    std::vector<typeId_t<SystemInfo>> mExistenceTypes;
    //Components that are read from (Read<>)
    std::vector<typeId_t<SystemInfo>> mReadTypes;
    //Components that are written to (Write<>)
    std::vector<typeId_t<SystemInfo>> mWriteTypes;
    //Components that are created or destroyed (EntityModifier<>)
    std::vector<typeId_t<SystemInfo>> mFactoryTypes;
    //If this can create or destroy entities (EntityFactory<>)
    bool mUsesEntityFactory = false;
  };

  //List of Views, Factories, and entity factory. Values must be specified here for them to be accessible in get
  template<class EntityT, class... Accessors>
  class SystemContext {
  public:
    using EntityType = EntityT;

    SystemContext(EntityRegistry<EntityT>& registry)
      : mRegistry(&registry) {
    }
    SystemContext(const SystemContext&) = default;
    SystemContext& operator=(const SystemContext&) = default;

    //Get values declared by the context
    template<class T>
    T get() {
      static_assert(std::disjunction_v<std::is_same<T, Accessors>...>, "Type must be declared by the context to be accessible");
      return T(*mRegistry);
    }

    static SystemInfo buildInfo() {
      SystemInfo result;
      (typename InfoBuilder<Accessors>::build(result), ...);
      return result;
    }

  private:
    //Specialization to deduce accessor types
    template<class T>
    struct InfoBuilder {
      static void build(SystemInfo& info) {
        static_assert(sizeof(T) == -1, "Accessor should have specialized below");
      }
    };

    //EntityModifier deduction
    template<class... Components>
    struct InfoBuilder<EntityModifier<EntityT, Components...>> {
      static void build(SystemInfo& info) {
        auto toAdd = { typeId<Components, SystemInfo>()... };
        info.mExistenceTypes.insert(info.mExistenceTypes.end(), toAdd);
        info.mReadTypes.insert(info.mReadTypes.end(), toAdd);
        info.mWriteTypes.insert(info.mWriteTypes.end(), toAdd);
        info.mFactoryTypes.insert(info.mFactoryTypes.end(), toAdd);
      }
    };

    //EntityFactory deduction
    template<>
    struct InfoBuilder<EntityFactory<EntityT>> {
      static void build(SystemInfo& info) {
        info.mUsesEntityFactory = true;
      }
    };

    //View deduction
    template<template<class...> class ViewT, class... ViewArgs>
    struct InfoBuilder<ViewT<EntityT, ViewArgs...>> {
      template<class T>
      static void _addOne(SystemInfo&, T) {
        static_cast(sizeof(T) == -1, "One of the overloads below should have been chosen");
      }

      template<class T>
      static void _addOne(SystemInfo& info, Read<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
        info.mReadTypes.push_back(typeId<T, SystemInfo>());
      }

      template<class T>
      static void _addOne(SystemInfo& info, Write<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
        info.mReadTypes.push_back(typeId<T, SystemInfo>());
        info.mWriteTypes.push_back(typeId<T, SystemInfo>());
      }

      template<class T>
      static void _addOne(SystemInfo& info, Include<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
      }

      template<class T>
      static void _addOne(SystemInfo& info, Exclude<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
      }

      template<class T>
      static void _addOne(SystemInfo& info, OptionalRead<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
        info.mReadTypes.push_back(typeId<T, SystemInfo>());
      }

      template<class T>
      static void _addOne(SystemInfo& info, OptionalWrite<T>) {
        info.mExistenceTypes.push_back(typeId<T, SystemInfo>());
        info.mReadTypes.push_back(typeId<T, SystemInfo>());
        info.mWriteTypes.push_back(typeId<T, SystemInfo>());
      }

      static void build(SystemInfo& info) {
        //Default construct an object of each ViewArg type for the sake of using overload resolution to deduce the type
        (_addOne(info, ViewArgs{}), ...);
      }
    };

    EntityRegistry<EntityT>* mRegistry = nullptr;
  };

  //Base system interface intended for storing registered systems.
  //Implementations are expected to go through the System below, not inhert directly from this
  template<class EntityT>
  struct ISystem {
    virtual ~ISystem() = default;
    virtual void tick(EntityRegistry<EntityT>& registry) const = 0;
    virtual SystemInfo getInfo() const = 0;
  };

  //Wrapper that scopes access of registry down to what is specified by the context
  //Implementation goes in derived _tick
  template<class Context, class EntityT>
  struct System : public ISystem<EntityT> {
    void tick(EntityRegistry<EntityT>& registry) const final {
      Context context(registry);
      _tick(context);
    }

    SystemInfo getInfo() const final {
      return typename Context::buildInfo();
    }

    virtual void _tick(Context& context) const = 0;
  };

  //Create a system from a function that takes the desired context: [](Context<uint32_t, View<Read<int>>, EntityFactory>& context) { ... }
  template<class Fn>
  auto makeSystem(Fn fn) {
    //The function's first argument should be the context type. Member functions not supported
    using ContextType = std::decay_t<typename FunctionTraits<Fn>::argument<0>::type>;
    using EntityT = typename ContextType::EntityType;

    struct FnSystem : public System<ContextType, EntityT> {
      FnSystem(Fn fn)
        : mFn(std::move(fn)) {
      }

      void _tick(ContextType& context) const override {
        mFn(context);
      }

      Fn mFn;
    };

    return std::make_unique<FnSystem>(fn);
  }
}