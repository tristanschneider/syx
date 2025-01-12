#pragma once

#include "QueryAlias.h"
#include "RuntimeTable.h"
#include "generics/DynamicBitset.h"

template<class RowT>
using QueryResultRow = std::vector<RowT*>;

template<class T> struct TestT : std::true_type {};

template<class T>
struct IterableRow {
  T* row{};
  size_t size{};

  typename T::IteratorT begin() const {
    return row->begin();
  }

  typename T::IteratorT end() const {
    return row->begin() + size;
  }

  typename T::ConstIteratorT cbegin() const {
    return row->begin();
  }

  typename T::ConstIteratorT cend() const {
    return row->begin() + size;
  }

  T* operator->() const {
    return row;
  }

  T& operator*() const {
    return *row;
  }

  T* get() const {
    return row;
  }

  explicit operator bool() const {
    return row != nullptr;
  }
};

template<class... Rows>
struct QueryResultBuilder {
  using TupleT = std::tuple<QueryResultRow<Rows>...>;
  TupleT tuple;
  std::vector<const RuntimeTable*> tables;
};

class QueryResultBase {
public:
  class Iterator {
  public:
    using value_type = TableID;
    using pointer = value_type*;
    using reference = value_type&;

    using TablePtr = const RuntimeTable*;

    Iterator(const TablePtr* t)
      : table{ t }
    {
    }

    Iterator& operator++() {
      ++table;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp{ *this };
      ++*this;
      return tmp;
    }

    value_type operator*() const {
      return (*table)->getID();
    }

    auto operator<=>(const Iterator&) const = default;

  private:
    const TablePtr* table{};
  };

  QueryResultBase() = default;

  QueryResultBase(std::vector<const RuntimeTable*>&& t)
    : tables{ std::move(t) }
  {
  }

  size_t size() const {
    return tables.size();
  }

  std::vector<TableID> getMatchingTableIDs() {
    std::vector<TableID> result(tables.size());
    std::transform(tables.begin(), tables.end(), result.begin(), [](const RuntimeTable* t) { return t->getID(); });
    return result;
  }

  TableID getTableID(size_t i) const {
    return i < tables.size() ? tables.at(i)->getID() : TableID{};
  }

  size_t tableSize(size_t i) const {
    return i < tables.size() ? tables.at(i)->size() : 0;
  }

  TableID tryGet() const {
    return getTableID(0);
  }

  TableID operator[](size_t i) const {
    return getTableID(i);
  }

  Iterator begin() const {
    return { tables.size() ? &tables[0] : nullptr };
  }

  Iterator end() const {
    return { tables.size() ? (tables.data() + tables.size()) : nullptr };
  }

  bool contains(const TableID& id) const {
    return std::any_of(tables.begin(), tables.end(), [&id](const RuntimeTable* t) { return t->getID() == id; });
  }

private:
  std::vector<const RuntimeTable*> tables;
};

template<class... Rows>
class QueryResult : public QueryResultBase {
public:
  static_assert((isRow<Rows>() && ...), "Should only be used for rows");
  static_assert((!isNestedRow<Rows>() && ...), "Nested row is likely unintentional");
  using TupleT = std::tuple<QueryResultRow<Rows>...>;
  using IndicesT = std::index_sequence_for<Rows...>;

  QueryResult() = default;
  QueryResult(QueryResultBuilder<Rows...>&& b)
    : QueryResultBase{ std::move(b.tables) }
    , rows{ std::move(b.tuple) }
  {
  }

  template<class CB>
  void forEachElement(const CB& cb) {
    for(size_t i = 0; i < size(); ++i) {
      UnpackedDatabaseElementID id = getTableID(i);
      for(size_t e = 0; e < std::get<0>(rows).at(i)->size(); ++e) {
        if constexpr(std::is_invocable_v<CB, typename Rows::ElementT&...>) {
          cb(std::get<std::vector<Rows*>>(rows).at(i)->at(e)...);
        }
        else if constexpr(std::is_invocable_v<CB, UnpackedDatabaseElementID, typename Rows::ElementT&...>) {
          id.remakeElement(e);
          cb(id, std::get<std::vector<Rows*>>(rows).at(i)->at(e)...);
        }
      }
    }
  }

  template<class CB>
  void forEachRow(CB&& cb) {
    for(size_t i = 0; i < size(); ++i) {
      if constexpr(std::is_invocable_v<CB, Rows&...>) {
        cb(*std::get<std::vector<Rows*>>(rows).at(i)...);
      }
      else if constexpr(std::is_invocable_v<CB, UnpackedDatabaseElementID, Rows&...>) {
        cb(getTableID(i), *std::get<std::vector<Rows*>>(rows).at(i)...);
      }
    }
  }

