#pragma once

struct dbDetails {
  static constexpr size_t constexprLog2(size_t input) {
    size_t result = 1;
    while(input /= 2) {
      ++result;
    }
    return result;
  }

  static constexpr size_t maskFirstBits(size_t numBits) {
    return (size_t(1) << numBits) - 1;
  }

  static constexpr size_t packTableAndElement(size_t tableIndex, size_t elementIndex, size_t elementBits) {
    return (tableIndex << elementBits) | (elementIndex);
  }

  static size_t unpackElementIndex(size_t packed, size_t elementBits) {
    return (packed & maskFirstBits(elementBits));
  }

  static size_t unpackTableIndex(size_t packed, size_t elementBits) {
    return packed >> elementBits;
  }

  static constexpr size_t INVALID_VALUE = std::numeric_limits<size_t>::max();
};

//Like a poor-man's Entity id as it represents a particular entry somewhere in the database but care needs to be taken
//not to move the desired element as that would cause the id to no longer point at it
template<size_t TableCount>
struct DatabaseElementID {
  static constexpr size_t TABLE_INDEX_BITS = dbDetails::constexprLog2(TableCount);
  static constexpr size_t ELEMENT_INDEX_BITS = sizeof(size_t)*8 - TABLE_INDEX_BITS;
  static constexpr size_t ELEMENT_INDEX_MASK = dbDetails::maskFirstBits(ELEMENT_INDEX_BITS);
  static constexpr size_t TABLE_INDEX_MASK = ~ELEMENT_INDEX_MASK;

  constexpr DatabaseElementID() = default;
  explicit constexpr DatabaseElementID(size_t rawid)
    : mValue(rawid) {
  }
  constexpr DatabaseElementID(size_t tableIndex, size_t elementIndex)
    : mValue(dbDetails::packTableAndElement(tableIndex, elementIndex, ELEMENT_INDEX_BITS)) {
  }
  DatabaseElementID(const DatabaseElementID&) = default;
  DatabaseElementID& operator=(const DatabaseElementID&) = default;

  constexpr bool operator==(const DatabaseElementID& id) const { return mValue == id.mValue; }
  constexpr bool operator!=(const DatabaseElementID& id) const { return mValue != id.mValue; }

  size_t getTableIndex() const {
    return dbDetails::unpackTableIndex(mValue, ELEMENT_INDEX_BITS);
  }

  size_t getShiftedTableIndex() const {
    return mValue & TABLE_INDEX_MASK;
  }

  size_t getElementIndex() const {
    return dbDetails::unpackElementIndex(mValue, ELEMENT_INDEX_BITS);
  }

  bool isValid() const {
    return mValue != dbDetails::INVALID_VALUE;
  }

  size_t mValue = dbDetails::INVALID_VALUE;
};

//For when a template DatabaseElementID type is not desired
struct UnpackedDatabaseElementID {
  template<size_t S>
  static constexpr UnpackedDatabaseElementID fromPacked(const DatabaseElementID<S>& id) {
    return { id.mValue, DatabaseElementID<S>::ELEMENT_INDEX_BITS };
  }

  //TableIndex comes from DB::getTableIndex<X>() which is already shifted
  static constexpr UnpackedDatabaseElementID fromElementMask(size_t elementMask, size_t shiftedTableIndex, size_t elementIndex) {
    size_t bits = 0;
    while(elementMask) {
      elementMask = (elementMask >> 1);
      ++bits;
    }
    return { shiftedTableIndex | elementIndex, bits };
  }

  size_t getTableIndex() const {
    return dbDetails::unpackTableIndex(mValue, mElementIndexBits);
  }

  size_t getElementIndex() const {
    return dbDetails::unpackElementIndex(mValue, mElementIndexBits);
  }

  size_t getElementMask() const {
    return dbDetails::maskFirstBits(mElementIndexBits);
  }

  size_t getTableMask() const {
    return ~getElementMask();
  }

  UnpackedDatabaseElementID remake(size_t tableIndex, size_t elementIndex) const {
    return { dbDetails::packTableAndElement(tableIndex, elementIndex, mElementIndexBits) };
  }

  size_t mValue = dbDetails::INVALID_VALUE;
  size_t mElementIndexBits;
};

template<class... Tables>
struct Database {
  using ElementID = DatabaseElementID<sizeof...(Tables)>;

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
      return functions[index];
    }

    using ConstVisitFn = void(*)(const std::tuple<Tables...>&, const Visitor&);
    static ConstVisitFn getConstVisitFn(size_t index) {
      static std::array<ConstVisitFn, sizeof...(Indices)> functions{ &_constVisitX<Indices>... };
      return functions[index];
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
    return ElementID(GetIndexImpl::_getTableIndex<TableT, 0, Tables...>(), index);
  }

  template<class TableT>
  static constexpr ElementID getTableIndex() {
    return getElementID<TableT>(0);
  }

  template<class Visitor>
  constexpr void visitOneByIndex(ElementID id, const Visitor& visitor) {
    Impl<Visitor, std::index_sequence_for<Tables...>>::getVisitFn(id.getTableIndex())(mTables, visitor);
  }

  template<class Visitor>
  constexpr void visitOneByIndex(ElementID id, const Visitor& visitor) const {
    Impl<Visitor, std::index_sequence_for<Tables...>>::getConstVisitFn(id.getTableIndex())(mTables, visitor);
  }

  std::tuple<Tables...> mTables;
};