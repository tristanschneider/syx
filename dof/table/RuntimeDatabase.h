#pragma once

#include "TableOperations.h"
#include "TypeId.h"

using DBTypeID = TypeID<struct DBTypeT>;

struct QueryAliasBase {
  DBTypeID type;
  bool isConst{};
};

//Alias for a query whose original type is stored in ID but unkown to the caller,
//which is cast to ResultT
template<class ResultT>
struct QueryAlias : QueryAliasBase {
  using RowT = ResultT;
  static_assert(isRow<RowT>(), "Should only be used for rows");
  static_assert(!isNestedRow<RowT>(), "Nested row is likely unintentional");

  template<class SourceT>
  static QueryAlias create() {
    static_assert(std::is_convertible_v<SourceT*, ResultT*>);
    static_assert(std::is_const_v<SourceT> == std::is_const_v<ResultT>);
    struct Caster {
      static ResultT* cast(void* p) {
        return static_cast<ResultT*>(static_cast<SourceT*>(p));
      }
      static const ResultT* constCast(void* p) {
        return cast(p);
      }
    };
    QueryAlias result;
    result.cast = &Caster::cast;
    result.constCast = &Caster::constCast;
    result.type = DBTypeID::get<std::decay_t<SourceT>>();
    result.isConst = false;
    return result;
  }

  //Alias of itself
  static QueryAlias create() {
    return create<ResultT>();
  }

  QueryAlias<const ResultT> read() const {
    QueryAlias<const ResultT> result;
    result.type = type;
    result.cast = constCast;
    result.constCast = constCast;
    result.isConst = true;
    return result;
  }

  ResultT*(*cast)(void*){};
  //Hack to enable the convenience of .read
  const ResultT*(*constCast)(void*){};
};

using FloatQueryAlias = QueryAlias<Row<float>>;

struct RuntimeRow {
  void* row{};
  void (*migrateOneElement)(void* from, void* to, const UnpackedDatabaseElementID& fromID, [[maybe_unused]] const UnpackedDatabaseElementID& toID, StableElementMappings& mappings){};
  void (*swapRemove)(void* row, const UnpackedDatabaseElementID& id, [[maybe_unused]] StableElementMappings& mappings){};
};

struct RuntimeTable {
  using IDT = DBTypeID;

  template<class RowT>
  RowT* tryGet() {
    auto it = rows.find(IDT::get<std::decay_t<RowT>>());
    return it != rows.end() ? (RowT*)it->second.row : nullptr;
  }

  void* tryGet(DBTypeID id) {
    auto it = rows.find(id);
    return it != rows.end() ? it->second.row : nullptr;
  }

  RuntimeRow* tryGetRow(DBTypeID id) {
    auto it = rows.find(id);
    return it != rows.end() ? &it->second : nullptr;
  }

  static void migrateOne(size_t i, RuntimeTable& from, RuntimeTable& to);

  UnpackedDatabaseElementID tableID;
  TableModifierInstance modifier;
  StableTableModifierInstance stableModifier;
  std::unordered_map<DBTypeID, RuntimeRow> rows;
};

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

  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
  TupleT rows;
};

struct RuntimeDatabaseArgs {
  size_t elementIndexBits;
  std::vector<RuntimeTable> tables;
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

  template<class... Aliases>
  auto queryAlias(const Aliases&... aliases) {
    if constexpr(sizeof...(Aliases) == 0) {
      return query();
    }
    else {
      QueryResult<typename Aliases::RowT...> result;
      result.matchingTableIDs.reserve(data.tables.size());
      for(size_t i = 0; i < data.tables.size(); ++i) {
        _tryAddAliasResult(i, result, aliases...);
      }
      return result;
    }
  }

  template<class... Aliases>
  auto queryAlias(const UnpackedDatabaseElementID& id, const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result;
    const size_t index = id.getTableIndex();
    if(index < data.tables.size()) {
      _tryAddAliasResult(index, result, aliases...);
    }
    return result;
  }

  DatabaseDescription getDescription();
  StableElementMappings& getMappings();

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
    std::tuple<Rows*...> rows{ aliases.cast(data.tables[index].tryGet(aliases.type))... };
    if(areAllNotNull(rows, indices)) {
      result.matchingTableIDs.push_back(getTableID(index));
      copyAll(rows, result.rows, indices);
    }
  }

  UnpackedDatabaseElementID getTableID(size_t index) const;

  RuntimeDatabaseArgs data;
};

struct IDatabase {
  virtual ~IDatabase() = default;
  virtual RuntimeDatabase& getRuntime() = 0;
};

