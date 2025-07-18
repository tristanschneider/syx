#pragma once

#include "QueryAlias.h"
#include "RuntimeTable.h"
#include "generics/DynamicBitset.h"

namespace TableName {
  struct TableName;
}

template<class RowT>
using QueryResultRow = std::vector<RowT*>;

template<class T>
concept UsesBeginEndIterators = requires(T t) {
  { t.begin() };
  { t.end() };
};
template<class T>
concept UsesRandomAccessIterator = !UsesBeginEndIterators<T> && requires(T t, size_t i) {
  { t.begin() + i };
};

template<class T>
struct IterableRowBase {
  IterableRowBase(T* r, size_t sz)
    : row{ r }
    , size{ sz }
  {}

  T* row{};
  size_t size{};

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

//Default doesn't expose iterators, just the underlying row
template<class T>
struct IterableRow : IterableRowBase<T> {
  using IterableRowBase<T>::IterableRowBase;
};

//If end iterator is exposed, use that
template<UsesBeginEndIterators T>
struct IterableRow<T> : IterableRowBase<T> {
  using IterableRowBase<T>::IterableRowBase;

  //May be iterator or const iterator depending on if the wrapped row is const
  auto begin() const {
    return this->row->begin();
  }

  auto end() const {
    return this->row->end();
  }

  typename T::ConstIteratorT cbegin() const {
    return this->row->begin();
  }

  typename T::ConstIteratorT cend() const {
    return this->row->end();
  }
};

//If it's a random access iterator, use that to generate a range based off of begin and size
template<UsesRandomAccessIterator T>
struct IterableRow<T> : IterableRowBase<T> {
  using IterableRowBase<T>::IterableRowBase;

  typename T::IteratorT begin() const {
    return this->row->begin();
  }

  typename T::IteratorT end() const {
    return this->row->begin() + this->size;
  }

  typename T::ConstIteratorT cbegin() const {
    return this->row->begin();
  }

  typename T::ConstIteratorT cend() const {
    return this->row->begin() + this->size;
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

  //Silly workaround for ambiguity of method names that match between QueryResult<...> and QueryResultBase
  QueryResultBase& base() {
    return *this;
  }

  const QueryResultBase& base() const {
    return *this;
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
  ChainedRuntimeStorage(std::unique_ptr<IRuntimeStorage> root)
    : child{ std::move(root) } {
  }

  std::unique_ptr<IRuntimeStorage> child;
};

template<class T>
struct TChainedRuntimeStorage : ChainedRuntimeStorage {
  using ChainedRuntimeStorage::ChainedRuntimeStorage;
  T value;
};

namespace RuntimeStorage {
  template<class T>
  concept HasChainedStorageBase = std::is_base_of_v<ChainedRuntimeStorage, T>;
  template<class T>
  concept IsRuntimeArgsConstructable = requires(std::unique_ptr<IRuntimeStorage> args) {
    T{ std::move(args) };
  };

  template<class T>
  concept ChainedStorage = HasChainedStorageBase<T> && IsRuntimeArgsConstructable<T>;

  template<ChainedStorage T>
  T* addToChain(std::unique_ptr<IRuntimeStorage>& root) {
    std::unique_ptr<T> result = std::make_unique<T>(std::move(root));
    T* ptr = result.get();
    root = std::move(result);
    return ptr;
  }

  template<ChainedStorage T>
  T* addToChain(RuntimeDatabaseArgs& args) {
    return addToChain<T>(args.storage);
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
    std::tuple<Rows*...> rows{ (aliases ? aliases.cast(table.tryGet(aliases.type)) : nullptr)... };
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

  template<IsRow RowT>
  void addRow(RowT& row, RuntimeTableRowBuilder& table) {
    details::reflectRow(row, table);
  }

  template<class DB>
  void addDatabase(RuntimeDatabaseArgs& args) {
    //Add the storage to the args
    DB& s = RuntimeStorage::addToChain<TChainedRuntimeStorage<DB>>(args)->value;
    //Add the tables in the new database to args
    reflect(s, args);
  }

  template<class T>
  void addTable(RuntimeDatabaseArgs& args) {
    addTable(RuntimeStorage::addToChain<TChainedRuntimeStorage<T>>(args)->value, args);
  }

  void addStableMappings(RuntimeDatabaseArgs& args, std::unique_ptr<StableElementMappings> mappings);
  RuntimeDatabaseArgs createArgsWithMappings();
}

//Allows incrementally building up a table through addRow calls that can then be inserted into
//RuntimeDatabaseArgs with `finalize`
class StorageTableBuilder {
public:
  template<IsRow... Rows>
  StorageTableBuilder& addRows() {
    (addRow<Rows>(), ...);
    return *this;
  }

  RuntimeTableRowBuilder& operator*() { return mBuilder; }
  const RuntimeTableRowBuilder& operator*() const { return mBuilder; }
  RuntimeTableRowBuilder* operator->() { return &mBuilder; }
  const RuntimeTableRowBuilder* operator->() const { return &mBuilder; }

  StorageTableBuilder& setTableName(TableName::TableName&& name);
  StorageTableBuilder& setStable();

  void finalize(RuntimeDatabaseArgs& args)&&;

private:
  template<IsDefaultConstructibleRow R>
  R& addRow() {
    struct RowStorage : ChainedRuntimeStorage {
      using ChainedRuntimeStorage::ChainedRuntimeStorage;
      R row;
    };
    R& result = RuntimeStorage::addToChain<RowStorage>(mStorage)->row;
    DBReflect::addRow(result, mBuilder);
    return result;
  }

  RuntimeTableRowBuilder mBuilder;
  std::unique_ptr<IRuntimeStorage> mStorage;
};