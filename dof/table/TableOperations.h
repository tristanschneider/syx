#pragma once
#include "Database.h"
#include "StableElementID.h"

struct TableOperations {
  template<class RowT>
  static auto _unwrapWithOffset(RowT& r, size_t offset) {
    return r.mElements.data() + offset;
  }

  template<class RowT, class TableT>
  static decltype(std::declval<RowT&>().mElements.data()) _unwrapRowWithOffset(TableT& t, size_t offset) {
    if constexpr(TableOperations::hasRow<RowT, TableT>()) {
      return _unwrapWithOffset(std::get<RowT>(t.mRows), offset);
    }
    else {
      return nullptr;
    }
  }

  template<class RowT, class TableT>
  static auto unwrapRow(TableT& t, size_t offset = 0) {
    return _unwrapRowWithOffset<RowT, TableT>(t, offset);
  }

  template<class TableT>
  static constexpr bool isStableTable = TableT::HasRow<StableIDRow>;

  template<class TableT>
  static auto getElement(TableT& table, size_t index) {
    return table.visitAll([&index](auto&... rows) -> TableT::ElementRef {
      return make_duple(&rows.at(index)...);
    });
  }

  template<class TableT>
  static auto addToTable(TableT& table) {
    static_assert(!isStableTable<TableT>);
    return table.visitAll([](auto&... rows) -> TableT::ElementRef {
       return make_duple(&rows.emplaceBack()...);
    });
  }

  //Insert in the table before the iterator relative to the LeadingRow
  template<class LeadingRow, class TableT>
  static auto addToTableAt(TableT& table, LeadingRow& row, const typename LeadingRow::IteratorT& at) {
    static_assert(!isStableTable<TableT>);
    const auto insertIndex = std::distance(row.begin(), at);
    return table.visitAll([&](auto&... rows) {
      return make_duple(&rows.insert(rows.begin() + insertIndex, typename std::decay_t<decltype(rows)>::ElementT{})...);
    });
  }

  template<class LeadingRow, class TableT, class T>
  static auto addToSortedUniqueTable(TableT& table, T value) {
    static_assert(!isStableTable<TableT>);
    auto& row = std::get<LeadingRow>(table.mRows);
    //Find the sorted insert position by the leading row
    auto it = std::lower_bound(row.begin(), row.end(), value);

    //Get the relative offset of that sorted position to use for all other rows
    const auto insertIndex = std::distance(row.begin(), it);

    //If this isn't new, return the existing element, as this is supposed to be a unique list
    if(it != row.end() && *it == value) {
      return getElement(table, insertIndex);
    }

    auto result = addToTableAt(table, row, it);

    row.at(size_t(insertIndex)) = std::move(value);

    return result;
  }

  template<class LeadingRow, class TableT, class T>
  static auto addToSortedTable(TableT& table, T value) {
    static_assert(!isStableTable<TableT>);
    auto& row = std::get<LeadingRow>(table.mRows);
    //Find the sorted insert position by the leading row
    auto it = std::lower_bound(row.begin(), row.end(), value);
    //Get the relative offset of that sorted position to use for all other rows
    const auto insertIndex = std::distance(row.begin(), it);

    auto result = addToTableAt(table, row, it);

    row.at(size_t(insertIndex)) = std::move(value);

    return result;
  }

  template<class TableT>
  static void resizeTable(TableT& table, size_t newSize) {
    static_assert(!isStableTable<TableT>);
    table.visitOne([newSize](auto& row) {
      row.resize(newSize);
    });
  }

