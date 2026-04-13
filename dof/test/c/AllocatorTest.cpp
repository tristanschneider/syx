#include <CppUnitTest.h>

extern "C" {
#include <std/Allocator.h>
#include <std/CountingAllocator.h>
#include <std/MallocAllocator.h>
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(AllocatorTest) {
    static void exerciseAllocator(std_Allocator& a) {
      std_CountingAllocator counter{
        .parent = &a
      };
      std_Allocator wrapped = std_CountingAllocator_toAlloc(&counter);

      int* i = (int*)std_Allocator_alloc(&wrapped, sizeof(int));
      Assert::IsNotNull(i);
      *i = 5;
      std_Allocator_dealloc(&wrapped, i);

      Assert::AreEqual(size_t(0), counter.bytesInUse);
    }

    TEST_METHOD(MallocAllocator) {
      std_Allocator a = std_MallocAllocator_ctor();
      exerciseAllocator(a);
    }
  };
}
