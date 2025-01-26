#pragma once

#include <numeric>

using StableElementVersion = uint8_t;
using ElementIndex = uint32_t;
using TableIndex = uint16_t;
using DatabaseIndex = uint8_t;

//This represents the makeup of identifiers used to access elements between tables and databases
//The packing here determines the size limits
//It was previously configurable which makes all access more complicated than necessary
//This is also what UnpackedDatabaseElementID is for, as it could as well use StableElementMapping directly now
//For tables with StableIDRow, each element has a slot in StableElementMappings pointing to a StableElementMapping
//These are referred to with versioned ElementRefs, which are unpacked into UnpackedDatabaseID to use as indices into tables and rows
struct StableElementMapping {
public:
  static constexpr ElementIndex INVALID = std::numeric_limits<ElementIndex>::max();

  ElementIndex getElementIndex() const {
    return elementIndex;
  }

  StableElementVersion getVersion() const {
    return version;
  }

  TableIndex getTableIndex() const {
    return tableIndex;
  }

  DatabaseIndex getDatabaseIndex() const {
    return dbIndex;
  }

  bool isValid() const {
    return elementIndex != INVALID;
  }

  void setElementIndex(ElementIndex e) {
    elementIndex = e;
  }

  void setTableIndex(TableIndex i) {
    tableIndex = i;
  }

  void setDatabaseIndex(DatabaseIndex i) {
    dbIndex = i;
  }

  void invalidate() {
    elementIndex = INVALID;
    ++version;
  }

  void setIgnoreVersion(const StableElementMapping& rhs) {
    setElementIndex(rhs.getElementIndex());
    setTableIndex(rhs.getTableIndex());
    setDatabaseIndex(rhs.getDatabaseIndex());
  }

  auto operator<=>(const StableElementMapping&) const = default;

  size_t hash() const {
    static_assert(sizeof(*this) == sizeof(uint64_t));
    return std::hash<uint64_t>()(reinterpret_cast<const uint64_t&>(*this));
  }

private:
  ElementIndex elementIndex{ INVALID };
  TableIndex tableIndex{};
  DatabaseIndex dbIndex{};
  StableElementVersion version{};
};

struct DatabaseDescription {
  DatabaseIndex dbIndex{};
};

struct UnpackedDatabaseElementID : StableElementMapping {
  UnpackedDatabaseElementID remake(size_t tableIdx, size_t elementIdx) const {
    UnpackedDatabaseElementID result{ *this };
    result.setTableIndex(tableIdx);
    result.setElementIndex(elementIdx);
    return result;
  }

  UnpackedDatabaseElementID remakeElement(size_t elementIdx) const {
    return remake(getTableIndex(), elementIdx);
  }

  auto operator<=>(const UnpackedDatabaseElementID&) const = default;
};

//Same as UnpackedDatabaseElementID mUnstableIndex of the table, intended for when referring to table vs element in table
struct TableID : UnpackedDatabaseElementID {
  TableID() {
    setElementIndex(0);
  }
  TableID(const UnpackedDatabaseElementID& i)
  {
    //Version is a concept only relevant to ids for elements in tables
    setIgnoreVersion(i);
    setElementIndex(0);
  }

  auto operator<=>(const TableID&) const = default;
};

namespace std {
  template<>
  struct hash<StableElementMapping> {
    size_t operator()(const TableID& i) const {
      return std::hash<size_t>{}(i.hash());
    }
  };

  template<>
  struct hash<TableID> {
    size_t operator()(const TableID& i) const {
      return i.hash();
    }
  };
}
