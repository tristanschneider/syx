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

  template<class... A, class... B, class... C>
  static decltype(combine(TypeList<A..., B...>(), std::declval<C>()...)) combine(TypeList<A...>, TypeList<B...>, C...);

  template<class T, class... A>
  static std::disjunction<std::is_same<T, A>...> typeListContains(TypeList<A...>);

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
}