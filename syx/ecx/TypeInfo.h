#pragma once

#include "TypeList.h"

//Provides a StaticTypeInfo<T> object containing information about the given type
//Types declare their non-default information by specializing the template and inheriting from StructTypeInfo:
//template<>
//struct StaticTypeInfo<MyType> : public StructTypeInfo<StaticTypeInfo<MyType>, ecx::TypeList<MyMembers...>, ecx::TypeList<MyFunctions...>> {
//  std::array<std::string> MemberNames = { ... };
//  std::array<std::string> FunctionNames = { ... };
//}
namespace ecx {
  //Base type info, this is what is looked up and also what is declared for plain types
  template<class T>
  struct StaticTypeInfo {
    using SelfT = T;
    static constexpr size_t MemberCount = 0;
    static constexpr size_t FunctionCount = 0;
  };

  //StructTypeInfo is the way for multi-member classes to delcare their information
  //This is the unspecialized template that shouldn't be instantiated
  template<class C, class Members, class Functions>
  struct StructTypeInfo {
  };

  template<class C, auto... Members, auto... Functions>
  struct StructTypeInfo<StaticTypeInfo<C>, ecx::AutoTypeList<Members...>, ecx::AutoTypeList<Functions...>> {
    using SelfT = C;
    static constexpr size_t MemberCount = sizeof...(Members);
    static constexpr size_t FunctionCount = sizeof...(Functions);

    using StaticSelfT = StaticTypeInfo<C>;

    template<class T>
    static T memberPointerToMemberType(T C::*);
    template<class T>
    static T memberPointerToMemberType(T);

    using MemberTypeList = ecx::TypeList<StaticTypeInfo<decltype(memberPointerToMemberType(Members))>...>;
    using StaticMemberTypesTuple = decltype(ecx::changeType<std::tuple>(MemberTypeList{}));
    //using MemberPointerTuple = std::tuple

    using MemberTupleT = std::tuple<decltype(Members)...>;
    inline static const MemberTupleT MemberTuple = MemberTupleT{ Members... };

    template<size_t I>
    static const std::string& getMemberName() {
      static_assert(I < MemberCount);
      static_assert(MemberCount == StaticSelfT::MemberNames.size(), "Derived type must have a member names array containing names for all members");
      return StaticSelfT::MemberNames[I];
    }

    template<size_t I>
    static const std::string& getFunctionName() {
      static_assert(I < FunctionCount);
      static_assert(FunctionCount == StaticSelfT::FunctionNames.size(), "Derived type must have a function names array containing names for all functions");
      return StaticSelfT::FunctionNames[I];
    }

    template<size_t I>
    static auto getStaticTypeInfo() {
      return std::tuple_element_t<I, StaticMemberTypesTuple>{};
    }

    template<size_t I>
    static auto getMemberPointer() {
      // For some reason, using a static tuple here works, but rebuilding it as std::make_tuple(Members...) doesn't,
      // resulting in tuple<int, int> when it's supposed to be tuple<int Two::*, int Two::*>
      return std::get<I>(MemberTuple);
    }

    //Visit all direct members, resulting in (const std::string& memberName, StaticTypeInfo<T> memberType)
    template<class Visitor>
    static void visitShallow(const Visitor& visitor) {
      _visitAll(visitor, std::make_index_sequence<MemberCount>());
    }

    //Visit all direct members, resulting in (const std::string& memberName, const T& memberType)
    template<class Visitor>
    static void visitShallow(const Visitor& visitor, const C& instance) {
      _visitAll(visitor, std::make_index_sequence<MemberCount>(), instance);
    }

    template<class Visitor>
    static void visitShallow(const Visitor& visitor, C& instance) {
      _visitAll(visitor, std::make_index_sequence<MemberCount>(), instance);
    }

    template<class Visitor, size_t... Indices>
    static void _visitAll(const Visitor& visitor, std::index_sequence<Indices...>, const C& instance) {
      (_visitOne<Indices>(visitor, instance), ...);
    }

    template<class Visitor, size_t... Indices>
    static void _visitAll(const Visitor& visitor, std::index_sequence<Indices...>, C& instance) {
      (_visitOne<Indices>(visitor, instance), ...);
    }

    template<class Visitor, size_t... Indices>
    static void _visitAll(const Visitor& visitor, std::index_sequence<Indices...>) {
      (_visitOne<Indices>(visitor), ...);
    }

    template<size_t I, class Visitor>
    static void _visitOne(const Visitor& visitor, const C& instance) {
      visitor(getMemberName<I>(), instance.*getMemberPointer<I>());
    }

    template<size_t I, class Visitor>
    static void _visitOne(const Visitor& visitor, C& instance) {
      visitor(getMemberName<I>(), instance.*getMemberPointer<I>());
    }

    template<size_t I, class Visitor>
    static void _visitOne(const Visitor& visitor) {
      visitor(getMemberName<I>(), getStaticTypeInfo<I>());
    }
  };
}