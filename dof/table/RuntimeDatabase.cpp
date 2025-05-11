#include "Precompile.h"
#include "RuntimeDatabase.h"

#include <algorithm>
#include "StableElementID.h"
#include "TableName.h"

QueryResultBase RuntimeDatabase::query() {
  std::vector<const RuntimeTable*> result(tables.size());
  std::transform(tables.begin(), tables.end(), result.begin(), [](const RuntimeTable& t) { return &t; });
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

QueryResultBase RuntimeDatabase::queryAliasTables(std::initializer_list<QueryAliasBase> aliases) const {
  std::vector<const RuntimeTable*> result;
  for(size_t i = 0; i < tables.size(); ++i) {
    if(std::all_of(aliases.begin(), aliases.end(), [this, i](const QueryAliasBase& q) { return tables[i].tryGet(q.type) != nullptr; })) {
      result.push_back(&tables[i]);
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

StorageTableBuilder& StorageTableBuilder::setTableName(TableName::TableName&& name) {
  addRow<TableName::TableNameRow>().at() = std::move(name);
  return *this;
}

StorageTableBuilder& StorageTableBuilder::setStable() {
  return addRows<StableIDRow>();
}

void StorageTableBuilder::finalize(RuntimeDatabaseArgs& args)&& {
  //Shouldn't matter what this is as long as it's unique
  mBuilder.tableType.value = std::accumulate(mBuilder.rows.begin(), mBuilder.rows.end(), size_t{}, [](size_t h, const RuntimeTableRowBuilder::Row& row) {
    return gnx::Hash::combine(h, row.type.value);
  });
  args.tables.push_back(std::move(mBuilder));
  RuntimeStorage::addToChain<TChainedRuntimeStorage<std::unique_ptr<IRuntimeStorage>>>(args)->value = std::move(mStorage);
}