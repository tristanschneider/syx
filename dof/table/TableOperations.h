#pragma once
#include "Database.h"
#include "StableElementID.h"

struct TableOperations {
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

  template<class TableT, size_t S>
  static void stableResizeTable(TableT& table, const DatabaseElementID<S>& id, size_t newSize, StableElementMappings& mappings) {
    static_assert(isStableTable<TableT>);
    table.visitOne([&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::resize(row, id, newSize, mappings);
      }
      else {
        row.resize(newSize);
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

  template<class TableT>
  static void swapRemove(TableT& table, size_t index) {
    static_assert(!isStableTable<TableT>);
    const size_t myLast = size(table) - 1;
    table.visitOne([index, myLast](auto& row) {
      std::swap(row.at(index), row.at(myLast));
      row.resize(myLast);
    });
  }

  template<class TableT, size_t S>
  static void stableSwapRemove(TableT& table, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    static_assert(isStableTable<TableT>);
    const size_t myLast = size(table) - 1;
    table.visitOne([&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_same_v<RowT, StableIDRow>) {
        StableOperations::swapRemove(row, id, mappings);
      }
      else {
        std::swap(row.at(id.getElementIndex()), row.at(myLast));
        row.resize(myLast);
      }
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
        std::swap(row.at(index), row.at(myLast));
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