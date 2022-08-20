#include "Precompile.h"
#include "CppUnitTest.h"

#include "BlockVector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(BlockVectorTest) {
    TEST_METHOD(BlockVector_DefaultConstruct_IsEmpty) {
      MemCopyRuntimeTraits traits(sizeof(int));

      BlockVector v(traits);

      Assert::IsTrue(v.empty());
      Assert::AreEqual(size_t(0), v.capacity());
      Assert::AreEqual(size_t(0), v.size());
    }

    TEST_METHOD(BlockVector_Emplace_HasElement) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      int* i = static_cast<int*>(v.emplace_back());
      *i = 5;

      Assert::AreEqual(size_t(1), v.size());
      Assert::IsTrue(v.front() == v.back());
      Assert::IsTrue(v.front() == i);
      Assert::IsTrue(v[0] == i);
      Assert::IsTrue(v.at(0) == i);
      Assert::IsTrue(*v.begin() == i);
    }

    TEST_METHOD(BlockVector_Clear_IsEmpty) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      v.emplace_back();
      v.clear();

      Assert::IsTrue(v.empty());
      Assert::AreEqual(size_t(0), v.size());
    }

    TEST_METHOD(BlockVector_Resize_IsSize) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      v.resize(10);
      Assert::AreEqual(size_t(10), v.size());
      v.resize(1);
      Assert::AreEqual(size_t(1), v.size());
    }

    TEST_METHOD(BlockVector_Reserve_IsCapacity) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      v.reserve(10);
      Assert::AreEqual(size_t(0), v.size());
      Assert::AreEqual(size_t(10), v.capacity());
      v.reserve(1);
      Assert::AreEqual(size_t(0), v.size());
      Assert::AreEqual(size_t(10), v.capacity());
    }

    TEST_METHOD(BlockVector_ShrinkToFit_Shrinks) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      v.reserve(10);
      v.resize(3);
      v.shrink_to_fit();

      Assert::AreEqual(size_t(3), v.capacity());
      Assert::AreEqual(size_t(3), v.size());
    }

    TEST_METHOD(BlockVector_PopBack_IsRemoved) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);

      v.emplace_back();
      v.pop_back();

      Assert::AreEqual(size_t(0), v.size());
    }

    TEST_METHOD(BlockVector_Swap_IsSwapped) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);
      *static_cast<int*>(v.emplace_back()) = 1;
      *static_cast<int*>(v.emplace_back()) = 2;
      BlockVector other(traits);

      v.swap(other);

      Assert::AreEqual(size_t(2), other.size());
      Assert::IsTrue(v.empty());
      Assert::AreEqual(1, *static_cast<int*>(other[0]));
      Assert::AreEqual(2, *static_cast<int*>(other[1]));
    }

    TEST_METHOD(BlockVector_CopyAssign_IsCopied) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);
      *static_cast<int*>(v.emplace_back()) = 1;
      *static_cast<int*>(v.emplace_back()) = 2;
      BlockVector other(traits);

      other = v;

      Assert::AreEqual(size_t(2), other.size());
      Assert::AreEqual(1, *static_cast<int*>(other[0]));
      Assert::AreEqual(2, *static_cast<int*>(other[1]));
      Assert::AreEqual(size_t(2), v.size());
      Assert::AreEqual(1, *static_cast<int*>(v[0]));
      Assert::AreEqual(2, *static_cast<int*>(v[1]));
    }

    TEST_METHOD(BlockVector_MoveAssign_IsMoved) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);
      *static_cast<int*>(v.emplace_back()) = 1;
      *static_cast<int*>(v.emplace_back()) = 2;
      BlockVector other(traits);

      other = std::move(v);

      Assert::AreEqual(size_t(2), other.size());
      Assert::AreEqual(1, *static_cast<int*>(other[0]));
      Assert::AreEqual(2, *static_cast<int*>(other[1]));
    }

    TEST_METHOD(BlockVector_Iterate_HasValues) {
      MemCopyRuntimeTraits traits(sizeof(int));
      BlockVector v(traits);
      *static_cast<int*>(v.emplace_back()) = 1;
      *static_cast<int*>(v.emplace_back()) = 2;
      std::vector<int> found;

      for(void* p : v) {
        found.push_back(*static_cast<int*>(p));
      }

      Assert::IsTrue(std::vector<int>{ 1, 2 } == found);
    }
  };
}