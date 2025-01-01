#pragma once

#include <numeric>

struct dbDetails {
  static constexpr size_t constexprLog2(size_t input) {
    size_t result = 1;
    while(input /= 2) {
      ++result;
    }
    return result;
  }

  static constexpr size_t bitsToContain(size_t value) {
    return constexprLog2(value);
  }

  static constexpr size_t maskFirstBits(size_t numBits) {
    return (size_t(1) << numBits) - 1;
  }

  static constexpr size_t packTableAndElement(size_t tableIndex, size_t elementIndex, size_t elementBits) {
    return (tableIndex << elementBits) | elementIndex;
  }

  static size_t unpackElementIndex(size_t packed, size_t elementBits) {
    return (packed & maskFirstBits(elementBits));
  }

  constexpr static size_t unpackTableIndex(size_t packed, size_t elementBits) {
    return packed >> elementBits;
  }

  static constexpr size_t INVALID_VALUE = std::numeric_limits<size_t>::max();
};

//This is a complicated way to allow stuffing a database-wide identifier into a size_t
//The minimum bits needed to store table index are used, and the rest go to the index in the table
//TODO: way simpler would be a fixed number of bits and enforcing table limits. A pair of uint32_t would probably be fine...
struct DatabaseDescription {
  constexpr size_t getElementIndexBits() const {
    return elementIndexBits;
  }

  constexpr size_t getElementIndexMask() const {
    return dbDetails::maskFirstBits(getElementIndexBits());
  }

  constexpr size_t getTableIndexMask() const {
    return ~getElementIndexMask();
  }

  size_t elementIndexBits{};
};

//For when a template DatabaseElementID type is not desired
struct UnpackedDatabaseElementID {
  //Entire packed bits from an unstable index or DB::GetTableIndex
  static constexpr UnpackedDatabaseElementID fromDescription(size_t unstableIndex, const DatabaseDescription& desc) {
    return { unstableIndex, desc.elementIndexBits };
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
    return { dbDetails::packTableAndElement(tableIndex, elementIndex, mElementIndexBits), mElementIndexBits };
  }

  UnpackedDatabaseElementID remakeElement(size_t elementIndex) const {
    return remake(getTableIndex(), elementIndex);
  }

  bool operator==(const UnpackedDatabaseElementID& rhs) const {
    return mValue == rhs.mValue;
  }

  bool operator<(const UnpackedDatabaseElementID& rhs) const {
    return mValue < rhs.mValue;
  }

  size_t mValue = dbDetails::INVALID_VALUE;
  size_t mElementIndexBits{};
};

//Same as UnpackedDatabaseElementID mUnstableIndex of the table, intended for when referring to table vs element in table
struct TableID : UnpackedDatabaseElementID {
  TableID() = default;
  TableID(const UnpackedDatabaseElementID& i)
    : UnpackedDatabaseElementID{ i }
  {
    mValue &= getTableMask();
  }

  bool operator==(const TableID& rhs) const {
    return mValue == rhs.mValue;
  }

  bool operator<(const TableID& rhs) const {
    return mValue < rhs.mValue;
  }
};

namespace std {
  template<>
  struct hash<TableID> {
    size_t operator()(const TableID& i) const {
      return std::hash<size_t>{}(i.mValue);
    }
  };
}