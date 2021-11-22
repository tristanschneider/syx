#include "Precompile.h"
#include "CppUnitTest.h"

#include "SparseSet.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(SparseSetTest) {
    using TestPagedVector = PagedVector<int, 10, 0>;

    TEST_METHOD(PagedVector_setValuePage1_hasValue) {
      TestPagedVector v;

      v.set(1, 10);

      const int* result = v.tryGet(1);
      Assert::IsNotNull(result);
      Assert::AreEqual(10, *result);
    }

    TEST_METHOD(PagedVector_setValueHighPage_hasValue) {
      TestPagedVector v;

      v.set(10000, 10);

      const int* result = v.tryGet(10000);
      Assert::IsNotNull(result);
      Assert::AreEqual(10, *result);
    }

    TEST_METHOD(PagedVector_resetValue_valueIsGone) {
      TestPagedVector v;
      v.set(1, 10);

      v.reset(1);

      Assert::IsNull(v.tryGet(1));
    }

    TEST_METHOD(PagedVector_clearMultiplePages_valuesAreGone) {
      TestPagedVector v;
      v.set(1, 10);
      v.set(99, 10);

      v.clear();

      Assert::IsNull(v.tryGet(1));
      Assert::IsNull(v.tryGet(99));
    }

    TEST_METHOD(PagedVector_setExistingValue_IsUpdated) {
      TestPagedVector v;
      v.set(1, 10);

      v.set(1, 99);

      const int* value = v.tryGet(1);
      Assert::IsNotNull(value);
      Assert::AreEqual(99, *value);
    }

    TEST_METHOD(PagedVector_getOrCreateNewValue_IsCreated) {
      TestPagedVector v;

      v.getOrCreate(90) = 10;

      const int* created = v.tryGet(90);
      Assert::IsNotNull(created);
      Assert::AreEqual(10, *created);
    }

    TEST_METHOD(PagedVector_getOrCeateExistingValue_IsUpdated) {
      TestPagedVector v;
      v.set(5, 8);

      v.getOrCreate(5) = 11;

      const int* updated = v.tryGet(5);
      Assert::IsNotNull(updated);
      Assert::AreEqual(11, *updated);
    }

    TEST_METHOD(SparseSetEmpty_getBeginEnd_Match) {
      SparseSet<int> set;

      Assert::IsTrue(set.begin() == set.end());
      Assert::IsFalse(set.begin() != set.end());
    }

    TEST_METHOD(SparseSetSingleValue_DereferenceIt_HasSparseAndPackedId) {
      SparseSet<int> set;
      set.insert(100);

      const SparseValuePair<int> pair = *set.begin();

      Assert::AreEqual(0, pair.mPackedId);
      Assert::AreEqual(100, pair.mSparseId);
    }

    template<class T>
    static void assertSortedContentsMatch(const SparseSet<T>& set, const std::vector<T>& expected) {
      std::vector<T> values;
      std::transform(set.begin(), set.end(), std::back_insert_iterator(values), [](SparseValuePair<int> value) {
        return value.mSparseId;
      });
      std::sort(values.begin(), values.end());
      Assert::IsTrue(expected == values);
    }

    TEST_METHOD(SparseSetTwoValues_Iterate_AllValuesFound) {
      SparseSet<int> set;
      set.insert(10);
      set.insert(99);
      set.insert(3);

      assertSortedContentsMatch(set, { 3, 10, 99 });
    }

    TEST_METHOD(SparseSetOneValue_ForEach_ValueFound) {
      SparseSet<int> set;
      set.insert(10000);
      bool valueFound = false;

      for(SparseValuePair<int> pair : set) {
        Assert::AreEqual(10000, pair.mSparseId);
        Assert::AreEqual(0, pair.mPackedId);
        valueFound = true;
      }

      Assert::IsTrue(valueFound);
    }

    TEST_METHOD(SparseSetOneValue_FindValue_IsFound) {
      SparseSet<int> set;
      set.insert(10);

      auto it = set.find(10);

      Assert::IsFalse(it == set.end());
      Assert::AreEqual(10, it.value().mSparseId);
    }

    TEST_METHOD(SparseSetEmpty_Find_IsEnd) {
      SparseSet<int> set;

      auto it = set.find(0);

      Assert::IsTrue(it == set.end());
    }

    TEST_METHOD(SparseSet_EraseDuringIteration_IsErased) {
      SparseSet<int> set;
      set.insert(1);
      set.insert(10);
      set.insert(1000);
      int i = 0;

      for(auto it = set.begin(); it != set.end(); ++i) {
        if(i == 1) {
          it = set.erase(it);
        }
        else {
          ++it;
        }
      }

      assertSortedContentsMatch(set, { 1, 1000 });
    }

    TEST_METHOD(SparseSet_EraseAtEndOfIteration_IsErased) {
      SparseSet<int> set;
      set.insert(10);
      int i = 0;

      for(auto it = set.begin(); it != set.end(); ++i) {
        it = set.erase(it);
      }

      Assert::AreEqual(i, 1);
      Assert::IsTrue(set.empty());
      Assert::AreEqual(size_t(0), set.size());
    }

    TEST_METHOD(SparseSetTwoValues_FindErase_IsErased) {
      SparseSet<int> set;
      set.insert(9);
      set.insert(33);

      set.erase(set.find(33));

      Assert::IsFalse(set.empty());
      Assert::AreEqual(size_t(1), set.size());
      Assert::AreEqual(9, set.begin().value().mSparseId);
    }

    TEST_METHOD(SparseSetLotsOfValues_Clear_IsCleared) {
      SparseSet<int> set;
      for(int i = 0; i < 10000; ++i) {
        set.insert(i);
      }

      set.clear();

      bool valueFound = false;
      for(SparseValuePair<int> v : set) {
        (void)v;
        valueFound = true;
      }
      Assert::IsFalse(valueFound);
      Assert::IsTrue(set.empty());
    }
  };
}