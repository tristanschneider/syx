#include "Precompile.h"
#include "CppUnitTest.h"

#include "AnyType.h"
#include "AnyTuple.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(AnyTest) {
    TEST_METHOD(AnyTypeEmpty_empty_True) {
      AnyType any;

      Assert::IsTrue(any.empty());
      Assert::IsFalse(any);
    }

    TEST_METHOD(AnyTypeTwoEmpty_operatorEq_True) {
      AnyType a, b;

      Assert::IsTrue(a == b);
      Assert::IsFalse(a != b);
    }

    TEST_METHOD(AnyTypeInt_get_HasValue) {
      AnyType any = AnyType::create<int>(5);

      int* value = any.tryGet<int>();
      Assert::IsNotNull(value);
      Assert::AreEqual(5, *value);
      Assert::AreEqual(5, any.get<int>());
      Assert::IsFalse(any.empty());
    }

    TEST_METHOD(AnyTypeInt_tryGetString_NoValue) {
      AnyType any = AnyType::create<int>(1);

      Assert::IsNull(any.tryGet<std::string>());
    }

    TEST_METHOD(AnyTypeInt_copyConstruct_HasValue) {
      AnyType a = AnyType::create<int>(1);
      AnyType b(a);

      Assert::AreEqual(1, b.get<int>());
    }

    TEST_METHOD(AnyTypeInt_moveConstruct_HasValue) {
      AnyType a = AnyType::create<int>(1);
      AnyType b(std::move(a));

      Assert::AreEqual(1, b.get<int>());
      Assert::IsTrue(a.empty());
    }

    TEST_METHOD(AnyTypeEmpty_copyAsign_HasValue) {
      AnyType a;
      AnyType b = AnyType::create<int>(1);

      a = b;

      Assert::AreEqual(1, a.get<int>());
    }

    TEST_METHOD(AnyTypeInt32_copyAssignInt8_HasValue) {
      AnyType a = AnyType::create<int32_t>(1);
      AnyType b = AnyType::create<int8_t>(int8_t(2));

      a = b;

      Assert::AreEqual(int8_t(2), a.get<int8_t>());
    }

    TEST_METHOD(AnyTypeInt32_copyAssignInt64_HasValue) {
      AnyType a = AnyType::create<int32_t>(1);
      AnyType b = AnyType::create<int64_t>(2);

      a = b;

      Assert::AreEqual(int64_t(2), a.get<int64_t>());
    }

    TEST_METHOD(AnyTypeInt_selfAssign_HasValue) {
      AnyType a = AnyType::create<int>(1);

      a = a;

      Assert::AreEqual(1, a.get<int>());
    }

    TEST_METHOD(AnyTypeEmpty_copyAssign_HasValue) {
      AnyType a;
      AnyType b = AnyType::create<int>(1);

      a = b;

      Assert::AreEqual(1, a.get<int>());
    }

    TEST_METHOD(AnyTypeEmpty_copyAssignEmpty_IsEmpty) {
      AnyType a, b;

      a = b;

      Assert::IsTrue(a.empty());
    }

    TEST_METHOD(AnyTypeInt_copyAssignEmpty_IsEmpty) {
      AnyType a = AnyType::create<int>(1);
      AnyType b;

      a = b;

      Assert::IsTrue(a.empty());
    }

    TEST_METHOD(AnyTypeInt_SelfMoveAssign_HasValue) {
      AnyType a = AnyType::create<int>(1);

      a = std::move(a);

      Assert::AreEqual(1, a.get<int>());
    }

    TEST_METHOD(AnyTypeInt_moveAssign_HasValue) {
      AnyType a = AnyType::create<int>(1);

      a = AnyType::AnyType::create<bool>(true);

      Assert::IsTrue(a.get<bool>());
    }

    TEST_METHOD(AnyTypeEmpty_emplace_HasValue) {
      AnyType a;

      a.emplace<int>(1);

      Assert::AreEqual(1, a.get<int>());
    }

    TEST_METHOD(AnyTypeInt32_emplaceInt8_HasValue) {
      AnyType a = AnyType::create<int32_t>(1);

      a.emplace<int8_t>(int8_t(2));

      Assert::AreEqual(int8_t(2), a.get<int8_t>());
    }

    TEST_METHOD(AnyTypeInt32_emplaceInt64_HasValue) {
      AnyType a = AnyType::create<int32_t>(1);

      a.emplace<int64_t>(2);

      Assert::AreEqual(int64_t(2), a.get<int64_t>());
    }

    TEST_METHOD(AnyTypeInt_CompareSameInt_AreSame) {
      AnyType a = AnyType::create<int>(1);
      AnyType b = AnyType::create<int>(1);

      Assert::IsTrue(a == b);
      Assert::IsFalse(a != b);
    }

    TEST_METHOD(AnyTypeInt_CompareDifferentInt_AreDifferent) {
      AnyType a = AnyType::create<int>(1);
      AnyType b = AnyType::create<int>(2);

      Assert::IsTrue(a != b);
      Assert::IsFalse(a == b);
    }

    TEST_METHOD(AnyTypeEmpty_CompareEmpty_AreSame) {
      Assert::IsTrue(AnyType() == AnyType());
      Assert::IsFalse(AnyType() != AnyType());
    }

    TEST_METHOD(AnyTypeEmpty_CompareNonEmpty_AreDifferent) {
      AnyType a;
      AnyType b = AnyType::create<int>(1);

      Assert::IsTrue(a != b);
      Assert::IsFalse(a == b);
    }

    TEST_METHOD(AnyTypeInt_CompareString_AreDifferent) {
      AnyType a = AnyType::create<int>(1);
      AnyType b = AnyType::create<std::string>("test");

      Assert::IsFalse(a == b);
      Assert::IsTrue(a != b);
    }

    TEST_METHOD(AnyTypeTracking_Destroy_DestructorCalledOnce) {
      struct Tracker {
        ~Tracker() {
          if(mCalls) {
            ++(*mCalls);
          }
        }

        int* mCalls = nullptr;
      };
      int calls = 0;

      {
        AnyType a;
        a.emplace<Tracker>(&calls);
      }

      Assert::AreEqual(1, calls);
    }

    struct TestCategory {};
    using TestAnyTuple = AnyTuple<TestCategory>;

    TEST_METHOD(AnyTuple_emplaceValues_HasValues) {
      TestAnyTuple t;
      t.emplace<int>(1);
      t.emplace<std::string>("test");

      Assert::AreEqual(1, t.getOrCreate<int>());
      Assert::AreEqual(std::string("test"), t.getOrCreate<std::string>());
    }
  };
}