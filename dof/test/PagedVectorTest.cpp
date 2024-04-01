#include "Precompile.h"
#include "CppUnitTest.h"

#include "generics/PagedVector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(PagedVectorTest) {
    struct TinyPageTraits {
      static constexpr size_t PAGE_SIZE = 2;
      static constexpr float GROWTH_FACTOR = 2.0f;
    };

    TEST_METHOD(Basic) {
      gnx::PagedVector<int, TinyPageTraits> v;
      for(size_t i = 0; i < 3; ++i) {
        v.push_back(static_cast<int>(i));
      }
      auto it = v.begin();
      auto cit = v.cbegin();
      for(int i = 0; i < 3; ++i) {
        Assert::AreEqual(i, v[i]);
        Assert::AreEqual(i, *it);
        Assert::AreEqual(i, *cit);
        ++it;
        ++cit;
      }
      Assert::IsTrue(it == v.end());
      Assert::IsTrue(cit == v.cend());
      Assert::AreEqual(static_cast<size_t>(3), v.size());
      Assert::IsFalse(v.empty());

      auto copy{ v };
      for(int i = 0; i < 3; ++i) {
        Assert::AreEqual(i, copy[i]);
      }
      auto moved{ std::move(copy) };
      for(int i = 0; i < 3; ++i) {
        Assert::AreEqual(i, moved[i]);
      }

      v.clear();

      Assert::AreEqual(static_cast<size_t>(0), v.size());
      Assert::IsTrue(v.empty());

      v.resize(10);
      for(int i = 0; i < 10; ++i) {
        v[i] = i;
      }
      v.resize(5);
      for(int i = 0; i < 5; ++i) {
        Assert::AreEqual(i, v[i]);
      }
      auto found = std::find(v.begin(), v.end(), 1);
      auto expected = v.begin();
      ++expected;
      Assert::IsTrue(expected == found);
    }
  };
}