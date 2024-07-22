#pragma once
#include "Database.h"
#include "StableElementID.h"

//TODO: responsibility of TableOperations vs Queries is unclear
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

  template<class Row, class DatabaseT>
  Row* getRowInTable(DatabaseT& db, typename DatabaseT::ElementID id) {
    Row* result = nullptr;
    db.visitOneByIndex(id, [&](auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>()) {
        result = &TableOperations::getRow<Row>(table);
      }
    });
    return result;
  }

  template<class Row, class DatabaseT>
  const Row* getRowInTable(const DatabaseT& db, typename DatabaseT::ElementID id) {
    const Row* result = nullptr;
    db.visitOneByIndex(id, [&](const auto& table) {
      using TableT = std::decay_t<decltype(table)>;
      if constexpr(TableOperations::hasRow<Row, TableT>()) {
        result = &TableOperations::getRow<Row>(table);
      }
    });
    return result;
  }

  template<class RowT, class TableT>
  static auto unwrapRow(TableT& t, size_t offset = 0) {
    return _unwrapRowWithOffset<RowT, TableT>(t, offset);
  }

  template<class TableT>
  static constexpr bool isStableTable = TableT::template HasRow<StableIDRow>;

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

  template<class TableT>
  static void insertRangeAt(TableT& table, const UnpackedDatabaseElementID& location, size_t count) {
    const size_t oldSize = TableOperations::size(table);
    const size_t newSize = oldSize + count;
    //Add space for the new elements to insert
    resizeTable(table, newSize);
    const size_t toSwap = oldSize - std::min(oldSize, location.getElementIndex());
    //Shift all elements after the range into the newly created space
    table.visitOne([&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_move_assignable_v<typename RowT::ElementT>) {
        for(size_t i = 0; i < toSwap; ++i) {
          //From right to left, take the elements from their old end and shift them up by count
          const size_t src = oldSize - i - 1;
          const size_t dst = src + count;
          using RowT = std::decay_t<decltype(row)>;
          row.at(dst) = std::move(row.at(src));
        }
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

  template<class SrcTable, class DstTable>
  static void migrateAll(SrcTable& src, DstTable& dst) {
    static_assert(!isStableTable<SrcTable> && !isStableTable<DstTable>);
    //Silly implementation, will optimize to row by row if used enough
    while(size(src)) {
      migrateOne(src, dst, 0);
    }
  }
};


struct StableOperations {
  struct details {
    template<class Row, class DatabaseT>
    static Row* getRowInTable(DatabaseT& db, typename DatabaseT::ElementID id) {
      Row* result = nullptr;
      db.visitOneByIndex(id, [&](auto& table) {
        using TableT = std::decay_t<decltype(table)>;
        if constexpr(TableOperations::template hasRow<Row, TableT>()) {
          result = &TableOperations::template getRow<Row>(table);
        }
      });
      return result;
    }

    static bool isUnstableElementValid(const StableElementID& id, const StableIDRow& ids, size_t elementMask) {
      const size_t unstableIndex = id.mUnstableIndex & elementMask;
      //Validate id, if it matches what's at the location it's fine as is
      return ids.size() > unstableIndex && ids.at(unstableIndex) == id.mStableID;
    }

    template<class... Tables>
    static bool isUnstableElementValid(Database<Tables...>& db, const StableElementID& id) {
      if(id.mUnstableIndex == dbDetails::INVALID_VALUE) {
        return false;
      }
      using ElementIDT = typename Database<Tables...>::ElementID;
      const ElementIDT unstableElement{ id.mUnstableIndex };
      const size_t unstableIndex = unstableElement.getElementIndex();

      const StableIDRow* ids = getRowInTable<StableIDRow>(db, unstableElement);
      assert(ids && "Element should at least be pointing at a table that has a stable id row");
      if(!ids) {
        return false;
      }
      //Validate id, if it matches what's at the location it's fine as is
      return ids->size() > unstableIndex && ids->at(unstableIndex) == id.mStableID;
    }
  };

  template<class TableT>
  static void stableSwapRemove(TableT& table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
    static_assert(TableOperations::isStableTable<TableT>);
    const size_t myLast = TableOperations::size(table) - 1;
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

  //Reserved keys can optionally be used to provide a key in the place of newly generated keys that have been created through StableElementMappings
  //This can be used to immediately reserve keys for elements in tables that are inserted later
  template<class TableT>
  static void stableResizeTable(TableT& table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    static_assert(TableOperations::isStableTable<TableT>);
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

  template<class SrcTable, class DstTable, size_t S>
  static void stableMigrateOne(SrcTable& src, DstTable& dst, const DatabaseElementID<S>& fromID, const DatabaseElementID<S>& toID, StableElementMappings& mappings) {
    static_assert(TableOperations::isStableTable<SrcTable> && TableOperations::isStableTable<DstTable>);
    const size_t index = fromID.getElementIndex();
    StableIDRow& sourceStableIDs = std::get<StableIDRow>(src.mRows);

    //Move all common rows to the destination
    dst.visitOne([&](auto& dstRow) {
      using RowT = std::decay_t<decltype(dstRow)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::migrateOne(sourceStableIDs, dstRow, fromID, toID, mappings);
      }
      else if constexpr(TableOperations::hasRow<RowT, SrcTable>()) {
        //They both have the row, move the value over
        dstRow.emplaceBack(std::move(TableOperations::getRow<RowT>(src).at(index)));
      }
      else {
        //Only the destination has this row, default construct the value
        dstRow.emplaceBack();
      }
    });
    //Swap Remove from source. Could be faster to combine this with the above step while visiting,
    //but is more confusing when accounting for cases where src has rows dst doesn't
    //Skip stable row because that was already addressed in the migrate above
    const size_t myLast = TableOperations::size(src) - 1;
    src.visitOne([index, myLast](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(!std::is_same_v<RowT, StableIDRow>) {
        row.swap(index, myLast);
        row.resize(myLast);
      }
    });
  }

  template<class DB, class TableT>
  static void stableResizeTable(TableT& table, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    stableResizeTable(table, UnpackedDatabaseElementID::fromPacked(DB::template getTableIndex<TableT>()), newSize, mappings, reservedKeys);
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
        else if constexpr(std::is_move_assignable_v<typename RowT::ElementT>) {
          row.at(dst) = std::move(row.at(src));
        }
      }
    });
  }

  template<size_t S>
  static StableElementID getStableID(const StableIDRow& ids, const DatabaseElementID<S>& id) {
    return { id.mValue, ids.at(id.getElementIndex()) };
  }

  static StableElementID getStableID(const StableIDRow& ids, const UnpackedDatabaseElementID& id) {
    return { id.mValue, ids.at(id.getElementIndex()) };
  }

  template<class DatabaseT>
  static size_t getUnstableElementIndex(const StableElementID& id) {
    using ElementT = typename DatabaseT::ElementID;
    return ElementT{ id.mUnstableIndex }.getElementIndex();
  }

  template<class DatabaseT>
  static constexpr size_t getElementMask() {
    using ET = typename DatabaseT::ElementID;
    return ET::ELEMENT_INDEX_MASK;
  }

  //This tries to use an id but will update it if it's out of date. The main point here is to make
  //element ids less error prone. This is also why the stable id row is read instead of only using the mapping
  //table, as this could also account for if the table was somehow modified without the mappings being updated
  //If performance is critical this could likely run with less validation in release, although resolving the mappings
  //should ideally be minor
  template<class... Tables>
  static std::optional<StableElementID> tryResolveStableID(const StableElementID& id, Database<Tables...>& db, const StableElementMappings& mappings) {
    if(details::isUnstableElementValid(db, id)) {
      return id;
    }

    //ID is wrong, update it from mappings
    auto it = mappings.findKey(id.mStableID);
    //If it isn't found that should mean the element has been removed, so return nothing
    if(!it.second) {
      return {};
    }

    const StableElementID result{ it.second->unstableIndex, it.first };
    //Now double check that the mapping wasn't wrong. Only really needed for debugging purposes
    //If it's wrong that means an unstable operation was done on a table that wasn't supposed to and the mappings
    //are now out of date
    assert(details::isUnstableElementValid(db, result));
    return result;
  }

  //For use if it's known the element can only be in the given table but may move around in it
  static std::optional<StableElementID> tryResolveStableIDWithinTable(const StableElementID& id, const StableIDRow& ids, const StableElementMappings& mappings, size_t elementMask) {
    if(details::isUnstableElementValid(id, ids, elementMask)) {
      return id;
    }
    //ID is wrong, update it from mappings
    auto it = mappings.findKey(id.mStableID);
    //If it isn't found that should mean the element has been removed, so return nothing
    if(!it.second) {
      return {};
    }

    const StableElementID result{ it.second->unstableIndex, it.first };
    //Now double check that the mapping wasn't wrong. Only really needed for debugging purposes
    //If it's wrong that means an unstable operation was done on a table that wasn't supposed to and the mappings
    //are now out of date
    assert(details::isUnstableElementValid(result, ids, elementMask));
    return result;
  }

  static std::optional<StableElementID> tryResolveStableID(const StableElementID& id, const StableElementMappings& mappings) {
    auto it = mappings.findKey(id.mStableID);
    return it.second ? std::make_optional(StableElementID{ it.second->unstableIndex, it.first }) : std::nullopt;
  }

  template<class DatabaseT>
  static std::optional<StableElementID> tryResolveStableIDWithinTable(const StableElementID& id, const StableIDRow& ids, const StableElementMappings& mappings) {
    return tryResolveStableIDWithinTable(id, ids, mappings, getElementMask<DatabaseT>());
  }

  static std::optional<StableElementID> tryResolveStableIDWithinTable(const StableElementID& id, const StableInfo& info) {
    return tryResolveStableIDWithinTable(id, *info.row, *info.mappings, info.description.getElementIndexMask());
  }

  template<size_t S>
  static void swapRemove(StableIDRow& row, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    swapRemove(row, UnpackedDatabaseElementID::fromPacked(id), mappings);
  }

  static void swapRemove(StableIDRow& row, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
    const size_t newSize = row.size() - 1;
    const size_t removeIndex = id.getElementIndex();
    size_t& stableIDToRemove = row.at(removeIndex);

    //Erase old mapping if valid. Case for invalid is in the reuse case for migrateOne below
    if(stableIDToRemove != dbDetails::INVALID_VALUE) {
      auto it = mappings.findKey(stableIDToRemove);
      assert(it.second);
      if(it.second) {
        //Assert mapping matched what it was pointing at
        assert((it.second->unstableIndex == id.remake(id.getTableIndex(), removeIndex).mValue));
        mappings.tryEraseKey(it.first);
      }
    }

    //Swap remove
    stableIDToRemove = row.at(newSize);
    row.resize(newSize);

    //Update mapping for swapped element
    if(removeIndex < newSize) {
      mappings.tryUpdateKey(stableIDToRemove, id.remake(id.getTableIndex(), removeIndex).mValue);
    }
  }

  static void swap(StableIDRow& row, const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b, StableElementMappings& mappings) {
    size_t& stableA = row.at(a.getElementIndex());
    size_t& stableB = row.at(b.getElementIndex());
    mappings.tryUpdateKey(stableA, b.mValue);
    mappings.tryUpdateKey(stableB, a.mValue);
    std::swap(stableA, stableB);
  }

  //ElementID is needed to get the table ID, index doesn't matter
  template<size_t S>
  static void emplaceBack(StableIDRow& row, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    const size_t newStableID = mappings.createKey();
    const size_t newUnstableIndex = row.size();
    row.emplaceBack(newStableID);
    mappings[newStableID] = DatabaseElementID<S>{ id.getTableIndex(), newUnstableIndex };
  }

  static void resize(StableIDRow& row, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys = nullptr) {
    size_t oldSize = row.size();
    //Remove mappings for elements about to be removed
    for(size_t i = newSize; i < oldSize; ++i) {
      [[maybe_unused]] const bool erased = mappings.tryEraseKey(row.at(i));
      assert(erased);
    }

    row.resize(newSize);
    for(size_t i = oldSize; i < newSize; ++i) {
      //Assign new id
      row.at(i) = reservedKeys ? reservedKeys[i - oldSize].mStableID : mappings.createRawKey();

      //Add mapping for new id
      mappings.insertKey(row.at(i), id.remake(id.getTableIndex(), i).mValue);
    }
  }

  static void migrateOne(StableIDRow& src, StableIDRow& dst, const UnpackedDatabaseElementID& fromID, const UnpackedDatabaseElementID& toID, StableElementMappings& mappings) {
    UnpackedDatabaseElementID dstID{ toID.remakeElement(dst.size()) };
    size_t& stableIDToMove = src.at(fromID.getElementIndex());

    //Copy stable id to destination table
    dst.emplaceBack(stableIDToMove);

    //Update old mapping to point at new table
    [[maybe_unused]] const bool updated = mappings.tryUpdateKey(stableIDToMove, dstID.mValue);
    assert(updated);
    //Invalidate
    stableIDToMove = UnpackedDatabaseElementID{}.mValue;

    //Swap remove element in old table
    swapRemove(src, fromID, mappings);
  }

  //Same here, toid element index doesn't matter, only table id
  template<size_t S>
  static void migrateOne(StableIDRow& src, StableIDRow& dst, const DatabaseElementID<S>& fromID, const DatabaseElementID<S>& toID, StableElementMappings& mappings) {
    migrateOne(src, dst, UnpackedDatabaseElementID::fromPacked(fromID), UnpackedDatabaseElementID::fromPacked(toID), mappings);
  }
};