  //Reserved keys can optionally be used to provide a key in the place of newly generated keys that have been created through StableElementMappings
  //This can be used to immediately reserve keys for elements in tables that are inserted later
  template<class TableT>
  static void stableResizeTable(TableT& table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    static_assert(isStableTable<TableT>);
    table.visitOne([&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::resize(row, id, newSize, mappings, reservedKeys);
      }
      else {
        row.resize(newSize);
      }
    });
  }

  template<class DB, class TableT>
  static void stableResizeTable(TableT& table, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    stableResizeTable(table, UnpackedDatabaseElementID::fromPacked(DB::getTableIndex<TableT>()), newSize, mappings, reservedKeys);
  }

  template<class TableT, class DB>
  static void stableResizeTableDB(DB& db, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    stableResizeTable<DB>(std::get<TableT>(db.mTables), newSize, mappings, reservedKeys);
  }

  //Insert before location, meaning the value of location will be after the newly created gap
  template<class TableT>
  static void stableInsertRangeAt(TableT& table, const UnpackedDatabaseElementID& location, size_t count, StableElementMappings& mappings) {
    const size_t oldSize = TableOperations::size(table);
    const size_t newSize = oldSize + count;
    //Add space for the new elements to insert
    stableResizeTable(table, location, newSize, mappings);
    const size_t toSwap = oldSize - std::min(oldSize, location.getElementIndex());
    //Shift all elements after the range into the newly created space
    table.visitOne([&](auto& row) {
      for(size_t i = 0; i < toSwap; ++i) {
        //From right to left, take the elements from their old end and shift them up by count
        const size_t src = oldSize - i - 1;
        const size_t dst = src + count;
        using RowT = std::decay_t<decltype(row)>;
        //Stable needs to swap to preserve the mappings, unstable can assign over
        if constexpr(std::is_same_v<RowT, StableIDRow>) {
          StableOperations::swap(row, location.remake(location.getTableIndex(), src), location.remake(location.getTableIndex(), dst), mappings);
        }
        else {
          row.at(dst) = std::move(row.at(src));
        }
      }
    });
  }

  template<class TableT>
  static void insertRangeAt(TableT& table, const UnpackedDatabaseElementID& location, size_t count) {
    const size_t oldSize = TableOperations::size(table);
    const size_t newSize = oldSize + count;
    //Add space for the new elements to insert
    resizeTable(table, newSize);
    const size_t toSwap = oldSize - std::min(oldSize, location.getElementIndex());
    //Shift all elements after the range into the newly created space
    table.visitOne([&](auto& row) {
      for(size_t i = 0; i < toSwap; ++i) {
        //From right to left, take the elements from their old end and shift them up by count
        const size_t src = oldSize - i - 1;
        const size_t dst = src + count;
        using RowT = std::decay_t<decltype(row)>;
        row.at(dst) = std::move(row.at(src));
      }
    });
  }

  template<class TableT>
  static size_t size(const TableT& table) {
    //They're all the same size, so getting any will be the correct size
    return std::get<0>(table.mRows).size();
  }

  template<class RowT, class TableT>
  static auto& getRow(TableT& table) {
    return std::get<RowT>(table.mRows);
  }

  template<class RowT, class TableT>
  static const auto& getRow(const TableT& table) {
    return std::get<RowT>(table.mRows);
  }

  template<class TableT>
  static void swapRemove(TableT& table, size_t index) {
    static_assert(!isStableTable<TableT>);
    const size_t myLast = size(table) - 1;
    table.visitOne([index, myLast](auto& row) {
      row.swap(index, myLast);
      row.resize(myLast);
    });
  }

  template<class TableT>
  static void stableSwapRemove(TableT& table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
    static_assert(isStableTable<TableT>);
    const size_t myLast = size(table) - 1;
    table.visitOne([&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::swapRemove(row, id, mappings);
      }
      else {
        row.swap(id.getElementIndex(), myLast);
        row.resize(myLast);
      }
    });
  }

  using StableSwapRemover = void(*)(void*, const UnpackedDatabaseElementID& id, StableElementMappings& mappings);
  template<class TableT>
  static constexpr StableSwapRemover getStableSwapRemove() {
    struct Adapter {
      static void f(void* table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
        stableSwapRemove(*static_cast<TableT*>(table), id, mappings);
      }
    };
    return &Adapter::f;
  }

  template<class TableT, size_t S>
  static void stableSwapRemove(TableT& table, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    stableSwapRemove(table, UnpackedDatabaseElementID::fromPacked(id), mappings);
  }

  template<class TableT>
  static void sortedRemove(TableT& table, size_t index) {
    static_assert(!isStableTable<TableT>);
    table.visitOne([index](auto& table) {
      table.erase(table.begin() + index);
    });
  }

  template<class T, class TableT>
  constexpr static bool hasRow() {
    return TableT::template HasRow<T>;
  }

  template<class T, class TableT>
  constexpr static bool hasRow(const TableT&) {
    return TableT::template HasRow<T>;
  }

  template<class T, class TableT>
  constexpr static T* tryGetRow(TableT& t) {
    if constexpr(hasRow<T, TableT>()) {
      return &getRow<T>(t);
    }
    else {
      return nullptr;
    }
  }

  template<class T, class TableT>
  constexpr static const T* tryGetRow(const TableT& t) {
    if constexpr(hasRow<T>(t)) {
      return &getRow<T>(t);
    }
    else {
      return nullptr;
    }
  }

  template<class SrcTable, class DstTable>
  static void migrateOne(SrcTable& src, DstTable& dst, size_t index) {
    static_assert(!isStableTable<SrcTable> && !isStableTable<DstTable>);
    //Move all common rows to the destination
    dst.visitOne([&](auto& dstRow) {
      using RowT = std::decay_t<decltype(dstRow)>;
      if constexpr(hasRow<RowT, SrcTable>()) {
        //They both have the row, move the value over
        dstRow.emplaceBack(std::move(getRow<RowT>(src).at(index)));
      }
      else {
        //Only the destination has this row, default construct the value
        dstRow.emplaceBack();
      }
    });
    //Remove from source. Could be faster to combine this with the above step while visiting,
    //but is more confusing when accounting for cases where src has rows dst doesn't
    swapRemove(src, index);
  }

  template<class SrcTable, class DstTable, size_t S>
  static void stableMigrateOne(SrcTable& src, DstTable& dst, const DatabaseElementID<S>& fromID, const DatabaseElementID<S>& toID, StableElementMappings& mappings) {
    static_assert(isStableTable<SrcTable> && isStableTable<DstTable>);
    const size_t index = fromID.getElementIndex();
    StableIDRow& sourceStableIDs = std::get<StableIDRow>(src.mRows);

    //Move all common rows to the destination
    dst.visitOne([&](auto& dstRow) {
      using RowT = std::decay_t<decltype(dstRow)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::migrateOne(sourceStableIDs, dstRow, fromID, toID, mappings);
      }
      else if constexpr(hasRow<RowT, SrcTable>()) {
        //They both have the row, move the value over
        dstRow.emplaceBack(std::move(getRow<RowT>(src).at(index)));
      }
      else {
        //Only the destination has this row, default construct the value
        dstRow.emplaceBack();
      }
    });
    //Swap Remove from source. Could be faster to combine this with the above step while visiting,
    //but is more confusing when accounting for cases where src has rows dst doesn't
    //Skip stable row because that was already addressed in the migrate above
    const size_t myLast = size(src) - 1;
    src.visitOne([index, myLast](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(!std::is_same_v<RowT, StableIDRow>) {
        row.swap(index, myLast);
        row.resize(myLast);
      }
    });
  }

  template<class SrcTable, class DstTable>
  static void migrateAll(SrcTable& src, DstTable& dst) {
    static_assert(!isStableTable<SrcTable> && !isStableTable<DstTable>);
    //Silly implementation, will optimize to row by row if used enough
    while(size(src)) {
      migrateOne(src, dst, 0);
    }
  }
};

