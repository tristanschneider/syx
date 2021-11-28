#pragma once

#include "EntityRegistry.h"
#include <optional>
#include <type_traits>

namespace ecx {
  //Type tags to specify desired access in a view

  //Component must be present but it will not be accessed
  template<class Component>
  struct Include {
    using type = Component;
  };

  //Component must not be present
  template<class Component>
  struct Exclude {
    using type = Component;
  };

  //Component may be present and is read only
  template<class Component>
  struct OptionalRead {
    using type = Component;
  };

  //Component may be present and is write only
  template<class Component>
  struct OptionalWrite {
    using type = Component;
  };

  //Component must be present and is read only
  template<class Component>
  struct Read {
    using type = Component;
  };

  //Component must be present and is write only
  template<class Component>
  struct Write {
    using type = Component;
  };

  namespace ViewDeducer {
    template<class Accessor>
    struct Unwrap {
      using type = Accessor;
    };

    template<class T>
    struct Unwrap<Include<T>> : public Unwrap<T> {};
    template<class T>
    struct Unwrap<Exclude<T>> : public Unwrap<T> {};
    template<class T>
    struct Unwrap<OptionalRead<T>> : public Unwrap<T> {};
    template<class T>
    struct Unwrap<OptionalWrite<T>> : public Unwrap<T> {};
    template<class T>
    struct Unwrap<Read<T>> : public Unwrap<T> {};
    template<class T>
    struct Unwrap<Write<T>> : public Unwrap<T> {};

    template<class Component>
    struct CanLeadIteration {
      static_assert(sizeof(Component) == -1, "Should have been speccialized below");
    };

    //One component type is selected to determine which entities come next, which can only apply to these access types
    template<class T>
    struct CanLeadIteration<Include<T>> : public std::true_type {};
    template<class T>
    struct CanLeadIteration<Exclude<T>> : public std::false_type {};
    template<class T>
    struct CanLeadIteration<OptionalRead<T>> : public std::false_type {};
    template<class T>
    struct CanLeadIteration<OptionalWrite<T>> : public std::false_type {};
    template<class T>
    struct CanLeadIteration<Read<T>> : public std::true_type {};
    template<class T>
    struct CanLeadIteration<Write<T>> : public std::true_type {};

    //True if view.tryGet<Component> should result in a value
    template<class Component>
    struct IsViewable {
      static_assert(sizeof(Component) == -1, "Should have been specialized below");
    };

    template<class T>
    struct IsViewable<Read<T>> : public std::true_type {};
    template<class T>
    struct IsViewable<Write<T>> : public std::true_type {};
    template<class T>
    struct IsViewable<Include<T>> : public std::false_type {};
    template<class T>
    struct IsViewable<Exclude<T>> : public std::false_type {};
    template<class T>
    struct IsViewable<OptionalRead<T>> : public std::true_type {};
    template<class T>
    struct IsViewable<OptionalWrite<T>> : public std::true_type {};

    template<class... Args>
    struct ViewTraits {
      //Takes the `To` type and recursively applies specializations of ApplyOne until all elements have been applied,
      //resulting in `To<args>` with only the desired arguments in the argument list
      template<template <class...> class To>
      struct ApplyAllowedTypes {
        //Base type to apply the list of allowed types to a ViewedEntity
        template<template <class...> class T, class... Args>
        struct ApplyOne {
          using type = T<>;
        };

        //Adds the given type to the list
        template<template <class...> class T, class Arg, class... Rest>
        struct AddOne {
          template<class... R>
          using TAndArg = T<Arg, R...>;
          using type = typename ApplyOne<TAndArg, Rest...>::type;
        };

        //Skips the given type in the list
        template<template <class...> class T, class Arg, class... Rest>
        struct IgnoreOne {
          using type = typename ApplyOne<T, Rest...>::type;
        };

        //Specializations for all allowed types
        template<template <class...> class T, class C, class... Rest>
        struct ApplyOne<T, Read<C>, Rest...> : public AddOne<T, const C, Rest...> {};
        template<class C, template <class...> class T, class... Rest>
        struct ApplyOne<T, Write<C>, Rest...> : public AddOne<T, C, Rest...> {};
        template<class C, template <class...> class T, class... Rest>
        struct ApplyOne<T, OptionalRead<C>, Rest...> : public AddOne<T, const C, Rest...> {};
        template<class C, template <class...> class T, class... Rest>
        struct ApplyOne<T, OptionalWrite<C>, Rest...> : public AddOne<T, C, Rest...> {};

