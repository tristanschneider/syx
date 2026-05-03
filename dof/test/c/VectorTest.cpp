#include <CppUnitTest.h>

extern "C" {
#include <std/Allocator.h>
#include <std/CountingAllocator.h>
#include <std/MallocAllocator.h>
#include <std/Vector.h>
}
#include <c/TestAllocator.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(VectorTest) {
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

    class TestVector {
    public:
      TestVector(size_t elementSize)
        : traits{ std::invoke([&] { std_VectorTraits result = std_Vector_defaultTraits(elementSize);
            result.initialCapacity = 1;
            return result;
          }) }
      {
      }

      ~TestVector() {
        std_Vector_dtor(&ca);
      }

      operator std_VectorCtxA*() {
        return &ca;
      }

      operator std_VectorCtxM*() {
        return &cm;
      }

      operator std_VectorCtxC*() {
        return &cc;
      }

      operator std_Vector*() {
        return &vector;
      }

      const std_VectorTraits& getTraits() const {
        return traits;
      }

    private:
      TestAllocator alloc;
      std_Vector vector{ 0 };
      std_VectorTraits traits{ 0 };
      std_VectorAllocTraits at{
        .traits = &traits,
        .allocator = alloc.get(),
      };
      std_VectorCtxA ca{
        .vector = &vector,
        .traits = &at,
      };
      std_VectorCtxM cm = std_Vector_ctxam(&ca);
      std_VectorCtxC cc = std_Vector_ctxac(&ca);
    };

    TEST_METHOD(VectorBasic) {
      TestVector v(sizeof(int));

      int value = 5;
      std_Vector_pushBack(v, &value);

      Assert::AreEqual(uint32_t(1), std_Vector_size(v));
      Assert::AreEqual(*static_cast<int*>(std_Vector_get(v, 0)), value);

      std_Vector_popBack(v);

      Assert::AreEqual(uint32_t(0), std_Vector_size(v));

      std_Vector_resize(v, 5);
      for(uint32_t i = 0; i < std_Vector_size(v); ++i) {
        *static_cast<int*>(std_Vector_get(v, i)) = static_cast<int>(i);
      }
      {
        std::array values{ 9, 10 };
        std_Vector_insert(v, 1, values.data(), static_cast<uint32_t>(values.size()));
        std::array expected{ 0, 9, 10, 1, 2, 3, 4 };
        Assert::AreEqual(0, std::memcmp(std_Vector_cdata(v), expected.data(), std_Vector_sizeBytes(v)));
      }

      {
        std_Vector_erase(v, uint32_t(3), uint32_t(2));
        std::array expected{ 0, 9, 10, 3, 4 };
        Assert::AreEqual(0, std::memcmp(std_Vector_cdata(v), expected.data(), std_Vector_sizeBytes(v)));
      }

      {
        std_Vector_resize(v, 3);
        std::array expected{ 0, 9, 10 };
        Assert::AreEqual(0, std::memcmp(std_Vector_cdata(v), expected.data(), std_Vector_sizeBytes(v)));

        TestAllocator alloc;
        std_Vector copy = std_Vector_clone(v, alloc.get());
        std_VectorAllocTraits at{
          .traits = &v.getTraits(),
          .allocator = alloc.get(),
        };
        std_VectorCtxA cat{
          .vector = &copy,
          .traits = &at
        };
        std_VectorCtxC cc = std_Vector_ctxac(&cat);
        Assert::AreEqual(0, std::memcmp(std_Vector_cdata(&copy), expected.data(), std_Vector_sizeBytes(&cc)));
        std_Vector_dtor(&cat);
      }

      std_Vector_clear(v);
      Assert::AreEqual(uint32_t(0), std_Vector_size(v));
    }

    TEST_METHOD(Vector_PushBack) {
      std::unordered_map<int, int> asdf;
      [[maybe_unused]] float lf = asdf.max_load_factor();
      TestVector v{ sizeof(int) };

      for(int i = 0; i < 1000; ++i) {
        std_Vector_pushBack(v, &i);
      }
      for(int i = 0; i < 1000; ++i) {
        Assert::AreEqual(i, *static_cast<int*>(std_Vector_get(v, uint32_t(i))));
      }

      std_Vector_erase(v, 0, 1000);
      Assert::AreEqual(uint32_t(0), std_Vector_size(v));
    }
  };
}
