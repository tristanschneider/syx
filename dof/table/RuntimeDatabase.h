#pragma once

#include "TableOperations.h"
#include "TypeId.h"

using DBTypeID = TypeID<struct DBTypeT>;

struct RuntimeTable {
  using IDT = DBTypeID;

  template<class RowT>
  RowT* tryGet() {
    auto it = rows.find(IDT::get<std::decay_t<RowT>>());
    return it != rows.end() ? (RowT*)it->second : nullptr;
  }

  TableModifierInstance modifier;
  StableTableModifierInstance stableModifier;
  std::unordered_map<DBTypeID, void*> rows;
};

template<class... Rows>
struct QueryResult {
  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
  std::tuple<std::vector<Rows*>...> rows;
};

struct RuntimeDatabaseArgs {
  size_t elementIndexBits;
  std::vector<RuntimeTable> tables;
  //TODO: this is a hack to get access to mappings without knowing where they are
  //relationship of table to their mappings should be built into table. Maybe a required row?
  StableElementMappings* mappings{};
  void* data;
};

class RuntimeDatabase {
public:
  RuntimeDatabase(RuntimeDatabaseArgs&& args)
    : data{ std::move(args) } {
  }

  RuntimeTable* tryGet(const UnpackedDatabaseElementID& id);
  QueryResult<> query();

  template<class... Rows>
  QueryResult<Rows...> query() {
    if constexpr(sizeof...(Rows) == 0) {
      return query();
    }
    else {
      QueryResult<Rows...> result;
      result.matchingTableIDs.reserve(data.tables.size());
      for(size_t i = 0; i < data.tables.size(); ++i) {
        std::tuple<Rows*...> rows{ data.tables[i].tryGet<Rows>()... };
        const bool allFound = (std::get<Rows*>(rows) && ...);
        if(allFound) {
          result.matchingTableIDs.push_back(getTableID(i));
          (std::get<std::vector<Rows*>>(result.rows).push_back(std::get<Rows*>(rows)), ...);
        }
      }
      return result;
    }
  }

private:
  UnpackedDatabaseElementID getTableID(size_t index) const;

  RuntimeDatabaseArgs data;
};

struct IDatabase {
  virtual ~IDatabase();
  virtual RuntimeDatabase& getRuntime() = 0;
};

//Specialization that provides all row ids
template<>
struct QueryResult<> {
  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
};

namespace DBReflect {
  namespace details {
    template<class RowT>
    void reflectRow(RowT& row, RuntimeTable& table) {
      table.rows[DBTypeID::get<RowT>()] = &row;
    }

    template<class DB, class TableT>
    void reflectTable(TableT& table, RuntimeDatabaseArgs& args) {
      RuntimeTable& rt = args.tables[DB::getTableIndex<TableT>().getTableIndex()];
      if constexpr(TableOperations::isStableTable<TableT>) {
        rt.stableModifier = StableTableModifierInstance::get<DB>(table, *args.mappings);
      }
      else {
        rt.modifier = TableModifierInstance::get(table);
      }
      table.visitOne([&](auto& row) { reflectRow(row, rt); });
    }
  }

  template<class DB>
  void reflect(DB& db, RuntimeDatabaseArgs& args, StableElementMappings& mappings) {
    args.elementIndexBits = DB::getDescription().elementIndexBits;
    args.data = &db;
    args.tables.resize(db.size());
    args.mappings = &mappings;
    db.visitOne([&](auto& table) { details::reflectTable<DB>(table, args); });
  }
}