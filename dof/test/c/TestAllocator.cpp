#include <c/TestAllocator.h>

#include <CppUnitTest.h>
extern "C" {
#include <std/CountingAllocator.h>
#include <std/MallocAllocator.h>
}

struct TestAllocator::Impl {
  ~Impl() {
    using namespace Microsoft::VisualStudio::CppUnitTestFramework;
    Assert::AreEqual(size_t(0), counting.bytesInUse, L"Test should end with no used bytes");
  }

  std_Allocator base = std_MallocAllocator_ctor();
  std_CountingAllocator counting{
    .parent = &base
  };
  std_Allocator wrapped = std_CountingAllocator_toAlloc(&counting);
};

TestAllocator::TestAllocator()
  : impl{ std::make_unique<Impl>() } {
}

TestAllocator::~TestAllocator() = default;

std_Allocator* TestAllocator::get() {
  return &impl->wrapped;
}
