#include "Precompile.h"
#include "CppUnitTest.h"

#include "Database.h"
#include "RuntimeDatabase.h"
#include "SparseRow.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(SparseRowTest) {
    template<class T>
    static RuntimeDatabase createDatabase() {
      RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
      DBReflect::addDatabase<T>(args);
      return RuntimeDatabase{ std::move(args) };
    }

    TEST_METHOD(PackedIndexArrayBasic) {
      PackedIndexArray a;
      for(size_t s = 0; s < 10; ++s) {
        const size_t size = a.size() ? a.size() * 2 : 3;
        a.resize(size, size);

        const size_t b = 0;
        const size_t e = size - 1;
        const size_t m = e / 2;

        a[b] = 1;
        a[m] = 2;
        a[e] = 3;

        const PackedIndexArray& ca = a;
        Assert::AreEqual(PackedIndexArray::IndexBase{ 1 }, *ca[b]);
        Assert::AreEqual(PackedIndexArray::IndexBase{ 2 }, *ca[m]);
        Assert::AreEqual(PackedIndexArray::IndexBase{ 3 }, *ca[e]);

        Assert::IsTrue(std::find(a.begin(), a.end(), PackedIndexArray::IndexBase{ 1 }) != a.end());
        Assert::IsTrue(std::find(a.begin(), a.end(), PackedIndexArray::IndexBase{ 2 }) != a.end());
        Assert::IsTrue(std::find(a.begin(), a.end(), PackedIndexArray::IndexBase{ 3 }) != a.end());
      }
    }

    TEST_METHOD(SparseRowBasic) {
      RuntimeDatabase db = createDatabase<Database<
        Table<SparseRow<int>>,
        Table<SparseRow<int>>
      >>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];

      a.resize(100);

      auto ra = a.tryGet<SparseRow<int>>();
      auto rb = b.tryGet<SparseRow<int>>();
      for(size_t i = 0; i < 100; ++i) {
        Assert::IsFalse(ra->contains(i));
      }

      ra->getOrAdd(5) = 50;
      {
        auto it = ra->find(5);
        Assert::IsFalse(it == ra->end());
        Assert::AreEqual(50, it.value());
        Assert::AreEqual(static_cast<size_t>(5), it.key());
      }

      ra->getOrAdd(5) = 5;
      Assert::AreEqual(size_t(1), ra->size());
      Assert::AreEqual(5, ra->getOrAdd(5));

      ra->getOrAdd(5) = 10;
      Assert::AreEqual(10, ra->getOrAdd(5));
      Assert::AreEqual(size_t(1), ra->size());

      a.addElements(1);
      {
        auto it = ra->find(5);
        Assert::IsTrue(it != ra->end());
      }

      ra->erase(5);
      Assert::AreEqual(size_t(0), ra->size());

      for(size_t i = 0; i < a.size(); ++i) {
        ra->getOrAdd(i) = static_cast<int>(i);
      }

      {
        int i = 0;
        for(auto&& [k, v] : *ra) {
          Assert::AreEqual(static_cast<size_t>(i), k);
          Assert::AreEqual(i, v);
          ++i;
        }
        Assert::AreEqual(101, i);
        Assert::AreEqual(size_t(101), ra->size());
      }

      for(size_t i = 0; i < a.size(); ++i) {
        auto it = ra->find(i);

        Assert::IsTrue(it != ra->end());
        Assert::AreEqual(i, it.key());
        Assert::AreEqual(static_cast<int>(i), it.value());

        ra->erase(i);

        it = ra->find(i);
        Assert::IsTrue(it == ra->end());
      }

      ra->getOrAdd(50) = 3;
      a.resize(50);
      Assert::AreEqual(size_t(0), ra->size());

      const size_t migrateBegin = 25;
      const size_t migrateCount = a.size() - migrateBegin;
      for(size_t i = migrateBegin; i < a.size(); ++i) {
        ra->getOrAdd(i) = static_cast<int>(i);
      }
      RuntimeTable::migrate(migrateBegin, a, b, migrateCount);

      Assert::AreEqual(migrateCount, rb->size());
      for(size_t i = 0; i < migrateCount; ++i) {
        auto it = rb->find(i);
        Assert::IsTrue(it != rb->end());
        Assert::AreEqual(it.key(), i);
        //Since this is the second half of the table, the expected values are offset by migrate count
        Assert::AreEqual(static_cast<int>(i + migrateCount), it.value());
      }

      //All sparse elements that had values were moved to B, leaving nothing in A
      Assert::AreEqual(size_t(0), ra->size());

      //rb should have the moved elements whose value matches the index offset by the number of remaining elements in ra
      {
        PackedIndexArray visited;
        visited.resize(migrateCount, migrateCount);
        for(auto it = rb->begin(); it != rb->end(); ++it) {
          visited.at(it.key()) = it.value();
        }
        for(size_t i = 0; i < visited.size(); ++i) {
          Assert::AreEqual(i + migrateCount, *visited.at(i));
        }
      }

      a.resize(0);
      b.resize(0);

      a.resize(10);
      RuntimeTable::migrate(0, a, b, 10);

      Assert::AreEqual(static_cast<size_t>(0), rb->size());

      a.resize(10);
      b.resize(10);
      ra->getOrAdd(0) = 1;
      ra->getOrAdd(2) = 2;
      ra->getOrAdd(3) = 3;

      RuntimeTable::migrate(0, a, b, 5);

      Assert::IsFalse(ra->contains(0));
      Assert::IsFalse(ra->contains(2));
      Assert::IsFalse(ra->contains(3));

      Assert::AreEqual(size_t(15), b.size());
      Assert::AreEqual(size_t(3), rb->size());
      {
        auto it = rb->find(10);
        Assert::IsTrue(it != rb->end() && it.key() == 10 && it.value() == 1);
        it = rb->find(12);
        Assert::IsTrue(it != rb->end() && it.key() == 12 && it.value() == 2);
        it = rb->find(13);
        Assert::IsTrue(it != rb->end() && it.key() == 13 && it.value() == 3);
      }
    }

    TEST_METHOD(SparseFlagRowBasic) {
      RuntimeDatabase db = createDatabase<Database<
        Table<SparseFlagRow>,
        Table<SparseFlagRow>
      >>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];

      a.resize(100);

      auto ra = a.tryGet<SparseFlagRow>();
      auto rb = b.tryGet<SparseFlagRow>();
      for(size_t i = 0; i < 100; ++i) {
        Assert::IsFalse(ra->contains(i));
      }

      ra->getOrAdd(5);
      {
        auto it = ra->find(5);
        Assert::IsFalse(it == ra->end());
        Assert::AreEqual(static_cast<size_t>(5), *it);
      }

      ra->getOrAdd(5);
      Assert::AreEqual(size_t(1), ra->size());
      Assert::IsTrue(ra->contains(5));

      a.addElements(1);
      {
        auto it = ra->find(5);
        Assert::IsTrue(it != ra->end());
      }

      ra->erase(5);
      Assert::AreEqual(size_t(0), ra->size());

      for(size_t i = 0; i < a.size(); ++i) {
        ra->getOrAdd(i);
      }

      {
        int i = 0;
        for(auto&& k : *ra) {
          Assert::AreEqual(static_cast<size_t>(i), k);
          ++i;
        }
        Assert::AreEqual(101, i);
        Assert::AreEqual(size_t(101), ra->size());
      }

      for(size_t i = 0; i < a.size(); ++i) {
        auto it = ra->find(i);

        Assert::IsTrue(it != ra->end());
        Assert::AreEqual(i, *it);

        ra->erase(i);

        it = ra->find(i);
        Assert::IsTrue(it == ra->end());
      }

      ra->getOrAdd(50);
      a.resize(50);
      Assert::AreEqual(size_t(0), ra->size());

      const size_t migrateBegin = 25;
      const size_t migrateCount = a.size() - migrateBegin;
      for(size_t i = migrateBegin; i < a.size(); ++i) {
        ra->getOrAdd(i);
      }
      RuntimeTable::migrate(migrateBegin, a, b, migrateCount);

      Assert::AreEqual(migrateCount, rb->size());
      for(size_t i = 0; i < migrateCount; ++i) {
        auto it = rb->find(i);
        Assert::IsTrue(it != rb->end());
        Assert::AreEqual(*it, i);
      }

      //All sparse elements that had values were moved to B, leaving nothing in A
      Assert::AreEqual(size_t(0), ra->size());

      //rb should have the moved elements whose value matches the index offset by the number of remaining elements in ra
      {
        PackedIndexArray visited;
        visited.resize(migrateCount, 1);
        for(auto it = rb->begin(); it != rb->end(); ++it) {
          visited.at(*it) = 1;
        }
        for(size_t i = 0; i < visited.size(); ++i) {
          Assert::AreEqual(size_t(1), *visited.at(i));
        }
      }

      a.resize(0);
      b.resize(0);

      a.resize(10);
      RuntimeTable::migrate(0, a, b, 10);

      Assert::AreEqual(static_cast<size_t>(0), rb->size());

      a.resize(10);
      b.resize(10);
      ra->getOrAdd(0);
      ra->getOrAdd(2);
      ra->getOrAdd(3);

      RuntimeTable::migrate(0, a, b, 5);

      Assert::IsFalse(ra->contains(0));
      Assert::IsFalse(ra->contains(2));
      Assert::IsFalse(ra->contains(3));

      Assert::AreEqual(size_t(15), b.size());
      Assert::AreEqual(size_t(3), rb->size());
      {
        auto it = rb->find(10);
        Assert::IsTrue(it != rb->end() && *it == 10);
        it = rb->find(12);
        Assert::IsTrue(it != rb->end() && *it == 12);
        it = rb->find(13);
        Assert::IsTrue(it != rb->end() && *it == 13);
      }
    }

    TEST_METHOD(DestructionAndReuse) {
      RuntimeDatabase db = createDatabase<Database<
        Table<SparseRow<int>>,
        Table<SparseRow<int>>
      >>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];

      a.resize(100);

      auto ra = a.tryGet<SparseRow<int>>();

      ra->getOrAdd(1) = 1;
      ra->erase(1);
      Assert::AreEqual(0, ra->getOrAdd(1));

      ra->getOrAdd(0) = 2;
      RuntimeTable::migrate(0, a, b, 1);

      Assert::IsFalse(ra->contains(0));
      Assert::AreEqual(0, ra->getOrAdd(0));

      ra->getOrAdd(1) = 3;
      a.resize(0);
      a.resize(100);
      Assert::IsFalse(ra->contains(1));
      Assert::AreEqual(0, ra->getOrAdd(1));

      a.resize(10);
      for(int i = 0; i < 5; ++i) {
        ra->getOrAdd(i) = 4;
      }
      RuntimeTable::migrate(0, a, b, 5);
      for(int i = 0; i < 5; ++i) {
        Assert::IsFalse(ra->contains(i));
        Assert::AreEqual(0, ra->getOrAdd(i));
      }
    }

    TEST_METHOD(SwapRemove) {
      SparseRow<int> row;
      row.resize(0, 2);
      row.getOrAdd(0) = 1;
      row.getOrAdd(1) = 2;

      row.swapRemove(0, 1, 2);

      Assert::IsTrue(row.contains(0));
      Assert::AreEqual(2, row.getOrAdd(0));
      for(auto it : row) {
        Assert::AreEqual(size_t(0), it.first);
        Assert::AreEqual(2, it.second);
      }

      row.swapRemove(0, 1, 1);

      Assert::AreEqual(size_t(0), row.size());
      bool found = false;
      for(auto it : row) {
        found = true;
      }
      Assert::IsFalse(found);
    }
  };
}