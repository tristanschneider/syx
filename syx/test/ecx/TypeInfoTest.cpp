#include "Precompile.h"
#include "CppUnitTest.h"

#include "TypeInfo.h"

struct Basic {
  int a;
  void fn() {};
};

struct Two {
  int a;
  bool b;
  void f1() {};
  bool f2(int) {};
};

struct Empty {
};

struct CustomTagA {};
struct CustomTagB {};

struct BasicTagged {
  int a;
};

struct TaggedSelf {
  struct MyTag {};
  int v;
};

namespace ecx {
  template<>
  struct StaticTypeInfo<Basic> : StructTypeInfo<StaticTypeInfo<Basic>
    , ecx::AutoTypeList<&Basic::a>
    , ecx::AutoTypeList<&Basic::fn>
    > {
    inline static const std::array<std::string, 1> MemberNames = { "a" };
    inline static const std::array<std::string, 1> FunctionNames = { "fn" };
  };

  template<>
  struct StaticTypeInfo<Two> : StructTypeInfo<StaticTypeInfo<Two>
    , ecx::AutoTypeList<&Two::a, &Two::b>
    , ecx::AutoTypeList<&Two::f1, &Two::f2>
    > {
    inline static const std::array<std::string, 2> MemberNames = { "a", "b" };
    inline static const std::array<std::string, 2> FunctionNames = { "f1", "f2" };
  };

  template<>
  struct StaticTypeInfo<Empty> : StructTypeInfo<StaticTypeInfo<Empty>
    , ecx::AutoTypeList<>
    , ecx::AutoTypeList<>
  > {
  };

  template<>
  struct StaticTypeInfo<BasicTagged> : StructTypeInfo<StaticTypeInfo<BasicTagged>
    , ecx::TypeList<ecx::TaggedType<&BasicTagged::a, CustomTagA, CustomTagB>>
    , ecx::TypeList<>
  > {
    inline static const std::array<std::string, 1> MemberNames = { "a" };
  };

  template<>
  struct StaticTypeInfo<TaggedSelf> : StructTypeInfo<StaticTypeInfo<TaggedSelf>
    , ecx::TypeList<ecx::TaggedType<&BasicTagged::a>>
    , ecx::TypeList<>
    , ecx::TypeList<TaggedSelf::MyTag>
  > {
    inline static const std::array<std::string, 1> MemberNames = { "a" };
  };
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  static_assert(!std::is_integral<void>::value);
  static_assert(std::is_integral<int>::value);
  static_assert(!std::is_integral<std::string>::value);
  static_assert(std::is_same_v<TypeList<int>,
    std::conditional_t<std::is_integral<int>::value, TypeList<int>, TypeList<>>>);

