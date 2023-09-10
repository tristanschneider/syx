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

namespace details {
  template<class... T>
  struct RowList {};

  struct ElementCallbackType {
    struct TupleNoID {};
    struct TupleWithID {};
    struct ElementNoID {};
    struct ElementWithID {};
  };

  template<class CB, class Rows, class Enabled = void>
  struct GetElementCallbackType;

  template<class CB, class... Rows>
  struct GetElementCallbackType<CB, RowList<Rows...>, std::enable_if_t<
      std::is_same_v<
        void,
        decltype(std::declval<const CB&>()(std::declval<UnpackedDatabaseElementID>(), std::declval<std::tuple<std::vector<Rows>*...>&>()))
      >>> {
    using type = typename ElementCallbackType::TupleWithID;
  };

  template<class CB, class... Rows>
  struct GetElementCallbackType<CB, RowList<Rows...>, std::enable_if_t<
    std::is_same_v<
      void,
      decltype(std::declval<const CB&>()(std::declval<std::tuple<std::vector<Rows*>...>&>()))
    >>> {
    using type = typename ElementCallbackType::TupleNoID;
  };

  template<class CB, class... Rows>
  struct GetElementCallbackType<CB, RowList<Rows...>, std::enable_if_t<
    std::is_same_v<
      void,
      decltype(std::declval<const CB&>()(std::declval<UnpackedDatabaseElementID>(), std::declval<Rows&>()...))
    >>> {
    using type = typename ElementCallbackType::ElementWithID;
  };

  template<class CB, class... Rows>
  struct GetElementCallbackType<CB, RowList<Rows...>, std::enable_if_t<
    std::is_same_v<
      void,
      decltype(std::declval<const CB&>()(std::declval<Rows&>().at(0)...))
    >>> {
    using type = typename ElementCallbackType::ElementNoID;
  };
};

template<class... Rows>
struct QueryResult {
  using TupleT = std::tuple<std::vector<Rows*>...>;

  template<class CB>
  void forEachElement(const CB& cb) {
    using CBT = typename details::GetElementCallbackType<CB, details::RowList<Rows...>>::type;
    for(size_t i = 0; i < matchingTableIDs.size(); ++i) {
      for(size_t e = 0; e < std::get<0>(rows).size(); ++e) {
        if constexpr(std::is_same_v<details::ElementCallbackType::ElementNoID, CBT>) {
          cb(std::get<std::vector<Rows*>>(rows).at(i)->at(e)...);
        }
      }
    }
  }

  std::tuple<Rows*...> get(size_t i) {
    return { &std::get<std::vector<Row<Rows>*>>(rows)... };
  }

  std::tuple<Rows*...> getSingleton() {
    return get(0);
  }

  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
  TupleT rows;
};

struct RuntimeDatabaseArgs {
  size_t elementIndexBits;
  std::vector<RuntimeTable> tables;
  //TODO: this is a hack to get access to mappings without knowing where they are
  //relationship of table to their mappings should be built into table. Maybe a required row?
  StableElementMappings* mappings{};
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
        std::tuple<Rows*...> rows{ data.tables[i].tryGet<std::decay_t<Rows>>()... };
        const bool allFound = (std::get<Rows*>(rows) && ...);
        if(allFound) {
          result.matchingTableIDs.push_back(getTableID(i));
          (std::get<std::vector<Rows*>>(result.rows).push_back(std::get<std::decay_t<Rows*>>(rows)), ...);
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
    void reflectTable(size_t baseIndex, TableT& table, RuntimeDatabaseArgs& args) {
      RuntimeTable& rt = args.tables[baseIndex + DB::getTableIndex<TableT>().getTableIndex()];
      if constexpr(TableOperations::isStableTable<TableT>) {
        rt.stableModifier = StableTableModifierInstance::get<DB>(table, *args.mappings);
      }
      else {
        rt.modifier = TableModifierInstance::get(table);
      }
      table.visitOne([&](auto& row) { reflectRow(row, rt); });
    }
  }

  //If used for a single database the UnpackedDatabaseElementID will be the same
  //If multiple are combined the IDs will differ if used/queried via the direct database or
  //the runtime database, so care must be taken not to mix them
  template<class DB>
  void reflect(DB& db, RuntimeDatabaseArgs& args, StableElementMappings& mappings) {
    const size_t baseIndex = args.tables.size();
    const size_t newTables = db.size();
    args.tables.resize(baseIndex + newTables);
    //TODO: what if multiple different mappings are desired? The table should probably point at the mappings it uses
    args.mappings = &mappings;
    args.elementIndexBits = dbDetails::constexprLog2(args.tables.size());
    db.visitOne([&](auto& table) { details::reflectTable<DB>(baseIndex, table, args); });
  }
}