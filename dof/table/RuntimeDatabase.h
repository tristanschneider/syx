#pragma once

#include "QueryAlias.h"
#include "RuntimeTable.h"

template<class RowT>
using QueryResultRow = std::vector<RowT*>;

template<class T> struct TestT : std::true_type {};

template<class... Rows>
struct QueryResult {
  static_assert((isRow<Rows>() && ...), "Should only be used for rows");
  static_assert((!isNestedRow<Rows>() && ...), "Nested row is likely unintentional");
  using TupleT = std::tuple<QueryResultRow<Rows>...>;
  using IndicesT = std::index_sequence_for<Rows...>;

  template<class CB>
  void forEachElement(const CB& cb) {
    for(size_t i = 0; i < matchingTableIDs.size(); ++i) {
      for(size_t e = 0; e < std::get<0>(rows).at(i)->size(); ++e) {
        if constexpr(std::is_invocable_v<CB, typename Rows::ElementT&...>) {
          cb(std::get<std::vector<Rows*>>(rows).at(i)->at(e)...);
        }
        else if constexpr(std::is_invocable_v<CB, UnpackedDatabaseElementID, typename Rows::ElementT&...>) {
          UnpackedDatabaseElementID id = matchingTableIDs[i].remakeElement(e);
          cb(id, std::get<std::vector<Rows*>>(rows).at(i)->at(e)...);
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

  struct details {
    template<class TupleT, size_t... I>
    static auto get(size_t i, TupleT& tuple, std::index_sequence<I...>) {
      return std::make_tuple(std::get<I>(tuple).at(i)...);
    }
  };

  //Get all rows of a given table.
  std::tuple<Rows*...> get(size_t i) {
    return details::get(i, rows, IndicesT{});
  }

  std::tuple<Rows*...> getSingleton() {
    return get(0);
  }

  template<size_t I>
  auto* getSingleton() {
    return std::get<I>(getSingleton());
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

  template<size_t TupleIndex>
  auto* tryGet(size_t tableIndex) {
    auto& rrow = get<TupleIndex>();
    return rrow.size() > tableIndex ? rrow.at(tableIndex) : nullptr;
  }

  //Get a row by row type
  template<class RowT>
  RowT& get(size_t tableIndex) {
    return *std::get<std::vector<RowT*>>(rows).at(tableIndex);
  }

  template<class RowT>
  auto* tryGet(size_t tableIndex) {
    auto& rrow = std::get<std::vector<RowT*>>(rows);
    return rrow.size() > tableIndex ? rrow.at(tableIndex) : nullptr;
  }

  //Return the first element in the first table
  template<size_t TupleIndex = 0>
  auto* tryGetSingletonElement() {
    decltype(&get<TupleIndex>().at(0)->at(0)) result = nullptr;
    if(auto& foundRows = get<TupleIndex>(); foundRows.size()) {
      //Hack to make this still return something on a shared row if the size is zero
      if constexpr(IsSharedRowT<std::decay_t<decltype(get<TupleIndex>(0))>>::value) {
        result = &foundRows.at(0)->at();
      }
      else {
        if(auto* foundRow = foundRows.at(0); foundRow->size()) {
          result = &foundRow->at(0);
        }
      }
    }
    return result;
  }

  size_t size() const {
    return matchingTableIDs.size();
  }

  std::vector<TableID> matchingTableIDs;
  TupleT rows;
};

struct RuntimeDatabaseArgs {
  std::vector<RuntimeTableRowBuilder> tables;
  StableElementMappings* mappings{};
  std::unique_ptr<IRuntimeStorage> storage;
};

struct ChainedRuntimeStorage : IRuntimeStorage {
  ChainedRuntimeStorage(RuntimeDatabaseArgs& args)
    : child{ std::move(args.storage) } {
  }
  std::unique_ptr<IRuntimeStorage> child;
};

namespace RuntimeStorage {
  template<class T>
  concept ChainedStorage = std::is_base_of_v<ChainedRuntimeStorage, T>;

  template<ChainedStorage T>
  T* addToChain(RuntimeDatabaseArgs& args) {
    std::unique_ptr<T> result = std::make_unique<T>(args);
    T* ptr = result.get();
    args.storage = std::move(result);
    return ptr;
  }
}

class RuntimeDatabase;

struct IDatabase {
  virtual ~IDatabase() = default;
  virtual RuntimeDatabase& getRuntime() = 0;
};

class RuntimeDatabase : public IDatabase {
public:
  RuntimeDatabase(RuntimeDatabaseArgs&& args);

  RuntimeDatabase& getRuntime() final {
    return *this;
  }

  RuntimeTable* tryGet(const TableID& id);
  QueryResult<> query();

  template<class... Rows>
  QueryResult<Rows...> query() {
    if constexpr(sizeof...(Rows) == 0) {
      return query();
    }
    else {
      QueryResult<Rows...> result;
      result.matchingTableIDs.reserve(tables.size());
      for(size_t i = 0; i < tables.size(); ++i) {
        _tryAddResult(i, result);
      }
      return result;
    }
  }

  template<class... Rows>
  QueryResult<Rows...> query(const TableID& id) {
    QueryResult<Rows...> result;
    const size_t index = id.getTableIndex();
    if(index < tables.size()) {
      _tryAddResult(index, result);
    }
    return result;
  }

  template<class... Aliases>
  auto queryAlias(const Aliases&... aliases) {
    if constexpr(sizeof...(Aliases) == 0) {
      return query();
    }
    else {
      QueryResult<typename Aliases::RowT...> result;
      result.matchingTableIDs.reserve(tables.size());
      for(size_t i = 0; i < tables.size(); ++i) {
        _tryAddAliasResult(i, result, aliases...);
      }
      return result;
    }
  }

  template<class... Aliases>
  auto queryAlias(const TableID& id, const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result;
    const size_t index = id.getTableIndex();
    if(index < tables.size()) {
      _tryAddAliasResult(index, result, aliases...);
    }
    return result;
  }

  QueryResult<> queryAliasTables(std::initializer_list<QueryAliasBase> aliases) const;

  DatabaseDescription getDescription();
  StableElementMappings& getMappings();

private:
  template<class... Rows>
  void _tryAddResult(size_t index, QueryResult<Rows...>& result) {
      std::tuple<Rows*...> rows{ tables[index].tryGet<std::decay_t<Rows>>()... };
      const bool allFound = (std::get<Rows*>(rows) && ...);
      if(allFound) {
        result.matchingTableIDs.push_back(getTableID(index));
        (std::get<std::vector<Rows*>>(result.rows).push_back(std::get<std::decay_t<Rows*>>(rows)), ...);
      }
  }

  template<class Tuple, size_t... I>
  static bool areAllNotNull(const Tuple& t, std::index_sequence<I...>) {
    return (std::get<I>(t) && ...);
  }

  template<class SrcTuple, class DstTuple, size_t... I>
  static void copyAll(SrcTuple& src, DstTuple& dst, std::index_sequence<I...>) {
    (std::get<I>(dst).push_back(std::get<I>(src)), ...);
  }

  template<class... Rows, class... Aliases>
  void _tryAddAliasResult(size_t index, QueryResult<Rows...>& result, const Aliases&... aliases) {
    constexpr auto indices = std::index_sequence_for<Rows...>{};
    std::tuple<Rows*...> rows{ aliases.cast(tables[index].tryGet(aliases.type))... };
    if(areAllNotNull(rows, indices)) {
      result.matchingTableIDs.push_back(getTableID(index));
      copyAll(rows, result.rows, indices);
    }
  }

  TableID getTableID(size_t index) const;

  size_t elementIndexBits{};
  std::vector<RuntimeTable> tables;
  StableElementMappings* mappings{};
  std::unique_ptr<IRuntimeStorage> storage;
};

//Specialization that provides all row ids
template<>
struct QueryResult<> {
  size_t size() const {
    return matchingTableIDs.size();
  }

  const TableID& operator[](size_t i) const {
    return matchingTableIDs[i];
  }

  TableID tryGet() const {
    return matchingTableIDs.size() ? matchingTableIDs[0] : TableID{};
  }

  std::vector<TableID> matchingTableIDs;
};

namespace DBReflect {
  namespace details {
    template<class RowT>
    void reflectRow(RowT& row, RuntimeTableRowBuilder& table) {
      table.rows.push_back(RuntimeTableRowBuilder::Row{
        .type = DBTypeID::get<RowT>(),
        .row = &row
      });
    }

    template<class TableT>
    void reflectTable(RuntimeTableRowBuilder& builder, TableT& table) {
      table.visitOne([&](auto& row) { reflectRow(row, builder); });
    }
  }

  //If used for a single database the UnpackedDatabaseElementID will be the same
  //If multiple are combined the IDs will differ if used/queried via the direct database or
  //the runtime database, so care must be taken not to mix them
  template<class DB>
  void reflect(DB& db, RuntimeDatabaseArgs& args) {
    const size_t baseIndex = args.tables.size();
    constexpr size_t newTables = db.size();
    args.tables.resize(baseIndex + newTables);
    size_t i = baseIndex;
    db.visitOne([&](auto& table) {
      //Order of tables in the db vs args doesn't matter, it'll all be finalized upon constructing the database
      details::reflectTable(args.tables[i++], table);
    });
  }

  template<class TableT>
  void addTable(TableT& table, RuntimeDatabaseArgs& args) {
    const size_t baseIndex = args.tables.size();
    const size_t newTables = 1;
    args.tables.resize(baseIndex + newTables);
    details::reflectTable(args.tables[baseIndex], table);
  }

  template<class DB>
  void addDatabase(RuntimeDatabaseArgs& args) {
    //Create a class to store the database
    struct Storage : ChainedRuntimeStorage {
      using ChainedRuntimeStorage::ChainedRuntimeStorage;
      DB db;
    };
    //Add the storage to the args
    Storage* s = RuntimeStorage::addToChain<Storage>(args);
    //Add the tables in the new database to args
    reflect(s->db, args);
  }

  void addStableMappings(RuntimeDatabaseArgs& args, std::unique_ptr<StableElementMappings> mappings);
  RuntimeDatabaseArgs createArgsWithMappings();
}