  TEST_CLASS(TypeInfoTests) {
    static_assert(std::is_same_v<
      ecx::TypeList<int>,
      decltype(ecx::typeListFilter<std::is_integral>(ecx::TypeList<void, int, std::string>{}))>
    );

    template<class T>
    struct RemapInt {
      using type = T;
    };

    template<>
    struct RemapInt<int> {
      using type = bool;
    };

    static_assert(std::is_same_v<
      ecx::TypeList<bool, bool, std::string>,
      decltype(ecx::typeListTransform<RemapInt>(ecx::TypeList<int, bool, std::string>{}))>);

    static int singleArgByValue(bool) { return 1; }
    static int singleArgByRef(bool&) { return 2; }
    static void voidFn() { }
    static int& singleArgByValueReturnRef(int) { static int i = 0; return i; }
    int memberFn(bool) { return 3; }
    int constMemberFn(bool) const { return 4; }

    static_assert(std::is_same_v<int, ecx::FunctionTypeInfo<&singleArgByValue>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<bool>, ecx::FunctionTypeInfo<&singleArgByValue>::ArgsTypeList>);
    static_assert(!ecx::FunctionTypeInfo<&singleArgByValue>::IsMemberFn);

    static_assert(std::is_same_v<int, ecx::FunctionTypeInfo<&singleArgByRef>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<bool&>, ecx::FunctionTypeInfo<&singleArgByRef>::ArgsTypeList>);
    static_assert(!ecx::FunctionTypeInfo<&singleArgByRef>::IsMemberFn);

    static_assert(std::is_same_v<void, ecx::FunctionTypeInfo<&voidFn>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<>, ecx::FunctionTypeInfo<&voidFn>::ArgsTypeList>);
    static_assert(!ecx::FunctionTypeInfo<&voidFn>::IsMemberFn);

    static_assert(std::is_same_v<int&, ecx::FunctionTypeInfo<&singleArgByValueReturnRef>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<int>, ecx::FunctionTypeInfo<&singleArgByValueReturnRef>::ArgsTypeList>);
    static_assert(!ecx::FunctionTypeInfo<&singleArgByValueReturnRef>::IsMemberFn);

    static_assert(std::is_same_v<int, ecx::FunctionTypeInfo<&TypeInfoTests::memberFn>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<TypeInfoTests*, bool>, ecx::FunctionTypeInfo<&memberFn>::ArgsTypeList>);
    static_assert(ecx::FunctionTypeInfo<&memberFn>::IsMemberFn);

    static_assert(std::is_same_v<int, ecx::FunctionTypeInfo<&TypeInfoTests::constMemberFn>::ReturnT>);
    static_assert(std::is_same_v<ecx::TypeList<const TypeInfoTests*, bool>, ecx::FunctionTypeInfo<&constMemberFn>::ArgsTypeList>);
    static_assert(!ecx::FunctionTypeInfo<&singleArgByValue>::IsMemberFn);

    TEST_METHOD(FunctionInfoTest_InvokeReturnMatches) {
      Assert::AreEqual(1, ecx::FunctionTypeInfo<&singleArgByValue>::InfoT::invoke(true));
      bool bref = false;
      Assert::AreEqual(2, ecx::FunctionTypeInfo<&singleArgByRef>::InfoT::invoke(bref));
      ecx::FunctionTypeInfo<&voidFn>::InfoT::invoke();
      int& iref = ecx::FunctionTypeInfo<&singleArgByValueReturnRef>::InfoT::invoke(1);
      Assert::AreEqual(0, iref);
      Assert::AreEqual(3, ecx::FunctionTypeInfo<&TypeInfoTests::memberFn>::InfoT::invoke(this, true));
      const TypeInfoTests* constSelf = this;
      Assert::AreEqual(4, ecx::FunctionTypeInfo<&TypeInfoTests::constMemberFn>::InfoT::invoke(constSelf, true));
    }

    TEST_METHOD(StaticTypeInfoEmpty_InfoMatches) {
      using Info = StaticTypeInfo<Empty>;

      static_assert(Info::MemberCount == 0);
      static_assert(Info::FunctionCount == 0);
    }

    TEST_METHOD(StaticTypeInfoInt_InfoMatches) {
      auto a = StaticTypeInfo<int>{};

      static_assert(decltype(a)::MemberCount == 0);
      static_assert(decltype(a)::FunctionCount == 0);
    }

    TEST_METHOD(StaticTypeInfoBasic_InfoMatches) {
      auto b = StaticTypeInfo<Basic>{};

      static_assert(decltype(b)::MemberCount == 1);
      static_assert(decltype(b)::FunctionCount == 1);
      static_assert(std::is_same_v<ecx::TypeList<StaticTypeInfo<int>>, decltype(b)::MemberTypeList>);
      Assert::AreEqual(std::string("a"), b.getMemberName<0>());
      Assert::AreEqual(std::string("fn"), b.getFunctionName<0>());

      Basic self;
      auto info = decltype(b)::getFunctionInfo<0>();
      info.invoker().invoke(&self);
      static_assert(std::is_same_v<void, decltype(info)::ReturnT>);
      static_assert(std::is_same_v<ecx::TypeList<Basic*>, decltype(info)::ArgsTypeList>);
    }

    TEST_METHOD(StaticTypeInfoBasicTagged_InfoMatches) {
      auto b = StaticTypeInfo<BasicTagged>{};

      static_assert(decltype(b)::MemberCount == 1);
      static_assert(decltype(b)::FunctionCount == 0);
      static_assert(std::is_same_v<ecx::TypeList<StaticTypeInfo<int>>, decltype(b)::MemberTypeList>);
      static_assert(decltype(b)::memberHasTags<0, CustomTagA, CustomTagB>());
      static_assert(!decltype(b)::memberHasTags<0, int>());
      static_assert(std::is_same_v<StaticTypeInfo<int>, decltype(decltype(b)::getStaticTypeInfo<0>())>);
      Assert::AreEqual(std::string("a"), b.getMemberName<0>());
    }

    TEST_METHOD(StaticTypeInfoSelfTagged_InfoMatches) {
      using Info = StaticTypeInfo<TaggedSelf>;
      static_assert(Info::HasTagsT<TaggedSelf::MyTag>::value);
      static_assert(!Info::HasTagsT<int>::value);
      static_assert(Info::hasTags<TaggedSelf::MyTag>());
      static_assert(!Info::hasTags<int>());
    }

    TEST_METHOD(StaticTypeInfoBasic_Visit_VisitsMember) {
      auto b = StaticTypeInfo<Basic>{};
      struct Visitor {
        void operator()(const std::string& name, StaticTypeInfo<int>) const {
          Assert::AreEqual(std::string("a"), name);
          ++mInvocations;
        };
        mutable int mInvocations = 0;
      };
      Visitor visitor;

      b.visitShallow(visitor);

      Assert::AreEqual(1, visitor.mInvocations);
    }

    TEST_METHOD(StaticTypeInfoBasic_VisitInstance_VisitsMember) {
      auto b = StaticTypeInfo<Basic>{};
      struct Visitor {
        void operator()(const std::string& name, const int& i) const {
          Assert::AreEqual(std::string("a"), name);
          Assert::AreEqual(1, i);
          ++mInvocations;
        };
        mutable int mInvocations = 0;
      };
      Visitor visitor;

      b.visitShallow(visitor, Basic{ 1 });

      Assert::AreEqual(1, visitor.mInvocations);
    }

    TEST_METHOD(StaticTypeInfoTwo_InfoMatches) {
      auto c = StaticTypeInfo<Two>{};

      static_assert(decltype(c)::MemberCount == 2);
      static_assert(decltype(c)::FunctionCount == 2);
      Assert::AreEqual(std::string("a"), c.getMemberName<0>());
      Assert::AreEqual(std::string("b"), c.getMemberName<1>());
      Assert::AreEqual(std::string("f1"), c.getFunctionName<0>());
      Assert::AreEqual(std::string("f2"), c.getFunctionName<1>());
    }

    TEST_METHOD(StaticTypeInfoTwo_Visit_VisitsMembers) {
      auto b = StaticTypeInfo<Two>{};
      struct Visitor {
        void operator()(const std::string& name, StaticTypeInfo<int>) const {
          Assert::AreEqual(std::string("a"), name);
          ++mInvocations;
        };
        void operator()(const std::string& name, StaticTypeInfo<bool>) const {
          Assert::AreEqual(std::string("b"), name);
          ++mInvocations;
        };
        mutable int mInvocations = 0;
      };
      Visitor visitor;

      b.visitShallow(visitor);

      Assert::AreEqual(2, visitor.mInvocations);
    }
  };
}