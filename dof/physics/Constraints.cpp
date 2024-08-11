#include "Precompile.h"
#include "Constraints.h"

#include "AppBuilder.h"

namespace Constraints {
  template<class T>
  auto getOrAssert(const QueryAlias<T>& q, RuntimeDatabaseTaskBuilder& task, const TableID& table) {
    auto result = task.queryAlias(table, q);
    assert(result.size());
    return &result.get<0>(0);
  }

  struct ResolveTarget {
    Rows::Target operator()(NoTarget t) const {
      return t;
    }
    Rows::Target operator()(SelfTarget t) const {
      return t;
    }
    Rows::Target operator()(const ExternalTargetRowAlias& t) const {
      return getOrAssert(t, task, table);
    }

    RuntimeDatabaseTaskBuilder& task;
    const TableID& table;
  };
  Rows Definition::resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
    Rows result;
    ResolveTarget resolveTarget{ task, table };
    result.targetA = std::visit(resolveTarget, targetA);
    result.targetB = std::visit(resolveTarget, targetB);
    //A side is allowed to be empty but if it is specified the row must exist
    if(sideA) {
      result.sideA = getOrAssert(sideA, task, table);
    }
    if(sideB) {
      result.sideB = getOrAssert(sideB, task, table);
    }
    result.common = getOrAssert(common, task, table);
    return result;
  }
}