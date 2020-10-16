#include "Precompile.h"
#include "CppUnitTest.h"

#include "util/FunctionTraits.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UtilTests {
  void testFn(int) {
  }

  struct TypeTestClass {
    void testFn(int) {
    }

    void cTestFn(int) const {
    }
  };

  template<class Func>
  void testTemplate(Func) {
    static_assert(std::is_same_v<typename FunctionTraits<Func>::template argument<0>::type, int>, "Should deduce as int");
    static_assert(std::is_same_v<typename FunctionTraits<Func>::return_type, void>, "Should deduce return type as void");
    static_assert(FunctionTraits<Func>::num_args == 1, "Should deduce that function takes one argument");
  }

  TEST_CLASS(FunctionTraitsTests) {
    TEST_METHOD(ArgType_Function) {
      static_assert(std::is_same_v<FunctionTraits<decltype(&testFn)>::argument<0>::type, int>, "Should deduce argument as int");
      static_assert(std::is_same_v<FunctionTraits<decltype(&testFn)>::return_type, void>, "Should deduce return as void");
      static_assert(FunctionTraits<decltype(&testFn)>::num_args == 1, "Should deduce that function takes one argument");
    }

    TEST_METHOD(FunctionTraits_MemberFunction) {
      static_assert(std::is_same_v<FunctionTraits<decltype(&TypeTestClass::testFn)>::argument<0>::type, int>, "Should deduce argument as int");
      static_assert(std::is_same_v<FunctionTraits<decltype(&TypeTestClass::testFn)>::return_type, void>, "Should deduce return as void");
      static_assert(FunctionTraits<decltype(&TypeTestClass::testFn)>::num_args == 1, "Should deduce that function takes one argument");
    }

    TEST_METHOD(FunctionTraits_ConstMemberFunction) {
      static_assert(std::is_same_v<FunctionTraits<decltype(&TypeTestClass::cTestFn)>::argument<0>::type, int>, "Should deduce argument as int");
      static_assert(std::is_same_v<FunctionTraits<decltype(&TypeTestClass::cTestFn)>::return_type, void>, "Should deduce return as void");
      static_assert(FunctionTraits<decltype(&TypeTestClass::cTestFn)>::num_args == 1, "Should deduce that function takes one argument");
    }

    TEST_METHOD(FunctionTraits_StdFunction) {
      static_assert(std::is_same_v<FunctionTraits<std::function<void(int)>>::argument<0>::type, int>, "Should deduce argument as int");
      static_assert(std::is_same_v<FunctionTraits<std::function<void(int)>>::return_type, void>, "Should deduce return as void");
      static_assert(FunctionTraits<std::function<void(int)>>::num_args == 1, "Should deduce that function takes one argument");
    }

    TEST_METHOD(FunctionTraits_Lambda) {
      auto fn = [](int) {};
      static_assert(std::is_same_v<FunctionTraits<decltype(fn)>::argument<0>::type, int>, "Should deduce argument as int");
      static_assert(std::is_same_v<FunctionTraits<decltype(fn)>::return_type, void>, "Should deduce return as void");
      static_assert(FunctionTraits<decltype(fn)>::num_args == 1, "Should deduce that function takes one argument");
    }

    TEST_METHOD(FunctionTraits_UseInTemplate) {
      testTemplate([](int){});
    }
  };
}