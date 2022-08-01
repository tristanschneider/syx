#include "Precompile.h"
#include "CppUnitTest.h"

#include "BlockVector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(BlockVectorTest) {
    TEST_METHOD(BlockVector_DefaultConstruct_IsEmpty) {
      MemCopyTraits traits(sizeof(int));

      BlockVector v(traits);

      Assert::IsTrue(v.empty());
      Assert::AreEqual(size_t(0), v.capacity());
      Assert::AreEqual(size_t(0), v.size());
    }

    TEST_METHOD(BlockVector_Emplace_HasElement) {
      MemCopyTraits traits(sizeof(int));
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
      MemCopyTraits traits(sizeof(int));
      BlockVector v(traits);

      v.emplace_back();
      v.clear();

      Assert::IsTrue(v.empty());
      Assert::AreEqual(size_t(0), v.size());
    }
  };
}