        //Specializations for all skipped types
        template<template <class...> class T, class C, class... Rest>
        struct ApplyOne<T, Include<C>, Rest...> : public IgnoreOne<T, C, Rest...> {};
        template<template <class...> class T, class C, class... Rest>
        struct ApplyOne<T, Exclude<C>, Rest...> : public IgnoreOne<T, C, Rest...> {};

        using type = typename ApplyOne<To, Args...>::type;
      };
    };

    //Test if a given entity in a view satisfies the view conditions
    template<class EntityT, class T>
    struct EntitySatisfiesCondition {
      static_assert(sizeof(T) == -1, "One of the specializations below should have been instantiated");
    };

    template<class EntityT, class T>
    struct TestComponentExists {
      static bool test(EntityRegistry<EntityT>& registry, const EntityT& entity) {
        return registry.hasComponent<T>(entity);
      }
    };

    template<class EntityT, class T>
    struct TestComponentMissing {
      static bool test(EntityRegistry<EntityT>& registry, const EntityT& entity) {
        return !registry.hasComponent<T>(entity);
      }
    };

    template<class EntityT, bool Value>
    struct TestAlwaysValue {
      static bool test(EntityRegistry<EntityT>&, const EntityT&) {
        return Value;
      }
    };

    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, Include<C>> : public TestComponentExists<EntityT, C> {};
    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, Exclude<C>> : public TestComponentMissing<EntityT, C> {};
    //Value doesn't need to be tested because any value satisfies optional
    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, OptionalRead<C>> : public TestAlwaysValue<EntityT, true> {}; 
    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, OptionalWrite<C>> : public TestAlwaysValue<EntityT, true> {}; 
    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, Read<C>> : public TestComponentExists<EntityT, C> {};
    template<class EntityT, class C>
    struct EntitySatisfiesCondition<EntityT, Write<C>> : public TestComponentExists<EntityT, C> {};
  }

  //A wrapper around access for a particular entity during iteration within a View
  template<class EntityT, class... Components>
  class ViewedEntity {
  public:
    ViewedEntity(EntityRegistry<EntityT>& registry, const EntityT& entity)
      : mRegistry(&registry)
      , mSelf(entity) {
    }

    //This is unforgiving when it comes to const, a write type cannot be accessed with const T
    template<class T>
    using IsAllowedType = std::disjunction<std::is_same<T, Components>...>;

    template<class Component>
    Component* tryGet() {
      static_assert(IsAllowedType<Component>::value);
      auto it = mRegistry->find<Component>(mSelf);
      return it != mRegistry->end<Component>() ? &it.component() : nullptr;
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

    const EntityT& entity() {
      return mSelf;
    }

  private:
    EntityRegistry<EntityT>* mRegistry;
    EntityT mSelf;
  };

  //A combination of registry iterators to allow viewing entities that satisfy conditions as specified by the tags above
  template<class EntityT, class... Args>
  class View {
  public:
    using ViewTraits = typename ViewDeducer::ViewTraits<Args...>;
    template<class... Args>
    using ViewedEntityT = ViewedEntity<EntityT, Args...>;

    class It {
    public:
      template<class...>
      struct Testa {};

      using value_type = typename ViewTraits::template ApplyAllowedTypes<ViewedEntityT>::type;
      using difference_type = std::ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;
      using iterator_category = std::forward_iterator_tag;

      using EntityIterators = std::tuple<typename EntityRegistry<EntityT>::It<typename ViewDeducer::Unwrap<Args>::type>...>;

      It(EntityRegistry<EntityT>& registry, EntityIterators iterators)
        : mRegistry(&registry)
        , mIterators(iterators) {
        static_assert(std::tuple_size_v<EntityIterators> > 0);
        _findInitialEntity();
      }

      It(const It&) = default;
      It& operator=(const It&) = default;

      It& operator++() {
        if(std::optional<EntityT> entity = _findNextEntity(_findSmallestIterator(), false)) {
          _setEntity(*entity);
        }
        else {
          (_setEnd<typename ViewDeducer::Unwrap<Args>::type>(), ...);
        }
        return *this;
      }

