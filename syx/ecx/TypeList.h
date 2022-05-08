#pragma once

namespace ecx {
  template<class... Args>
  struct TypeList {};

  //Type list for non type template parameters
  template<auto... Args>
  struct AutoTypeList {
    using TypeList = TypeList<decltype(Args)...>;
    inline constexpr static size_t Size = sizeof...(Args);

    static auto getValues() {
      return std::make_tuple(Args...);
    }
  };

  template<class... A>
  static TypeList<A...> combine(TypeList<A...>);
  template<class... A, class... B>
  static TypeList<A..., B...> combine(TypeList<A...>, TypeList<B...>);
  TypeList<> combine();

  //Remove all of type from list
  template<class Value, class List>
  struct RemoveT {};
  //Specialization for type lists that contain the argument
  template<class Value, class... Args>
  struct RemoveT<Value, ecx::TypeList<Args...>> {
    template<class ToAdd, class... CurList>
    using AddExceptValue = std::conditional_t<std::is_same_v<Value, ToAdd>, ecx::TypeList<CurList...>, ecx::TypeList<CurList..., ToAdd>>;

    //Base template
    template<class Built, class Remaining>
    struct BuildExceptValue {};
    //Main recursive case, add one to the list
    template<class... Built, class One, class... Remaining>
    struct BuildExceptValue<ecx::TypeList<Built...>, ecx::TypeList<One, Remaining...>> {
      using Type = typename BuildExceptValue<AddExceptValue<One, Built...>, ecx::TypeList<Remaining...>>::Type;
    };
    //End of recursion, resulting list without `Value`
    template<class... Built>
    struct BuildExceptValue<ecx::TypeList<Built...>, ecx::TypeList<>> {
      using Type = ecx::TypeList<Built...>;
    };
    //Recursively build up the empty type list with all arguments in the original except `Value`
    using Type = typename BuildExceptValue<ecx::TypeList<>, ecx::TypeList<Args...>>::Type;
  };
  template<class ToRemove, class List>
  using RemoveType = typename RemoveT<ToRemove, List>::Type;

  //Predicate<a, b>::value must be bool indicating "<" status
  template<class List, template<class, class> class Predicate>
  struct SmallestTypeInListT {};
  template<class... List, template<class, class> class Predicate>
  struct SmallestTypeInListT<TypeList<List...>, Predicate> {
    template<auto>
    struct BoolToType {};
    template<>
    struct BoolToType<true> : std::true_type {};
    template<>
    struct BoolToType<false> : std::false_type {};

    //True if the predicate returns true for value against all other values in the list,
    //or if they are equal, as determined by < being false on both sides.
    template<class Value, class ToCheck>
    struct IsSmallerThanListT {};
    template<class Value, class... ToCheck>
    struct IsSmallerThanListT<Value, TypeList<ToCheck...>> {
      constexpr static bool value = std::conjunction_v<
        std::disjunction<
          Predicate<Value, ToCheck>,
          //Check !(a < b) && !(b < a), which is equality assuming predicate isn't broken
          BoolToType<!Predicate<Value, ToCheck>::value && !Predicate<ToCheck, Value>::value>
        >...
      >;
    };

    //Go through each value, recursing until a type is found that is smaller than all the rest
    template<class ToCheck>
    struct ResultT {};
    template<class One, class... ToCheck>
    struct ResultT<TypeList<One, ToCheck...>> {
      //Build a list of all types except this to compare against
      using ListWithoutOne = RemoveType<One, TypeList<List...>>;
      //Compare this type against list without it. If it's smallest, the desired type has been found
      //If not, recurse with the next element in the list
      using Type = std::conditional_t<
        IsSmallerThanListT<One, ListWithoutOne>::value,
        One,
        typename ResultT<TypeList<ToCheck...>>::Type
      >;
    };

    template<class One>
    struct ResultT<TypeList<One>> {
      using Type = One;
    };
    template<>
    struct ResultT<TypeList<>> {
      //Only happens if trying to get smallest of empty type list, don't do this
    };

    using Type = typename ResultT<TypeList<List...>>::Type;
  };
  template<class List, template<class, class> class Predicate>
  using SmallestTypeInList = typename SmallestTypeInListT<List, Predicate>::Type;

