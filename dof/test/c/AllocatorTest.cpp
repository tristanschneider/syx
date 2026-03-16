#include <CppUnitTest.h>

extern "C" {
#include <std/Allocator.h>
#include <std/MallocAllocator.h>
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(AllocatorTest) {
    static void exerciseAllocator(std_Allocator& a) {
      int* i = (int*)std_Allocator_alloc(&a, sizeof(int));
      Assert::IsNotNull(i);
      *i = 5;
      std_Allocator_dealloc(&a, i);
    }

    TEST_METHOD(MallocAllocator) {
      std_Allocator a = std_MallocAllocator_ctor();
      exerciseAllocator(a);
    }
  };
}
