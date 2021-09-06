#include "Precompile.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "util/TypeId.h"

namespace TypeTests {
  TEST_CLASS(TypeTest) {
  public:
    TEST_METHOD(Type_DefaultGen) {
      Assert::IsTrue(typeId<bool>() == typeId<bool>(), L"Same type should have same id", LINE_INFO());
      Assert::IsTrue(typeId<bool>() != typeId<int>(), L"Different types should have different ids", LINE_INFO());
    }

    struct CategoryA {
      DECLARE_TYPE_CATEGORY;
    };

    struct CategoryB {
      DECLARE_TYPE_CATEGORY;
    };

    TEST_METHOD(Type_Category) {
      Assert::AreEqual((uint32_t)typeId<bool, CategoryA>(), (uint32_t)typeId<bool, CategoryB>(), L"Both have different categories, so should generate 0", LINE_INFO());
      Assert::IsTrue(typeId<int, CategoryA>() != typeId<bool, CategoryA>(), L"Different types should have different ids", LINE_INFO());
      Assert::AreEqual((uint32_t)typeId<int, CategoryA>(), (uint32_t)typeId<int, CategoryB>(), L"Both have different categories, so should generate 1", LINE_INFO());
    }

    TEST_METHOD(Type_Map) {
      TypeMap<int> map;
      Assert::IsTrue(map.get<int>() == nullptr);
      map.set<char>(0);
      map.set(typeId<bool>(), 1);
      map[typeId<int>()] = 2;
      Assert::AreEqual(*map.get<char>(), 0, L"Should equal above sets", LINE_INFO());
      Assert::AreEqual(*map.get(typeId<bool>()), 1, L"Should equal above sets", LINE_INFO());
      Assert::AreEqual(map[typeId<int>()], 2, L"Should equal above sets", LINE_INFO());

      map.clear();
      Assert::IsTrue(map.get<char>() == nullptr, L"Values should be cleared from clear", LINE_INFO());
    }
  };
}