//Specialization that provides all row ids
template<>
struct QueryResult<> {
  size_t size() const {
    return matchingTableIDs.size();
  }

  std::vector<UnpackedDatabaseElementID> matchingTableIDs;
};

namespace DBReflect {
  namespace details {
    template<class RowT>
    RuntimeRow createRuntimeRow(RowT& row) {
      static constexpr bool isStableRow = std::is_same_v<RowT, StableIDRow>;
      struct Funcs {
        static void migrateOneElement(void* from, void* to, const UnpackedDatabaseElementID& fromID, [[maybe_unused]] const UnpackedDatabaseElementID& toID, [[maybe_unused]] StableElementMappings& mappings) {
          RowT* fromT = static_cast<RowT*>(from);
          RowT* toT = static_cast<RowT*>(to);
          if constexpr(isStableRow) {
            StableOperations::migrateOne(*fromT, *toT, fromID, toID, mappings);
          }
          else {
            //Hack to deal with migration case where not all tables exist in source and destination
            if(fromT) {
              toT->emplaceBack(std::move(fromT->at(fromID.getElementIndex())));
            }
            else {
              toT->emplaceBack();
            }
          }
        }

        static void swapRemove(void* row, const UnpackedDatabaseElementID& id, [[maybe_unused]] StableElementMappings& mappings) {
          RowT* r = static_cast<RowT*>(row);
          if constexpr(isStableRow) {
            StableOperations::swapRemove(*r, id, mappings);
          }
          else {
            //Nonzero because there must be an element we're removing
            const size_t newSize = r->size() - 1;
            r->swap(id.getElementIndex(), newSize);
            r->resize(newSize);
          }
        }
      };
      return {
        &row,
        &Funcs::migrateOneElement,
        &Funcs::swapRemove
      };
    }

    constexpr size_t computeElementIndexBits(size_t tableCount) {
      constexpr size_t totalBits = sizeof(size_t)*8;
      return totalBits - dbDetails::constexprLog2(tableCount);
    }

    template<class RowT>
    void reflectRow(RowT& row, RuntimeTable& table) {
      table.rows[DBTypeID::get<RowT>()] = createRuntimeRow(row);
    }

    template<class TableT>
    void reflectTable(const UnpackedDatabaseElementID& tableID, TableT& table, RuntimeDatabaseArgs& args) {
      RuntimeTable& rt = args.tables[tableID.getTableIndex()];
      rt.tableID = tableID;
      if constexpr(TableOperations::isStableTable<TableT>) {
        rt.stableModifier = StableTableModifierInstance::get(table, tableID, *args.mappings);
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
    args.elementIndexBits = details::computeElementIndexBits(args.tables.size());
    args.mappings = &mappings;
    db.visitOne([&](auto& table) {
      const size_t rawIndex = DB::getTableIndex(table).getTableIndex();
      const auto tableID = UnpackedDatabaseElementID{ 0, args.elementIndexBits }.remake(baseIndex + rawIndex, 0);
      details::reflectTable(tableID, table, args);
    });
  }

  template<class TableT>
  void addTable(TableT& table, RuntimeDatabaseArgs& args, StableElementMappings& mappings) {
    const size_t baseIndex = args.tables.size();
    const size_t newTables = 1;
    args.tables.resize(baseIndex + newTables);
    args.elementIndexBits = details::computeElementIndexBits(args.tables.size());
    args.mappings = &mappings;
    const auto tableID = UnpackedDatabaseElementID{ 0, args.elementIndexBits }.remake(baseIndex, 0);
    details::reflectTable(tableID, table, args);
  }

  template<class DB>
  std::unique_ptr<IDatabase> createDatabase(StableElementMappings& mappings) {
    struct Impl : IDatabase {
      Impl(StableElementMappings& mappings)
        : runtime(getArgs(mappings)) {
      }

      RuntimeDatabase& getRuntime() override {
        return runtime;
      }

      RuntimeDatabaseArgs getArgs(StableElementMappings& mappings) {
        RuntimeDatabaseArgs result;
        reflect(db, result, mappings);
        return result;
      }

      DB db;
      RuntimeDatabase runtime;
    };
    return std::make_unique<Impl>(mappings);
  }

  std::unique_ptr<IDatabase> merge(std::unique_ptr<IDatabase> l, std::unique_ptr<IDatabase> r);
  std::unique_ptr<IDatabase> bundle(std::unique_ptr<IDatabase> db, std::unique_ptr<StableElementMappings> mappings);
}