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
  template<class C, class Members, class Functions, class Tags = ecx::TypeList<>>
  struct StructTypeInfo {
  };

  template<auto MemberT, class... TagsT>
  struct TaggedType {
    static inline constexpr auto Member = MemberT;
    using TagList = ecx::TypeList<TagsT...>;
  };

  template<auto Fn>
  struct FunctionTypeInfo {
    template<class Ret, class... Args>
    struct FreeFunctionInfo {
      using ReturnT = Ret;
      using ArgsList = ecx::TypeList<Args...>;
      using IsMemberFn = std::false_type;

      static Ret invoke(Args&&... args) {
        return Fn(std::forward<Args>(args)...);
      }
    };
    template<class Ret, class Class, class... Args>
    struct MemberFunctionInfo {
      using ReturnT = Ret;
      using ArgsList = ecx::TypeList<Class*, Args...>;
      using IsMemberFn = std::true_type;

      static Ret invoke(Class* self, Args&&... args) {
        return (self->*Fn)(std::forward<Args>(args)...);
      }
    };
    template<class Ret, class Class, class... Args>
    struct ConstMemberFunctionInfo {
      using ReturnT = Ret;
      using ArgsList = ecx::TypeList<const Class*, Args...>;
      using IsMemberFn = std::true_type;

      static Ret invoke(const Class* self, Args&&... args) {
        return (self->*Fn)(std::forward<Args>(args)...);
      }
    };

    template<class Ret, class... Args>
    static FreeFunctionInfo<Ret, Args...> _deduceFnPointer(Ret(*)(Args...));
    template<class Ret, class Class, class... Args>
    static MemberFunctionInfo<Ret, Class, Args...> _deduceFnPointer(Ret(Class::*)(Args...));
    template<class Ret, class Class, class... Args>
    static ConstMemberFunctionInfo<Ret, Class, Args...> _deduceFnPointer(Ret(Class::* fn)(Args...) const);

    using InfoT = decltype(_deduceFnPointer(Fn));
    using ReturnT = typename InfoT::ReturnT;
    using ArgsTypeList = typename InfoT::ArgsList;
    static inline constexpr bool IsMemberFn = typename InfoT::IsMemberFn::value;

    //Allows info.invoker().invoke() syntax
    static InfoT invoker() {
      return InfoT{};
    }
  };

  //Type lists of TaggedType<&C::mMember, TagA, TagB> or
  //AutoTypeList<&C::mMember> if no tags are desired
  //Where tags indicate desired metadata, like serialization, public vs private, or whatever else
  template<class C, class Members, class Functions, class MemberTags, class FunctionTags, class SelfTags>
  struct StructTypeInfoImpl {
    using SelfT = C;
    using TagsList = SelfTags;
    static constexpr size_t MemberCount = Members::Size;
    static constexpr size_t FunctionCount = Functions::Size;

    using StaticSelfT = StaticTypeInfo<C>;

    template<class T>
    static T memberPointerToMemberType(T C::*);
    template<class T>
    static T memberPointerToMemberType(T);

    template<auto... Fns>
    using FunctionInfoTupleT = std::tuple<FunctionTypeInfo<Fns>...>;

    template<auto... ToApply>
    using MemberTypeListTemplate = ecx::TypeList<StaticTypeInfo<decltype(memberPointerToMemberType(ToApply))>...>;
    using MemberTypeList = decltype(ecx::changeType<MemberTypeListTemplate>(Members{}));
    using StaticMemberTypesTuple = decltype(ecx::changeType<std::tuple>(MemberTypeList{}));
    inline static const decltype(ecx::changeType<FunctionInfoTupleT>(Functions{})) FunctionInfoTuple;

    template<auto... ToApply>
    using MemberTupleTemplate = std::tuple<decltype(ToApply)...>;
    using MemberTupleT = decltype(ecx::changeType<MemberTupleTemplate>(Members{}));
    inline static const MemberTupleT MemberTuple = ecx::changeType<MemberTupleT>(Members{});

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

    static const std::string& getTypeName() {
      static_assert(std::is_same_v<std::string, std::decay_t<decltype(StaticSelfT::SelfName)>>, "Derived type should have a self identifier (SelfName)");
      return StaticSelfT::SelfName;
    }

    template<size_t I>
    static constexpr auto getStaticTypeInfo() {
      return std::tuple_element_t<I, StaticMemberTypesTuple>{};
    }

    template<size_t I>
    static constexpr auto getFunctionInfo() {
      return std::tuple_element_t<I, decltype(FunctionInfoTuple)>{};
    }

    static constexpr auto getFunctionCount() {
      return std::tuple_size_v<decltype(FunctionInfoTuple)>;
    }

    template<size_t I, class... Tags>
    static constexpr bool memberHasTags() {
      // Turn type list of type lists into tuple of type lists
      using TupleT = decltype(changeType<std::tuple>(MemberTags{}));
      // See if the type list at the member's index contains the types
      return (decltype(ecx::typeListContains<Tags>(std::tuple_element_t<I, TupleT>{}))::value && ...);
    }

    //For some reason this works in enable_if when calling hasTags doesn't
    template<class... Tags>
    using HasTagsT = std::conjunction<decltype(ecx::typeListContains<Tags>(SelfTags{}))...>;

    template<class... Tags>
    static constexpr bool hasTags() {
      return (decltype(ecx::typeListContains<Tags>(SelfTags{}))::value && ...);
    }

    template<size_t I>
    static auto getMemberPointer() {
      //For some reason, using a static tuple here works, but rebuilding it as std::make_tuple(Members...) doesn't,
      //resulting in tuple<int, int> when it's supposed to be tuple<int Two::*, int Two::*>
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

  //Default if no tags are provided : StructTypeInfo<T, ecx::AutoTypeList<&T::mValue>, ecx::AutoTypeList<&T::func>>
  template<class C, auto... Members, auto... Functions>
  struct StructTypeInfo<StaticTypeInfo<C>, ecx::AutoTypeList<Members...>, ecx::AutoTypeList<Functions...>, ecx::TypeList<>>
    : StructTypeInfoImpl<C, ecx::AutoTypeList<Members...>, ecx::AutoTypeList<Functions...>, ecx::TypeList<>, ecx::TypeList<>, ecx::TypeList<>> {
  };

  //Tagged type form : StructTypeInfo<T, ecx::TypeList<ecx::TaggedType<&T::mValue, TagA>>, ecx::TypeList<ecx::TaggedType<&T::func, TagB>>>
  template<class C, class... TaggedMembers, class... TaggedFunctions>
  struct StructTypeInfo<StaticTypeInfo<C>, ecx::TypeList<TaggedMembers...>, ecx::TypeList<TaggedFunctions...>, ecx::TypeList<>>
    : StructTypeInfoImpl<C, ecx::AutoTypeList<TaggedMembers::Member...>, ecx::AutoTypeList<TaggedFunctions::Member...>,
    ecx::TypeList<typename TaggedMembers::TagList...>, ecx::TypeList<typename TaggedFunctions::TagList...>, ecx::TypeList<>> {
  };

  //Same as above with self tags
  template<class C, class... TaggedMembers, class... TaggedFunctions, class... SelfTags>
  struct StructTypeInfo<StaticTypeInfo<C>, ecx::TypeList<TaggedMembers...>, ecx::TypeList<TaggedFunctions...>, ecx::TypeList<SelfTags...>>
    : StructTypeInfoImpl<C, ecx::AutoTypeList<TaggedMembers::Member...>, ecx::AutoTypeList<TaggedFunctions::Member...>,
    ecx::TypeList<typename TaggedMembers::TagList...>, ecx::TypeList<typename TaggedFunctions::TagList...>,
    ecx::TypeList<SelfTags...>> {
  };
}