      It& operator++(int) {
        auto result = mSparseIt;
        ++(*this);
        return It(mPool, result);
      }

      bool operator==(const It& rhs) const {
        return mIterators == rhs.mIterators;
      }

      bool operator!=(const It& rhs) const {
        return !(*this == rhs);
      }

      value_type operator*() {
        return value_type(*mRegistry, std::get<0>(mIterators).entity());
      }

    private:
      void _findInitialEntity() {
       if(std::optional<EntityT> found = _findNextEntity(_findSmallestIterator(), true) ) {
         _setEntity(*found);
       }
       else {
         (_setEnd<typename ViewDeducer::Unwrap<Args>::type>(), ...);
       }
      }

      template<class T>
      void _findSmallestIterator(void*& currentBest, size_t& smallest) {
        if constexpr(ViewDeducer::CanLeadIteration<T>::value) {
          auto& it = _getIterator<T>();
          if(const size_t size = mRegistry->size<T>(); size < smallest || !currentBest) {
            currentBest = &it;
            smallest = size;
          }
        }
        else {
          (void)currentBest,smallest;
        }
      }

      void* _findSmallestIterator() {
        void* best = nullptr;
        size_t smallest = 0;
        (_findSmallestIterator<Args>(best, smallest), ...);
        return best;
      }

      template<class T>
      void _findNextEntity(std::optional<EntityT>& result, void* iterator, bool tryThisOne) {
        //Keep iterating until all view conditions are satisfied or the end is reached
        bool satisfied = false;
        while(!satisfied) {
          satisfied = true;
          auto& it = _getIterator<T>();
          if(&it == iterator) {
            if(!tryThisOne) {
              ++it;
            }
            //Causes the first one to be tried or skipped over depending on the value
            tryThisOne = false;

            if(it != mRegistry->end<T>()) {
              const EntityT candidate = it.entity();
              satisfied = (ViewDeducer::EntitySatisfiesCondition<EntityT, Args>::test(*mRegistry, candidate) && ...);
              if(satisfied) {
                result = candidate;
              }
            }
          }
        }
      }

      std::optional<EntityT> _findNextEntity(void* iterator, bool tryThisOne) {
        std::optional<EntityT> found;
        (_findNextEntity<typename ViewDeducer::Unwrap<Args>::type>(found, iterator, tryThisOne), ...);
        return found;
      }

      template<class T>
      void _setEntity(const EntityT& entity) {
        //Iterator doesn't matter for non-viewable types.
        //TODO: exclude them from the tuple
        if constexpr(ViewDeducer::IsViewable<T>::value) {
          auto& it = _getIterator<typename ViewDeducer::Unwrap<T>::type>();
          //Optional could be end
          if(it == mRegistry->end<typename ViewDeducer::Unwrap<T>::type>() || it.entity() != entity) {
            it = mRegistry->find<typename ViewDeducer::Unwrap<T>::type>(entity);
          }
        }
        else {
          (void)entity;
        }
      }

      void _setEntity(const EntityT& entity) {
        (_setEntity<Args>(entity), ...);
      }

      template<class T>
      void _setEnd() {
        _getIterator<T>() = mRegistry->end<T>();
      }

      template<class T>
      auto& _getIterator() {
        return std::get<typename EntityRegistry<EntityT>::It<typename ViewDeducer::Unwrap<T>::type>>(mIterators);
      }

      EntityRegistry<EntityT>* mRegistry = nullptr;
      EntityIterators mIterators;
    };

    View(EntityRegistry<EntityT>& registry)
      : mRegistry(&registry) {
    }
    View(const View&) = default;
    View& operator=(const View&) = default;

    It begin() {
      return It(*mRegistry, std::make_tuple(mRegistry->begin<typename ViewDeducer::Unwrap<Args>::type>()...));
    }

    It end() {
      return It(*mRegistry, std::make_tuple(mRegistry->end<typename ViewDeducer::Unwrap<Args>::type>()...));
    }

    It find(const EntityT& entity) {
      return It(*mRegistry, std::make_tuple(mRegistry->find<typename ViewDeducer::Unwrap<Args>::type>(entity)...));
    }

  private:
    EntityRegistry<EntityT>* mRegistry;
  };
}