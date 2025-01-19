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
    /* TODO: make it pass
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
        Assert::AreEqual(static_cast<int>(i), it.value());
      }

      a.resize(0);
      b.resize(0);

      a.resize(10);
      RuntimeTable::migrate(0, a, b, 10);

      Assert::AreEqual(static_cast<size_t>(0), rb->size());
    }
    */
  };
}