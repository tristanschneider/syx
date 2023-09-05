#include "RuntimeDatabase.h"

QueryResult<> RuntimeDatabase::query() {
  QueryResult<> result;
  result.matchingTableIDs.resize(data.tables.size());
  for(size_t i = 0; i < data.tables.size(); ++i) {
    result.matchingTableIDs[i] = getTableID(i);
  }
  return result;
}

RuntimeTable* RuntimeDatabase::tryGet(const UnpackedDatabaseElementID& id) {
  const size_t i = id.getTableIndex();
  return i < data.tables.size() ? &data.tables[i] : nullptr;
}

UnpackedDatabaseElementID RuntimeDatabase::getTableID(size_t index) const {
  return UnpackedDatabaseElementID{ 0, data.elementIndexBits }.remake(index, 0);
}