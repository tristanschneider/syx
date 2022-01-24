#pragma once

namespace ecx {
  template<class... Args>
  struct TypeList {};
  template<class... A>
  static TypeList<A...> combine(TypeList<A...>);
  template<class... A, class... B>
  static TypeList<A..., B...> combine(TypeList<A...>, TypeList<B...>);

  template<class... A, class... B, class... C>
  static decltype(combine(TypeList<A..., B...>(), std::declval<C>()...)) combine(TypeList<A...>, TypeList<B...>, C...);

  template<class T, class... A>
  static std::disjunction<std::is_same<T, A>...> typeListContains(TypeList<A...>);

  template<template<class...> class To, class... Args>
  static To<Args...> changeType(TypeList<Args...>);

  //Type list for non type template parameters
  template<auto... Args>
  struct AutoTypeList {
    using TypeList = TypeList<decltype(Args)...>;

    static auto getValues() {
      return std::make_tuple(Args...);
    }
  };
}