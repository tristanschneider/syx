#pragma once

#include "Database.h"
#include "TableOperations.h"

namespace Queries {
  template<class Row, class... Rows, class DatabaseT, class Visitor>
  void viewEachRow(DatabaseT& db, const Visitor& visitor) {
    db.visitOne([&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>() && (TableOperations::hasRow<Rows, TableT>() && ...)) {
        visitor(TableOperations::getRow<Row>(table), TableOperations::getRow<Rows>(table)...);
      }
    });
  }

  template<class Row, class... Rows, class DatabaseT, class Visitor>
  void viewEachRowWithTableID(DatabaseT& db, const Visitor& visitor) {
    db.visitOne([&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>() && (TableOperations::hasRow<Rows, TableT>() && ...)) {
        typename DatabaseT::ElementID tableID = typename DatabaseT::getTableIndex<TableT>();
        visitor(tableID, TableOperations::getRow<Row>(table), TableOperations::getRow<Rows>(table)...);
      }
    });
  }

  namespace impl {
    template<class Fn>
    struct DeduceArgs {};
    template<class R, class T, class Arg, class... Args>
    struct DeduceArgs<R(T::*)(Arg, Args...) const> {
      template<class DatabaseT>
      static void _viewEachRow(DatabaseT& db, const T& visitor) {
        Queries::viewEachRow<std::decay_t<Arg>, std::decay_t<Args>...>(db, visitor);
      }

      template<class DatabaseT>
      static void _viewEachRowWithTableID(DatabaseT& db, const T& visitor) {
        //Ignore the first argument since it's now a row, it's the id
        Queries::viewEachRowWithTableID<std::decay_t<Args>...>(db, visitor);
      }
    };

    template<class T>
    using DeduceArgsT = DeduceArgs<decltype(&T::operator())>;
  }

  template<class DatabaseT, class Visitor>
  void viewEachRow(DatabaseT& db, const Visitor& visitor) {
    impl::DeduceArgsT<Visitor>::_viewEachRow(db, visitor);
  }

  template<class DatabaseT, class Visitor>
  void viewEachRowWithTableID(DatabaseT& db, const Visitor& visitor) {
    impl::DeduceArgsT<Visitor>::_viewEachRowWithTableID(db, visitor);
  }

  template<class Row, class DatabaseT>
  Row* getRowInTable(DatabaseT& db, typename DatabaseT::ElementID id) {
    Row* result = nullptr;
    db.visitOneByIndex(id, [&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>()) {
        result = &TableOperations::getRow<Row>(table);
      }
    });
    return result;
  }

  template<class Row, class DatabaseT>
  const Row* getRowInTable(const DatabaseT& db, typename DatabaseT::ElementID id) {
    const Row* result = nullptr;
    db.visitOneByIndex(id, [&](const auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>()) {
        result = &TableOperations::getRow<Row>(table);
      }
    });
    return result;
  }
};