#pragma once

#include <vector>

#include "DatabaseID.h"

template<class... Tables>
struct Database {
  using ElementID = DatabaseElementID<sizeof...(Tables)>;

  constexpr static DatabaseDescription getDescription() {
    return {
      ElementID::ELEMENT_INDEX_BITS
    };
  }

  template<class T, class Visitor, size_t... I>
  static constexpr void visitImpl(T& tuple, std::index_sequence<I...>, const Visitor& visitor) {
    (visitor(std::get<I>(tuple)), ...);
  }

  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor) {
    visitImpl(mTables, std::make_index_sequence<sizeof...(Tables)>(), visitor);
  }

  template<class Visitor>
  constexpr void visitOne(const Visitor& visitor) const {
    visitImpl(mTables, std::make_index_sequence<sizeof...(Tables)>(), visitor);
  }

  struct GetIndexImpl {
    template<class TestT, size_t CurrentIndex>
    static constexpr size_t _getTableIndex() {
      return CurrentIndex;
    }
    template<class TestT, size_t CurrentIndex, class CurrentTable, class... Rest>
    static constexpr size_t _getTableIndex() {
      if constexpr(std::is_same_v<TestT, CurrentTable>) {
        return CurrentIndex;
      }
      else {
        return _getTableIndex<TestT, CurrentIndex + 1, Rest...>();
      }
    }
  };

  template<class TableT>
  static constexpr ElementID getElementID(size_t index) {
    return ElementID(GetIndexImpl::template _getTableIndex<TableT, 0, Tables...>(), index);
  }

  template<class TableT>
  static constexpr ElementID getTableIndex() {
    return getElementID<TableT>(0);
  }

  template<class TableT>
  static constexpr ElementID getTableIndex(const TableT&) {
    return getElementID<TableT>(0);
  }

  static constexpr size_t size() {
    return sizeof...(Tables);
  }

  std::tuple<Tables...> mTables;
};