//Wraps the operations needed to modify any table in a single type, with the table itself passed through via void*
struct StableTableModifier {
  template<class TableT>
  static StableTableModifier get() {
    struct Adapter {
      static void resize(void* table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings) {
        TableOperations::stableResizeTable(*static_cast<TableT*>(table), id, newSize, mappings);
      }

      static size_t size(const void* table) {
        return TableOperations::size(*static_cast<const TableT*>(table));
      }

      static void swapRemove(void* table const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
        TableOperations::stableSwapRemove(*static_cast<TableT*>(table), id, mappings);
      }

      static void insert(void* table, const UnpackedDatabaseElementID& location, size_t count, StableElementMappings& mappings) {
        TableOperations::stableInsertRangeAt(static_cast<TableT*>(table), location, count, mappings);
      }
    };

    return {
      &Adapter::resize,
      &Adapter::size,
      &Adapter::swapRemove,
      &Adapter::insert
    };
  }

  void(*resize)(void* table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings){};
  size_t(*size)(const void* table){};
  void(*swapRemove)(void* table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings){};
  void(*insert)(void* table, const UnpackedDatabaseElementID& id, size_t count, StableElementMappings& mappings){};
};

struct TableModifier {
  template<class TableT>
  static TableModifier get() {
    struct Adapter {
      static void resize(void* table, size_t newSize) {
        TableOperations::resizeTable<TableT>(*static_cast<TableT*>(table), newSize);
      }

      static size_t size(const void* table) {
        return TableOperations::size(*static_cast<const TableT*>(table));
      }

      static void swapRemove(void* table, const UnpackedDatabaseElementID& id) {
        TableOperations::swapRemove(*static_cast<TableT*>(table), id.getElementIndex());
      }

      static void insert(void* table, const UnpackedDatabaseElementID& location, size_t count) {
        TableOperations::insertRangeAt(static_cast<TableT*>(table), location, count);
      }
    };

    return {
      &Adapter::resize,
      &Adapter::size,
      &Adapter::swapRemove,
      &Adapter::insert
    };
  }

