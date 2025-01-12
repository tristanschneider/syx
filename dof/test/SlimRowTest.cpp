#include "Precompile.h"
#include "CppUnitTest.h"

#include "SlimRow.h"
#include "generics/ValueConstant.h"
#include "RuntimeDatabase.h"
#include "Database.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace Test {
  static_assert(gnx::float_constant<1.5f>::value == 1.5f);
  static_assert(ValueGen<gnx::float_constant<0.1f>, float>);
  static_assert(gnx::value_constant<float>::value == 0.0f);

  TEST_CLASS(SlimRowTest) {
    template<class T>
    static RuntimeDatabase createDatabase() {
      RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
      DBReflect::addDatabase<T>(args);
      return RuntimeDatabase{ std::move(args) };
    }

    TEST_METHOD(Basic) {
      RuntimeDatabase db = createDatabase<Database<
        Table<Row<int>, SlimRow<float>>,
        Table<SlimRow<float>>
      >>();
      RuntimeTable& a = db[0];
      RuntimeTable& b = db[1];
      auto qa = db.query<SlimRow<float>>(a.getID());
      auto qb = db.query<SlimRow<float>>(b.getID());

      a.addElements(5);

      {
        auto [sa] = qa.get(0);
        int i = 0;
        for(float& f : sa) {
          f = static_cast<float>(i++);
        }
      }

      RuntimeTable::migrate(2, a, b, 2);

      {
        auto [sb] = qb.get(0);
        std::vector<float> values(sb.begin(), sb.end());

        Assert::IsTrue(std::vector<float>{ 2, 3 } == values);
      }
      {
        auto [sa] = qa.get(0);
        std::vector<float> values(sa.begin(), sa.end());

        Assert::IsTrue(std::vector<float>{ 0, 1, 4 } == values);
      }

      a.swapRemove(1);

      {
        auto [sa] = qa.get(0);
        std::vector<float> values(sa.begin(), sa.end());

        Assert::IsTrue(std::vector<float>{ 0, 4 } == values);
      }

      b.resize(1);
      {
        auto [sb] = qb.get(0);
        std::vector<float> values(sb.begin(), sb.end());
        Assert::IsTrue(std::vector<float>{ 2 } == values);
      }

      b.tryGet<SlimRow<float>>()->at(0) = 5.0f;
      b.resize(100);
      {
        auto [sb] = qb.get(0);
        std::vector<float> values(sb.begin(), sb.end());
        Assert::AreEqual(5.0f, values[0]);
        for(size_t i = 1; i < sb.size; ++i) {
          Assert::AreEqual(0.0f, sb->at(i));
        }
      }
    }
  };
}