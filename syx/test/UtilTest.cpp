#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include <atomic>
#include <algorithm>
#include "util/ScopeWrap.h"

namespace UtilTests {
  TEST_CLASS(ScopeWrapTests) {
  struct StateTester {
    StateTester()
      : mCount(0) {
    }

    void push() {
      ++mCount;
    }

    void pop() {
      --mCount;
    }

    int mCount;

    using Scope = ScopeWrapType(StateTester, push, pop);
  };

public:
    TEST_METHOD(ScopeWrap_Basic) {
      StateTester state;
      {
        StateTester::Scope scope(state);
        Assert::AreEqual(state.mCount, 1, L"Within scope, count should be 1", LINE_INFO());
      }
      Assert::AreEqual(state.mCount, 0, L"Out of scope, count should be back to 0", LINE_INFO());
    }

    TEST_METHOD(ScopeWrap_MoveConstruct) {
      StateTester state;
      {
        StateTester::Scope scope(state);
        Assert::AreEqual(state.mCount, 1, L"Within scope, count should be 1", LINE_INFO());
        {
          StateTester::Scope movedScope(std::move(scope));
          Assert::AreEqual(state.mCount, 1, L"Within scope, count should be 1", LINE_INFO());
        }
        Assert::AreEqual(state.mCount, 0, L"Out of scope, count should be back to 0", LINE_INFO());
      }
      Assert::AreEqual(state.mCount, 0, L"Out of scope, count should be back to 0", LINE_INFO());
    }
  };
}