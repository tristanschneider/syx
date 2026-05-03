#include <CppUnitTest.h>

extern "C" {
#include <std/Map.h>
}
#include <c/TestAllocator.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(MapTest) {
    struct VoidMap {
      ~VoidMap() {
        std_VoidMap_dtor(*this, alloc.get());
      }

      operator std_VoidMap*() {
        return &map;
      }

      operator std_Allocator*() {
        return alloc.get();
      }

      std_VoidMapInsertPair insert(int k, int v) {
        return std_VoidMap_insert(*this, static_cast<uint32_t>(k), toValue(v), loadFactor, *this);
      }

      std_VoidMapInsertPair tryInsert(int k, int v) {
        return std_VoidMap_tryInsert(*this, static_cast<uint32_t>(k), toValue(v));
      }

      void reserve(int size) {
        std_VoidMap_reserve(*this, static_cast<size_t>(size), loadFactor, *this);
      }

      void assertEmpty() {
        Assert::IsNull(std_VoidMap_begin(*this));
        Assert::AreEqual(size_t(0), std_VoidMap_size(*this));
      }

      TestAllocator alloc;
      float loadFactor = 0.5f;
      std_VoidMap map{};
    };

    static void* toValue(int v) {
      return (void*)static_cast<size_t>(v);
    }

    static int fromValue(void* v) {
      return static_cast<int>((size_t)v);
    }

    TEST_METHOD(EmptyMap) {
      VoidMap map;
      Assert::IsNull(std_VoidMap_tryInsert(map, 1, nullptr).inserted);
      Assert::IsFalse(std_VoidMap_eraseKey(map, 5));
      std_VoidMap_clear(map);
      Assert::IsNull(std_VoidMap_find(map, 4));
      map.assertEmpty();
    }

    TEST_METHOD(SingleElementMap) {
      VoidMap map;
      std_VoidMapPair* pair = map.insert(1, 2).inserted;
      Assert::IsNotNull(pair);
      Assert::AreEqual(uint32_t(1), pair->key);
      Assert::AreEqual(toValue(2), pair->value);

      Assert::IsTrue(pair == std_VoidMap_find(map, uint32_t(1)));
      std_VoidMapPair* it = std_VoidMap_begin(map);
      Assert::IsTrue(pair == it);
      Assert::IsNull(std_VoidMap_next(it));
      Assert::AreEqual(size_t(1), std_VoidMap_size(map));

      std_VoidMap_eraseKey(map, 1);
      map.assertEmpty();

      pair = map.insert(2, 3).inserted;
      std_VoidMap_clear(map);
      map.assertEmpty();
    }

    TEST_METHOD(FullMap) {
      VoidMap map;
      map.loadFactor = 1;

      map.reserve(8);

      for(int i = 0; i < 8; ++i) {
        Assert::IsNotNull(map.tryInsert(i, -i).inserted);
      }
      Assert::IsNull(map.tryInsert(9, 0).inserted);

      for(int i = 0; i < 8; ++i) {
        std_VoidMapPair* found = std_VoidMap_find(map, static_cast<uint32_t>(i));
        Assert::IsTrue(found && found->value == toValue(-i));
      }

      VoidMap validator;
      std_VoidMapPair* it = std_VoidMap_begin(map);
      while(it) {
        Assert::IsTrue(validator.insert(static_cast<int>(it->key), 0).isNew);
        it = std_VoidMap_next(it);
      }
      Assert::AreEqual(size_t(8), std_VoidMap_size(validator));

      for(int i = 0; i < 8; ++i) {
        Assert::IsTrue(std_VoidMap_eraseKey(map, static_cast<int>(i)));
        Assert::IsNotNull(map.insert(i + 100, 0).inserted);
      }

      std_VoidMap_clear(map);
      map.assertEmpty();
    }
  };
}