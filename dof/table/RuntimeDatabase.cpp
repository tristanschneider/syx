#include "Precompile.h"
#include "RuntimeDatabase.h"

#include <algorithm>
#include "StableElementID.h"

QueryResult<> RuntimeDatabase::query() {
  QueryResult<> result;
  result.matchingTableIDs.resize(tables.size());
  for(size_t i = 0; i < tables.size(); ++i) {
    result.matchingTableIDs[i] = getTableID(i);
  }
  return result;
}

RuntimeTable* RuntimeDatabase::tryGet(const TableID& id) {
  const size_t i = id.getTableIndex();
  return i < tables.size() ? &tables[i] : nullptr;
}

TableID RuntimeDatabase::getTableID(size_t index) const {
  TableID result;
  result.setDatabaseIndex(databaseIndex);
  result.setTableIndex(index);
  return result;
}

QueryResult<> RuntimeDatabase::queryAliasTables(std::initializer_list<QueryAliasBase> aliases) const {
  QueryResult result;
  for(size_t i = 0; i < tables.size(); ++i) {
    if(std::all_of(aliases.begin(), aliases.end(), [this, i](const QueryAliasBase& q) { return tables[i].tryGet(q.type) != nullptr; })) {
      result.matchingTableIDs.push_back(tables[i].getID());
    }
  }
  return result;
}

DatabaseDescription RuntimeDatabase::getDescription() {
  return { databaseIndex };
}

StableElementMappings& RuntimeDatabase::getMappings() {
  return *mappings;
}

RuntimeTable& RuntimeDatabase::operator[](size_t i) {
  return tables[i];
}

size_t RuntimeDatabase::size() const {
  return tables.size();
}

void RuntimeDatabase::setTableDirty(size_t i) {
  dirtyTables.set(i);
}

void RuntimeDatabase::setTableDirty(const TableID& t) {
  setTableDirty(t.getTableIndex());
}

const gnx::DynamicBitset& RuntimeDatabase::getDirtyTables() const {
  return dirtyTables;
}

void RuntimeDatabase::clearDirtyTables() {
  dirtyTables.resetBits();
}

namespace DBReflect {
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
  : databaseIndex{ args.dbIndex }
  , mappings{ args.mappings }
  , storage{ std::move(args.storage) }
{
  //Create tables, assigning their table ids using the finalized bit count in ascending index order
  //This isn't intended to guarantee any order to the original static DB objects they came from
  TableID base;
  base.setDatabaseIndex(args.dbIndex);
  tables.reserve(args.tables.size());
  for(size_t i = 0; i < args.tables.size(); ++i) {
    RuntimeTableRowBuilder* table = &args.tables[i];
    const bool hasStableRow = table->contains<StableIDRow>();
    base.setTableIndex(i);
    tables.emplace_back(RuntimeTableArgs{
      //Populate the stable mappings if the table should use them
      .mappings = hasStableRow ? args.mappings : nullptr,
      .tableID = base,
      .rows = std::move(*table),
    });
  }

  dirtyTables.resize(tables.size());
}