//Wraps the operations needed to modify any table in a single type, with the table itself passed through via void*
struct StableTableModifier {
  template<class TableT>
  static StableTableModifier get() {
    struct Adapter {
      static void resize(void* table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys) {
        StableOperations::stableResizeTable(*static_cast<TableT*>(table), id, newSize, mappings, reservedKeys);
      }

      static size_t size(const void* table) {
        return TableOperations::size(*static_cast<const TableT*>(table));
      }

      static void swapRemove(void* table, const UnpackedDatabaseElementID& id, StableElementMappings& mappings) {
        StableOperations::stableSwapRemove(*static_cast<TableT*>(table), id, mappings);
      }

      static void insert(void* table, const UnpackedDatabaseElementID& location, size_t count, StableElementMappings& mappings) {
        StableOperations::stableInsertRangeAt(*static_cast<TableT*>(table), location, count, mappings);
      }
    };

    return {
      &Adapter::resize,
      &Adapter::size,
      &Adapter::swapRemove,
      &Adapter::insert
    };
  }

  void(*resize)(void* table, const UnpackedDatabaseElementID& id, size_t newSize, StableElementMappings& mappings, const StableElementID* reservedKeys){};
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
        TableOperations::insertRangeAt(*static_cast<TableT*>(table), location, count);
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
      UnpackedDatabaseElementID::fromPacked(DB::template getTableIndex<TableT>())
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
  size_t addElements(size_t count, const StableElementID* reservedIDs) {
    const size_t first = modifier.size(table);
    modifier.resize(table, tableID, count + first, *stableMappings, reservedIDs);
    return first;
  }

  void resize(size_t count, const StableElementID* reservedIDs) {
    modifier.resize(table, tableID, count, *stableMappings, reservedIDs);
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
  explicit operator bool() const { return row; }
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
    return TableOperations::getRowInTable<Row>(*static_cast<DB*>(db), typename DB::ElementID{ id.mValue });
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
