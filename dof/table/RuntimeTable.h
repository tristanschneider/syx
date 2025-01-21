#pragma once

#include "DBTypeID.h"
#include "DatabaseID.h"

class IRow;
struct StableElementMappings;
class ElementRef;

//All the runtime types hold pointers to rows and tables stored somewhere. This is a simple way to define their storage.
//Layout doesn't matter strongly as they will only be queried once during initialization after which all operations are done directly on row pointers
struct IRuntimeStorage {
  virtual ~IRuntimeStorage() = default;
};

struct RuntimeTableRowBuilder {
  struct Row {
    bool operator==(DBTypeID rhs) const { return type == rhs; }

    DBTypeID type;
    IRow* row{};
  };

  bool contains(DBTypeID type) const {
    return std::find(rows.begin(), rows.end(), type) != rows.end();
  }

  template<class T>
  bool contains() const {
    return contains(DBTypeID::get<std::decay_t<T>>());
  }

  DBTypeID tableType;
  std::vector<Row> rows;
};

struct RuntimeTableArgs {
  //Optional, for when this table has a StableIDRow
  StableElementMappings* mappings{};
  TableID tableID;
  RuntimeTableRowBuilder rows;
};

class RuntimeTable {
public:
  using IDT = DBTypeID;

  RuntimeTable(RuntimeTableArgs&& args);

  const TableID& getID() const {
    return tableID;
  }

  const DBTypeID& getType() const {
    return tableType;
  }

  template<class RowT>
  RowT* tryGet() {
    return static_cast<RowT*>(tryGet(IDT::get<std::decay_t<RowT>>()));
  }

  template<class RowT>
  const RowT* tryGet() const {
    return static_cast<const RowT*>(tryGet(IDT::get<std::decay_t<RowT>>()));
  }

  template<class RowT>
  RuntimeTable& tryGet(RowT*& out) {
    return out = tryGet<RowT>(), *this;
  }

  template<class RowT>
  const RuntimeTable& tryGet(const RowT*& out) const {
    return out = tryGet<const RowT>(), *this;
  }

  IRow* tryGet(DBTypeID id) {
    auto it = rows.find(id);
    return it != rows.end() ? it->second : nullptr;
  }

  const IRow* tryGet(DBTypeID id) const {
    auto it = rows.find(id);
    return it != rows.end() ? it->second : nullptr;
  }

  //Number of elements in the table. All rows have this many elements except for SharedRow
  size_t size() const;
  //Number of rows in the table
  size_t rowCount() const;

  //Migrates the element in `from` table at `i` to the `to` table at the index indicated by the return value
  static size_t migrate(size_t i, RuntimeTable& from, RuntimeTable& to, size_t count);

  void resize(size_t newSize, const ElementRef* reservedKeys = nullptr);
  size_t addElements(size_t count, const ElementRef* reservedKeys = nullptr);
  void swapRemove(size_t i);

private:
  //Optional, for when this table has a StableIDRow
  StableElementMappings* mappings{};
  TableID tableID;
  DBTypeID tableType;
  std::unordered_map<DBTypeID, IRow*> rows;
  size_t tableSize{};
};