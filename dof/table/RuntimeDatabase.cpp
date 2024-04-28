#include "RuntimeDatabase.h"
#include <algorithm>

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

QueryResult<> RuntimeDatabase::queryAliasTables(std::initializer_list<QueryAliasBase> aliases) const {
  QueryResult result;
  for(size_t i = 0; i < data.tables.size(); ++i) {
    if(std::all_of(aliases.begin(), aliases.end(), [this, i](const QueryAliasBase& q) { return data.tables[i].tryGet(q.type) != nullptr; })) {
      result.matchingTableIDs.push_back(data.tables[i].tableID);
    }
  }
  return result;
}

DatabaseDescription RuntimeDatabase::getDescription() {
  return { data.elementIndexBits };
}

StableElementMappings& RuntimeDatabase::getMappings() {
  return *data.mappings;
}

void RuntimeTable::migrateOne(size_t i, RuntimeTable& from, RuntimeTable& to) {
  //Only implemented for stable tables right now
  assert(to.stableModifier.table);
  const UnpackedDatabaseElementID fromID = from.tableID.remakeElement(i);
  //Move all common rows to the destination
  for(auto& pair : to.rows) {
    RuntimeRow* toRow = &pair.second;
    RuntimeRow* fromRow = from.tryGetRow(pair.first);
    //This handles the case where the source row is empty and in that case adds an empty destination element
    toRow->migrateOneElement(fromRow ? fromRow->row : nullptr, toRow->row, fromID, to.tableID, *to.stableModifier.stableMappings);
  }

  //Swap Remove from source. Could be faster to combine this with the above step while visiting,
  //but is more confusing when accounting for cases where src has rows dst doesn't
  //Skip stable row because that was already addressed in the migrate above
  for(auto& pair : from.rows) {
    if(pair.first != DBTypeID::get<StableIDRow>()) {
      pair.second.swapRemove(pair.second.row, fromID, *to.stableModifier.stableMappings);
    }
  }
}

namespace DBReflect {
  //Hack to recompute the final results because they can sometimes be invalid when incrementally merging multiple
  //dbs together. Should be a more formal process so it's computed once in a realistic place
  void fixArgs(RuntimeDatabaseArgs& args) {
    //Make sure the final bit count is enough to contain ids across all tables
    args.elementIndexBits = details::computeElementIndexBits(args.tables.size());
    UnpackedDatabaseElementID base{ 0, args.elementIndexBits };
    //Make sure all table ids now reflect the potentially changed bit counts in their ids
    for(size_t i = 0; i < args.tables.size(); ++i) {
      RuntimeTable* table = &args.tables[i];
      table->tableID = base.remake(i, 0);
      if(table->stableModifier) {
        table->stableModifier.tableID = table->tableID;
      }
    }
  }

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
      StableElementMappings* mappings = &a->getRuntime().getMappings();
      assert(mappings == &b->getRuntime().getMappings());
      result.mappings = mappings;
      fixArgs(result);
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

  std::unique_ptr<IDatabase> bundle(std::unique_ptr<IDatabase> db, std::unique_ptr<StableElementMappings> mappings) {
    struct Bundle : IDatabase {
      RuntimeDatabase& getRuntime() override {
        return db->getRuntime();
      }

      std::unique_ptr<IDatabase> db;
      std::unique_ptr<StableElementMappings> mappings;
    };
    auto result = std::make_unique<Bundle>();
    result->db = std::move(db);
    result->mappings = std::move(mappings);
    return result;
  }
}