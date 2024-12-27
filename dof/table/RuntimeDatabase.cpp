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

RuntimeTable* RuntimeDatabase::tryGet(const TableID& id) {
  const size_t i = id.getTableIndex();
  return i < data.tables.size() ? &data.tables[i] : nullptr;
}

TableID RuntimeDatabase::getTableID(size_t index) const {
  return TableID{ UnpackedDatabaseElementID{ 0, data.elementIndexBits }.remake(index, 0) };
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

size_t RuntimeTable::migrateOne(size_t i, RuntimeTable& from, RuntimeTable& to) {
  //Only implemented for stable tables right now
  assert(to.stableModifier.table);
  const UnpackedDatabaseElementID fromID = from.tableID.remakeElement(i);
  //Move all common rows to the destination
  size_t result{};
  for(auto& pair : to.rows) {
    RuntimeRow* toRow = &pair.second;
    RuntimeRow* fromRow = from.tryGetRow(pair.first);
    //This handles the case where the source row is empty and in that case adds an empty destination element
    result = toRow->migrateOneElement(fromRow ? fromRow->row : nullptr, toRow->row, fromID, to.tableID, *to.stableModifier.stableMappings);
  }

  //Swap Remove from source. Could be faster to combine this with the above step while visiting,
  //but is more confusing when accounting for cases where src has rows dst doesn't
  //Skip stable row because that was already addressed in the migrate above
  for(auto& pair : from.rows) {
    if(pair.first != DBTypeID::get<StableIDRow>()) {
      pair.second.swapRemove(pair.second.row, fromID, *to.stableModifier.stableMappings);
    }
  }

  return result;
}

namespace DBReflect {
  //Hack to recompute the final results because they can sometimes be invalid when incrementally merging multiple
  //dbs together. Should be a more formal process so it's computed once in a realistic place
  void fixArgs(RuntimeDatabaseArgs& args) {
    //Make sure the final bit count is enough to contain ids across all tables
    args.elementIndexBits = details::computeElementIndexBits(args.tables.size());
    TableID base{ UnpackedDatabaseElementID{ 0, args.elementIndexBits } };
    //Make sure all table ids now reflect the potentially changed bit counts in their ids
    for(size_t i = 0; i < args.tables.size(); ++i) {
      RuntimeTable* table = &args.tables[i];
      table->tableID = TableID{ base.remake(i, 0) };
      if(table->stableModifier) {
        table->stableModifier.tableID = table->tableID;
      }
    }
  }

  void addStableMappings(RuntimeDatabaseArgs& args, std::unique_ptr<StableElementMappings> mappings) {
    //Create a class to store the mappings
    struct Storage : ChainedRuntimeStorage {
      using ChainedRuntimeStorage::ChainedRuntimeStorage;
      std::unique_ptr<StableElementMappings> m;
    };
    //Store the mappings on args
    args.mappings = mappings.get();
    //Link the storage of this into args
    Storage* s = RuntimeStorage::addToChain<Storage>(args);
    //Transfer ownership of the mappings to Storage
    s->m = std::move(mappings);
  }

  RuntimeDatabaseArgs createArgsWithMappings() {
    RuntimeDatabaseArgs result;
    addStableMappings(result, std::make_unique<StableElementMappings>());
    return result;
  }
}

RuntimeDatabase::RuntimeDatabase(RuntimeDatabaseArgs&& args)
  : data{ std::move(args) } {
  DBReflect::fixArgs(data);
}
