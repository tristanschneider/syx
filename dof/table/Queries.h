#pragma once

#include "Database.h"
#include "TableOperations.h"

namespace Queries {
  //TODO: could use some argument deduction here to avoid having to duplicate template parameters in template arguments and visitor arguments
  template<class... Rows, class DatabaseT, class Visitor>
  void viewEachRow(DatabaseT& db, const Visitor& visitor) {
    db.visitOne([&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr((TableOperations::hasRow<Rows, TableT>() && ...)) {
        visitor(TableOperations::getRow<Rows>(table)...);
      }
    });
  }

  template<class... Rows, class DatabaseT, class Visitor>
  void viewEachRowWithTableID(DatabaseT& db, const Visitor& visitor) {
    db.visitOne([&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr((TableOperations::hasRow<Rows, TableT>() && ...)) {
        typename DatabaseT::ElementID tableID = typename DatabaseT::getTableIndex<TableT>();
        visitor(tableID, TableOperations::getRow<Rows>(table)...);
      }
    });
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