  //Sort a list of types with the predicate assuming there are no duplicates
  //Predicate must behave like a "<=" for types
  template<class List, template<class,class> class Predicate>
  struct SortUniqueTypesT {};
  template<class... Args, template<class,class> class Predicate>
  struct SortUniqueTypesT<TypeList<Args...>, Predicate> {
    template<class Built, class Remaining>
    struct BuildSorted {};
    //Build up one at a time, putting the smallest element on the end of the list every time
    //Existing order is preserved if multiple smallest elements exist
    template<class... Built, class... Remaining>
    struct BuildSorted<TypeList<Built...>, TypeList<Remaining...>> {
      using SmallestT = SmallestTypeInList<TypeList<Remaining...>, Predicate>;
      using ListWithoutSmallest = RemoveType<SmallestT, TypeList<Remaining...>>;

      using Type = typename BuildSorted<TypeList<Built..., SmallestT>, ListWithoutSmallest>::Type;
    };
    template<class... Built>
    struct BuildSorted<TypeList<Built...>, TypeList<>> {
      using Type = TypeList<Built...>;
    };

    using Type = typename BuildSorted<TypeList<>, TypeList<Args...>>::Type;
  };
  template<class List, template<class,class> class Predicate>
  using SortUniqueTypes = typename SortUniqueTypesT<List, Predicate>::Type;

  template<class... A, class... B, class... C>
  static decltype(combine(TypeList<A..., B...>(), std::declval<C>()...)) combine(TypeList<A...>, TypeList<B...>, C...);

  template<class T, class... A>
  static std::disjunction<std::is_same<T, A>...> typeListContains(TypeList<A...>);

  //Remove duplicates in list
  template<class>
  struct TypeListUniqueT {};
  template<class... List>
  struct TypeListUniqueT<TypeList<List...>> {
    template<class Built, class Todo>
    struct ResultT {};
    //Build one at a time, adding if it's not already in the destination list
    template<class... Built, class One, class... Rest>
    struct ResultT<TypeList<Built...>, TypeList<One, Rest...>>
      : std::conditional<std::disjunction_v<std::is_same<One, Built>...>,
          typename ResultT<TypeList<Built...>, TypeList<Rest...>>::type,
          typename ResultT<TypeList<Built..., One>, TypeList<Rest...>>::type> {};
    template<class... Built>
    struct ResultT<TypeList<Built...>, TypeList<>> {
      using type = TypeList<Built...>;
    };

    using Type = typename ResultT<TypeList<>, TypeList<List...>>::type;
  };
  template<class List>
  using TypeListUnique = typename TypeListUniqueT<List>::Type;

  //End of recursion, nothing left to evaluate
  template<template<class> class Filter, class... Results>
  TypeList<int> _typeListFilter(TypeList<Results...>, TypeList<>);

  //Use conditional to add to result type list if it passes filter then recurse the rest of EvalRest
  template<template<class> class Filter, class... Results, class EvalOne, class... EvalRest>
  decltype(_typeListFilter<Filter>(
    std::declval<std::conditional_t<Filter<EvalOne>::value, TypeList<EvalOne, Results...>, TypeList<Results...>>>(),
    std::declval<TypeList<EvalRest...>>())) _typeListFilter(TypeList<Results...>, TypeList<EvalOne, EvalRest...>);

  //Filter type list using the filter template expected to evaluate to true_type (include) or false_type (exclude)
  //Begin recursion with empty result type list and full evaluation list
  template<template<class> class Filter, class... Args>
  decltype(_typeListFilter<Filter>(std::declval<TypeList<>>(), std::declval<TypeList<Args...>>())) typeListFilter(TypeList<Args...>);

  //Transform one type list to another using Transform<T>::type template
  template<template<class> class Transform, class... Args>
  TypeList<typename Transform<Args>::type...> typeListTransform(TypeList<Args...>);

  template<class... Args>
  constexpr size_t typeListSize(TypeList<Args...>) {
    return sizeof...(Args);
  }

  template<template<class...> class To, class... Args>
  static To<Args...> changeType(TypeList<Args...>);

  template<template<auto...> class To, auto... Args>
  static constexpr To<Args...> changeType(AutoTypeList<Args...>);

  //Unlike type list, it could be appropriate to actually return the values instead of only using in unevaluated contexts
  template<class To, auto... Args>
  static constexpr To changeType(AutoTypeList<Args...>) {
    return To{ Args... };
  }

  template<class To, class...Args>
  using changeTypeT = decltype(changeType<To>(TypeList<Args...>{}));
}