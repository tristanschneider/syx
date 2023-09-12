#include "RuntimeDatabase.h"

QueryResult<> RuntimeDatabase::query() {
  QueryResult<> result;
  result.matchingTableIDs.resize(data.tables.size());
  for(size_t i = 0; i < data.tables.size(); ++i) {
    result.matchingTableIDs[i] = getTableID(i);
  }
  return result;
}

RuntimeTable* RuntimeDatabase::tryGet(const UnpackedDatabaseElementID& id) {
  const size_t i = id.getTableIndex();
  return i < data.tables.size() ? &data.tables[i] : nullptr;
}

UnpackedDatabaseElementID RuntimeDatabase::getTableID(size_t index) const {
  return UnpackedDatabaseElementID{ 0, data.elementIndexBits }.remake(index, 0);
}

namespace DBReflect {
  struct MergedDatabase : IDatabase {
    MergedDatabase(std::unique_ptr<IDatabase> l, std::unique_ptr<IDatabase> r) 
      : a{ std::move(l) }
      , b{ std::move(r) }
      , runtime{ getArgs() } {
    }

    static void addToArgs(RuntimeDatabaseArgs& args, IDatabase& db) {
      RuntimeDatabase& rdb = db.getRuntime();
      for(const UnpackedDatabaseElementID& t : rdb.query().matchingTableIDs) {
        RuntimeTable* table = rdb.tryGet(t);
        args.tables.push_back(*table);
      }
    }

    RuntimeDatabaseArgs getArgs() {
      RuntimeDatabaseArgs result;
      addToArgs(result, *a);
      addToArgs(result, *b);
      result.elementIndexBits = dbDetails::bitsToContain(result.tables.size());
      return result;
    }

    RuntimeDatabase& getRuntime() override {
      return runtime;
    }

    std::unique_ptr<IDatabase> a;
    std::unique_ptr<IDatabase> b;
    RuntimeDatabase runtime;
  };

  std::unique_ptr<IDatabase> merge(std::unique_ptr<IDatabase> l, std::unique_ptr<IDatabase> r) {
    return std::make_unique<MergedDatabase>(std::move(l), std::move(r));
  }

}