  void(*resize)(void* table, size_t newSize){};
  size_t(*size)(const void* table){};
  void(*swapRemove)(void* table, const UnpackedDatabaseElementID& id){};
  void(*insert)(void* table, const UnpackedDatabaseElementID& id, size_t count){};
};

//Wraps the operations needed to modify any table with an included instance
//Sort of manual vtable, although trying to keep inheritance out of tables
struct StableTableModifierInstance {
  template<class DB, class TableT>
  static StableTableModifierInstance get(TableT& table, StableElementMappings& mappings) {
    return {
      StableTableModifier::get<TableT>(),
      &table,
      &mappings,
      UnpackedDatabaseElementID::fromPacked(DB::getTableIndex<TableT>())
    };
  }

  template<class TableT>
  static StableTableModifierInstance get(TableT& table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
    return {
      StableTableModifier::get<TableT>(),
      &table,
      &mappings,
      id
    };
  }

  template<class TableT, class DB>
  static StableTableModifierInstance getDB(DB& db, StableElementMappings& mappings) {
    return get<DB, TableT>(std::get<TableT>(db.mTables), mappings);
  }

  //Add the requested amount of elements and return the index of the first new one
  size_t addElements(size_t count) {
    const size_t first = modifier.size(table);
    modifier.resize(table, tableID, count + first, *stableMappings);
    return first;
  }

  void resize(size_t count) {
    modifier.resize(table, tableID, count, *stableMappings);
  }

  operator bool() const {
    return table != nullptr;
  }

  //Currently several function pointers on each instance, modifier could be a singleton if it matters
  StableTableModifier modifier;
  void* table{};
  StableElementMappings* stableMappings{};
  UnpackedDatabaseElementID tableID;
};

struct TableModifierInstance {
  template<class TableT>
  static TableModifierInstance get(TableT& table) {
    return {
      TableModifier::get<TableT>(),
      &table
    };
  }

  template<class TableT, class DB>
  static TableModifierInstance getDB(DB& db) {
    return get(std::get<TableT>(db.mTables));
  }

  operator bool() const {
    return table != nullptr;
  }

  //Add the requested amount of elements and return the index of the first new one
  size_t addElements(size_t count) {
    const size_t first = modifier.size(table);
    modifier.resize(table, count + first);
    return first;
  }

  void resize(size_t count) {
    modifier.resize(table, count);
  }

  //Currently several function pointers on each instance, modifier could be a singleton if it matters
  TableModifier modifier;
  void* table{};
};


//Intended for use when iterating over many ids, this reuses the fetched table in the common case,
//then swaps it out if the id is from a different table. The assumption is that large groups of
//ids are all from the same table
template<class T>
struct CachedRow {
  operator bool() const { return row; }
  const T* operator->() const { return row; }
  T* operator->() { return row; }
  const T& operator*() const { return *row; }
  T& operator*() { return *row; }

  T* row{};
  size_t tableID{};
};

//Exposes the ability to query the given rows in any table without a direct dependency on the database
template<class... Rows>
struct TableResolver {
  template<class DB>
  static TableResolver create(DB& db) {
    return {
      &db,
      std::make_tuple(&TableResolver::tryGetRow<Rows, DB>...)
    };
  }

  template<class Row, class DB>
  static Row* tryGetRow(void* db, const UnpackedDatabaseElementID& id) {
    return Queries::getRowInTable<Row>(*static_cast<DB*>(db), typename DB::ElementID{ id.mValue });
  }
  template<class Row>
  using GetRowT = Row*(*)(void*, const UnpackedDatabaseElementID&);

  template<class Row>
  Row* tryGetRow(const UnpackedDatabaseElementID& id) {
    return std::get<GetRowT<Row>>(getters)(db, id);
  }

  template<class Row>
  bool tryGetOrSwapRow(CachedRow<Row>& row, const UnpackedDatabaseElementID& id) {
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRow<Row>(id);
      row.tableID = id.getTableIndex();
      return row.row != nullptr;
    }
    return row.row != nullptr;
  }

  template<class... Args>
  bool tryGetOrSwapAllRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) && ...);
  }

  template<class... Args>
  bool tryGetOrSwapAnyRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) || ...);
  }

  void* db{};
  std::tuple<GetRowT<Rows>...> getters;
};
