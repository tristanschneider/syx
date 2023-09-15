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

  //TODO: replace with is_invokable_v
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

  template<class CB>
  void forEachRow(CB&& cb) {
    for(size_t i = 0; i < matchingTableIDs.size(); ++i) {
      if constexpr(std::is_invocable_v<CB, Rows&...>) {
        cb(*std::get<std::vector<Rows*>>(rows).at(i)...);
      }
      else if constexpr(std::is_invocable_v<CB, UnpackedDatabaseElementID, Rows&...>) {
        cb(matchingTableIDs[i], *std::get<std::vector<Rows*>>(rows).at(i)...);
      }
    }
  }

  std::tuple<Rows*...> get(size_t i) {
    return { &std::get<std::vector<Rows*>>(rows)... };
  }

  std::tuple<Rows*...> getSingleton() {
    return get(0);
  }

  //Get the vector of rows
  template<size_t TupleIndex>
  auto& get() {
    return std::get<TupleIndex>(rows);
  }

  //Get a particular row
  template<size_t TupleIndex>
  auto& get(size_t tableIndex) {
    return *get<TupleIndex>().at(tableIndex);
  }

  //Get a row by row type
  template<class RowT>
  RowT& get(size_t tableIndex) {
    return *std::get<std::vector<RowT*>>(rows).at(tableIndex);
  }

  //Return the first element in the first table
  template<size_t TupleIndex = 0>
  auto* tryGetSingletonElement() {
    decltype(&get<TupleIndex>().at(0)->at(0)) result = nullptr;
    if(auto& foundRows = get<TupleIndex>(); foundRows.size()) {
      if(auto* foundRow = foundRows.at(0); foundRow->size()) {
        result = &foundRow->at(0);
      }
    }
    return result;
  }

  size_t size() const {
    return matchingTableIDs.size();
  }

  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
  TupleT rows;
};

struct RuntimeDatabaseArgs {
  size_t elementIndexBits;
  std::vector<RuntimeTable> tables;
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
        _tryAddResult(i, result);
      }
      return result;
    }
  }

  template<class... Rows>
  QueryResult<Rows...> query(const UnpackedDatabaseElementID& id) {
    QueryResult<Rows...> result;
    const size_t index = id.getTableIndex();
    if(index < data.tables.size()) {
      _tryAddResult(index, result);
    }
    return result;
  }

private:
  template<class... Rows>
  void _tryAddResult(size_t index, QueryResult<Rows...>& result) {
      std::tuple<Rows*...> rows{ data.tables[index].tryGet<std::decay_t<Rows>>()... };
      const bool allFound = (std::get<Rows*>(rows) && ...);
      if(allFound) {
        result.matchingTableIDs.push_back(getTableID(index));
        (std::get<std::vector<Rows*>>(result.rows).push_back(std::get<std::decay_t<Rows*>>(rows)), ...);
      }
  }

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

    template<class TableT>
    void reflectTable(const UnpackedDatabaseElementID& tableID, TableT& table, RuntimeDatabaseArgs& args, StableElementMappings& mappings) {
      RuntimeTable& rt = args.tables[tableID.getTableIndex()];
      if constexpr(TableOperations::isStableTable<TableT>) {
        rt.stableModifier = StableTableModifierInstance::get<DB>(table, mappings);
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
    constexpr size_t newTables = db.size();
    args.tables.resize(baseIndex + newTables);
    args.elementIndexBits = dbDetails::constexprLog2(args.tables.size());
    db.visitOne([&](auto& table) {
      const size_t rawIndex = DB::getTableIndex(table).getTableIndex();
      const auto tableID = UnpackedDatabaseElementID{ 0, args.elementIndexBits }.remake(baseIndex + rawIndex, 0);
      details::reflectTable(tableID, table, args, mappings);
    });
  }

  template<class TableT>
  void addTable(TableT& table, RuntimeDatabaseArgs& args, StableElementMappings& mappings) {
    const size_t baseIndex = args.tables.size();
    const size_t newTables = 1;
    args.tables.resize(baseIndex + newTables);
    args.elementIndexBits = dbDetails::constexprLog2(args.tables.size());
    const auto tableID = UnpackedDatabaseElementID{ 0, args.elementIndexBits }.remake(baseIndex, 0);
    details::reflectTable(tableID, table, args, mappings);
  }

  template<class DB>
  std::unique_ptr<IDatabase> createDatabase() {
    struct Impl : IDatabase {
      Impl()
        : runtime(getArgs()) {
      }

      RuntimeDatabase& getRuntime() override {
        return runtime;
      }

      RuntimeDatabaseArgs getArgs() {
        RuntimeDatabaseArgs result;
        reflect(db, result, mappings);
        return result;
      }

      DB db;
      StableElementMappings mappings;
      RuntimeDatabase runtime;
    };
    return std::make_unique<Impl>();
  }

  std::unique_ptr<IDatabase> merge(std::unique_ptr<IDatabase> l, std::unique_ptr<IDatabase> r);
}