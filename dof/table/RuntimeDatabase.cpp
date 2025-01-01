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
  return TableID{ UnpackedDatabaseElementID{ 0, elementIndexBits }.remake(index, 0) };
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
  return { elementIndexBits };
}

StableElementMappings& RuntimeDatabase::getMappings() {
  return *mappings;
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

constexpr size_t computeElementIndexBits(size_t tableCount) {
  constexpr size_t totalBits = sizeof(size_t)*8;
  return totalBits - dbDetails::constexprLog2(tableCount);
}

RuntimeDatabase::RuntimeDatabase(RuntimeDatabaseArgs&& args)
  //Now that the final table count is known, determine bits needed to store the index
  : elementIndexBits{ computeElementIndexBits(args.tables.size()) }
  , mappings{ args.mappings }
  , storage{ std::move(args.storage) }
{
  //Create tables, assigning their table ids using the finalized bit count in ascending index order
  //This isn't intended to guarantee any order to the original static DB objects they came from
  TableID base{ UnpackedDatabaseElementID{ 0, elementIndexBits } };
  tables.reserve(args.tables.size());
  for(size_t i = 0; i < args.tables.size(); ++i) {
    RuntimeTableRowBuilder* table = &args.tables[i];
    const bool hasStableRow = table->contains<StableIDRow>();
    tables.emplace_back(RuntimeTableArgs{
      //Populate the stable mappings if the table should use them
      .mappings = hasStableRow ? args.mappings : nullptr,
      .tableID = TableID{ base.remake(i, 0) },
      .rows = std::move(table->rows),
    });
  }
}
