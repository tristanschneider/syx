#pragma once

struct TableOperations {
  template<class TableT>
  static auto getElement(TableT& table, size_t index) {
    return table.visitAll([&index](auto&... rows) -> TableT::ElementRef {
      return make_duple(&rows.at(index)...);
    });
  }

  template<class TableT>
  static auto addToTable(TableT& table) {
    return table.visitAll([](auto&... rows) -> TableT::ElementRef {
       return make_duple(&rows.emplaceBack()...);
    });
  }

  //Sorted and unique
  template<class LeadingRow, class TableT, class T>
  static auto addToSortedTable(TableT& table, T value) {
    auto& row = std::get<LeadingRow>(table.mRows);
    //Find the sorted insert position by the leading row
    auto it = std::lower_bound(row.begin(), row.end(), value);
    //Get the relative offset of that sorted position to use for all other rows
    const auto insertIndex = std::distance(row.begin(), it);

    //If this isn't new, return the existing element, as this is supposed to be a unique list
    if(it != row.end() && *it == value) {
      return getElement(table, insertIndex);
    }

    //Sorted insert using the found index, with special case to move in value for leading storage
    auto insertOne = [&](auto& row) {
      using RowT = std::decay_t<decltype(row)>;
      if constexpr(std::is_same_v<LeadingRow, RowT>) {
        return &row.insert(row.begin() + insertIndex, std::move(value));
      }
      else {
        //Insert a default constructed element
        return &row.insert(row.begin() + insertIndex, typename RowT::ElementT{});
      }
    };
    return table.visitAll([&](auto&... rows) -> TableT::ElementRef {
      return make_duple(insertOne(rows)...);
    });
  }

  template<class TableT>
  static void resizeTable(TableT& table, size_t newSize) {
    table.visitOne([newSize](auto& row) {
      row.resize(newSize);
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
    const size_t myLast = size(table) - 1;
    table.visitOne([index, myLast](auto& row) {
      std::swap(row.at(index), row.at(myLast));
      row.resize(myLast);
    });
  }

  template<class TableT>
  static void sortedRemove(TableT& table, size_t index) {
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
    //Silly implementation, will optimize to row by row if used enough
    while(size(src)) {
      migrateOne(src, dst, 0);
    }
  }
};