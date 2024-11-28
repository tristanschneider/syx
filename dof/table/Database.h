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

  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) {
    (visitor(std::get<Tables>(mTables), args...), ...);
  }

  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) const {
    (visitor(std::get<Tables>(mTables), args...), ...);
  }

  //Call a multi-argument visitor with all rows
  template<class Visitor, class... Args>
  constexpr auto visitAll(const Visitor& visitor, Args&&... args) {
    return visitor(args..., std::get<Tables>(mTables)...);
  }

  template<class Visitor, class... Args>
  constexpr auto visitAll(const Visitor& visitor, Args&&... args) const {
    return visitor(args..., std::get<Tables>(mTables)...);
  }

  template<class Visitor, class T>
  struct Impl {};
  template<class Visitor, size_t... Indices>
  struct Impl<Visitor, std::index_sequence<Indices...>> {
    template<size_t I>
    static void _visitX(std::tuple<Tables...>& tuple, const Visitor& visitor) {
      visitor(std::get<I>(tuple));
    }

    template<size_t I>
    static void _constVisitX(const std::tuple<Tables...>& tuple, const Visitor& visitor) {
      visitor(std::get<I>(tuple));
    }

    using VisitFn = void(*)(std::tuple<Tables...>&, const Visitor&);
    static VisitFn getVisitFn(size_t index) {
      static std::array<VisitFn, sizeof...(Indices)> functions{ &_visitX<Indices>... };
      return index < functions.size() ? functions[index] : nullptr;
    }

    using ConstVisitFn = void(*)(const std::tuple<Tables...>&, const Visitor&);
    static ConstVisitFn getConstVisitFn(size_t index) {
      static std::array<ConstVisitFn, sizeof...(Indices)> functions{ &_constVisitX<Indices>... };
      return index < functions.size() ? functions[index] : nullptr;
    }
  };

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

  template<class Visitor>
  constexpr void visitOneByIndex(ElementID id, const Visitor& visitor) {
    if(auto fn = Impl<Visitor, std::index_sequence_for<Tables...>>::getVisitFn(id.getTableIndex())) {
      fn(mTables, visitor);
    }
  }

  template<class Visitor>
  constexpr void visitOneByIndex(ElementID id, const Visitor& visitor) const {
    if(auto fn = Impl<Visitor, std::index_sequence_for<Tables...>>::getConstVisitFn(id.getTableIndex())) {
      fn(mTables, visitor);
    }
  }

  static constexpr size_t size() {
    return sizeof...(Tables);
  }

  std::tuple<Tables...> mTables;
};