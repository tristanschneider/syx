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
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(TypeInfoTests) {
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
      Assert::AreEqual(std::string("fn"), b.getMemberName<0>());
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
      Assert::AreEqual(std::string("b"), c.getMemberName<0>());
      Assert::AreEqual(std::string("c"), c.getMemberName<1>());
      Assert::AreEqual(std::string("f1"), c.getFunctionName<0>());
      Assert::AreEqual(std::string("f2"), c.getMemberName<1>());
    }

    TEST_METHOD(StaticTypeInfoTwo_Visit_VisitsMembers) {
      auto b = StaticTypeInfo<Two>{};
      struct Visitor {
        void operator()(const std::string& name, StaticTypeInfo<int>) const {
          Assert::AreEqual(std::string("b"), name);
          ++mInvocations;
        };
        void operator()(const std::string& name, StaticTypeInfo<bool>) const {
          Assert::AreEqual(std::string("c"), name);
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