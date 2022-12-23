#pragma once

#include "Database.h"
#include "TableOperations.h"

namespace Queries {
  template<class... Rows, class DatabaseT, class Visitor>
  void viewEachRow(DatabaseT& db, const Visitor& visitor) {
    db.visitOne([&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr((TableOperations::hasRow<Rows, TableT>() && ...)) {
        visitor(TableOperations::getRow<Rows>(table)...);
      }
    });
  }
};