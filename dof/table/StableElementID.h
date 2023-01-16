#pragma once
#include "Database.h"
#include "Queries.h"
#include "Table.h"

#include <optional>
#include <unordered_map>

//A stable id is used to reference an element in a table that might move
//If the id is still valid the unstable index can be used to look it up
//Otherwise, the stable id is used to ubdate the unstable index using the global mappings
struct StableElementID {
  bool operator==(const StableElementID& id) const {
    return mUnstableIndex == id.mUnstableIndex && mStableID == id.mStableID;
  }

  //ElementID of database, meaning a combination of the table index and element index
  size_t mUnstableIndex{};
  size_t mStableID{};
};

struct StableElementMappings {
  std::unordered_map<size_t, size_t> mStableToUnstable;
  size_t mKeygen{};
};

struct StableIDRow : Row<size_t> {};

struct StableOperations {
  struct details {
    template<class... Tables>
    static bool isUnstableElementValid(Database<Tables...>& db, const StableElementID& id) {
      using ElementIDT = typename Database<Tables...>::ElementID;
      const ElementIDT unstableElement{ id.mUnstableIndex };
      const size_t unstableIndex = unstableElement.getElementIndex();

      const StableIDRow* ids = Queries::getRowInTable<StableIDRow>(db, unstableElement);
      assert(ids && "Element should at least be pointing at a table that has a stable id row");
      if(!ids) {
        return false;
      }
      //Validate id, if it matches what's at the location it's fine as is
      return ids->size() > unstableIndex && ids->at(unstableIndex) == id.mStableID;
    }
  };

  template<size_t S>
  static StableElementID getStableID(const StableIDRow& ids, const DatabaseElementID<S>& id) {
    return { id.mValue, ids.at(id.getElementIndex()) };
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
    auto it = mappings.mStableToUnstable.find(id.mStableID);
    //If it isn't found that should mean the element has been removed, so return nothing
    if(it == mappings.mStableToUnstable.end()) {
      return {};
    }

    const StableElementID result{ it->second, it->first };
    //Now double check that the mapping wasn't wrong. Only really needed for debugging purposes
    //If it's wrong that means an unstable operation was done on a table that wasn't supposed to and the mappings
    //are now out of date
    assert(details::isUnstableElementValid(db, result));
    return result;
  }

  template<size_t S>
  static void swapRemove(StableIDRow& row, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    const size_t newSize = row.size() - 1;
    const size_t removeIndex = id.getElementIndex();
    size_t& stableIDToRemove = row.at(removeIndex);

    //Erase old mapping if valid. Case for invalid is in the reuse case for migrateOne below
    if(DatabaseElementID<S>{ stableIDToRemove }.isValid()) {
      auto it = mappings.mStableToUnstable.find(stableIDToRemove);
      assert(it != mappings.mStableToUnstable.end());
      if(it != mappings.mStableToUnstable.end()) {
        //Assert mapping matched what it was pointing at
        assert((it->second == DatabaseElementID<S>{ id.getTableIndex(), removeIndex}.mValue));
        mappings.mStableToUnstable.erase(it);
      }
    }

    //Swap remove
    stableIDToRemove = row.at(newSize);
    row.resize(newSize);

    //Update mapping for swapped element
    if(removeIndex < newSize) {
      mappings.mStableToUnstable[stableIDToRemove] = DatabaseElementID<S>{ id.getTableIndex(), removeIndex }.mValue;
    }
  }

  //ElementID is needed to get the table ID, index doesn't matter
  template<size_t S>
  static void emplaceBack(StableIDRow& row, const DatabaseElementID<S>& id, StableElementMappings& mappings) {
    const size_t newStableID = ++mappings.mKeygen;
    const size_t newUnstableIndex = row.size();
    row.emplaceBack(newStableID);
    mappings[newStableID] = DatabaseElementID<S>{ id.getTableIndex(), newUnstableIndex };
  }

  template<size_t S>
  static void resize(StableIDRow& row, const DatabaseElementID<S>& id, size_t newSize, StableElementMappings& mappings) {
    size_t oldSize = row.size();
    //Remove mappings for elements about to be removed
    for(size_t i = newSize; i < oldSize; ++i) {
      auto it = mappings.mStableToUnstable.find(row.at(i));
      assert(it != mappings.mStableToUnstable.end());
      if(it != mappings.mStableToUnstable.end()) {
        mappings.mStableToUnstable.erase(it);
      }
    }

    row.resize(newSize);
    for(size_t i = oldSize; i < newSize; ++i) {
      //Assign new id
      row.at(i) = ++mappings.mKeygen;
      //Add mapping for new id
      mappings.mStableToUnstable[row.at(i)] = DatabaseElementID<S>{ id.getTableIndex(), i }.mValue;
    }
  }

  //Same here, toid element index doesn't matter, only table id
  template<size_t S>
  static void migrateOne(StableIDRow& src, StableIDRow& dst, const DatabaseElementID<S>& fromID, const DatabaseElementID<S>& toID, StableElementMappings& mappings) {
    DatabaseElementID<S> dstID{ toID.getTableIndex(), dst.size() };
    size_t& stableIDToMove = src.at(fromID.getElementIndex());

    //Copy stable id to destination table
    dst.emplaceBack(stableIDToMove);

    //Update old mapping to point at new table
    assert(mappings.mStableToUnstable.find(stableIDToMove) != mappings.mStableToUnstable.end());
    mappings.mStableToUnstable[stableIDToMove] = dstID.mValue;
    //Invalidate
    stableIDToMove = DatabaseElementID<S>{}.mValue;

    //Swap remove element in old table
    swapRemove(src, fromID, mappings);
  }
};