  struct details {
    template<class TupleT, size_t... I>
    static auto get(size_t i, TupleT& tuple, std::index_sequence<I...>, size_t tableSize) {
      return std::make_tuple(IterableRow{ std::get<I>(tuple).at(i), tableSize }...);
    }
  };

  //Get all rows of a given table.
  std::tuple<IterableRow<Rows>...> get(size_t i) {
    return details::get(i, rows, IndicesT{}, tableSize(i));
  }

  std::tuple<IterableRow<Rows>...> getSingleton() {
    return get(0);
  }

  template<size_t I>
  auto getSingleton() {
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
  IterableRow<RowT> get(size_t tableIndex) {
    return { std::get<std::vector<RowT*>>(rows).at(tableIndex), tableSize(tableIndex) };
  }

  template<class RowT>
  IterableRow<RowT> tryGet(size_t tableIndex) {
    auto& rrow = std::get<std::vector<RowT*>>(rows);
    if(rrow.size() > tableIndex) {
      return { rrow.at(tableIndex), tableSize(tableIndex) };
    }
    return {};
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

private:
  TupleT rows;
};

struct RuntimeDatabaseArgs {
  std::vector<RuntimeTableRowBuilder> tables;
  StableElementMappings* mappings{};
  std::unique_ptr<IRuntimeStorage> storage;
  DatabaseIndex dbIndex{};
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
  QueryResultBase query();

  template<class... Rows>
  QueryResult<Rows...> query() {
    if constexpr(sizeof...(Rows) == 0) {
      return query();
    }
    else {
      QueryResultBuilder<Rows...> result;
      result.tables.reserve(tables.size());
      for(size_t i = 0; i < tables.size(); ++i) {
        _tryAddResult(i, result);
      }
      return result;
    }
  }

  template<class... Rows>
  QueryResult<Rows...> query(const TableID& id) {
    QueryResultBuilder<Rows...> result;
    const size_t index = id.getTableIndex();
    if(index < tables.size()) {
      _tryAddResult(index, result);
    }
    return result;
  }

  template<class... Aliases>
  auto queryAlias(const Aliases&... aliases) {
    if constexpr(sizeof...(Aliases) == 0) {
      return QueryAliasBase{ query() };
    }
    else {
      QueryResultBuilder<typename Aliases::RowT...> result;
      result.tables.reserve(tables.size());
      for(size_t i = 0; i < tables.size(); ++i) {
        _tryAddAliasResult(i, result, aliases...);
      }
      return QueryResult<typename Aliases::RowT...>{ std::move(result) };
    }
  }

  template<class... Aliases>
  auto queryAlias(const TableID& id, const Aliases&... aliases) {
    QueryResultBuilder<typename Aliases::RowT...> result;
    const size_t index = id.getTableIndex();
    if(index < tables.size()) {
      _tryAddAliasResult(index, result, aliases...);
    }
    return result;
  }

  QueryResultBase queryAliasTables(std::initializer_list<QueryAliasBase> aliases) const;

  DatabaseDescription getDescription();
  StableElementMappings& getMappings();

  RuntimeTable& operator[](size_t i);
  size_t size() const;

  void setTableDirty(size_t i);
  void setTableDirty(const TableID& t);
  const gnx::DynamicBitset& getDirtyTables() const;
  void clearDirtyTables();

private:
  template<class... Rows>
  void _tryAddResult(size_t index, QueryResultBuilder<Rows...>& result) {
    RuntimeTable& table = tables[index];
    std::tuple<Rows*...> rows{ table.tryGet<std::decay_t<Rows>>()... };
    const bool allFound = (std::get<Rows*>(rows) && ...);
    if(allFound) {
      result.tables.push_back(&table);
      (std::get<std::vector<Rows*>>(result.tuple).push_back(std::get<std::decay_t<Rows*>>(rows)), ...);
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
  void _tryAddAliasResult(size_t index, QueryResultBuilder<Rows...>& result, const Aliases&... aliases) {
    constexpr auto indices = std::index_sequence_for<Rows...>{};
    RuntimeTable& table = tables[index];
    std::tuple<Rows*...> rows{ aliases.cast(table.tryGet(aliases.type))... };
    if(areAllNotNull(rows, indices)) {
      result.tables.push_back(&table);
      copyAll(rows, result.tuple, indices);
    }
  }

  TableID getTableID(size_t index) const;

  std::vector<RuntimeTable> tables;
  StableElementMappings* mappings{};
  std::unique_ptr<IRuntimeStorage> storage;
  gnx::DynamicBitset dirtyTables;
  DatabaseIndex databaseIndex{};
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
      builder.tableType = DBTypeID::get<std::decay_t<TableT>>();
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