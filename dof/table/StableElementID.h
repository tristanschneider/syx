#pragma once
#include "Database.h"
#include "Table.h"

#include <cassert>
#include <optional>
#include <unordered_map>
#include <shared_mutex>

struct StableIDRow : Row<size_t> {};

//This is clunky, it shouldn't need thread safety since the use cases are unrelated, but they all share the same map
//This could likely be improved with more individual local stable mappings, but that would also make their use more confusing
struct StableElementMappings {
public:
  static constexpr size_t INVALID = std::numeric_limits<size_t>::max();
  using WriteLockGuard = std::unique_lock<std::shared_mutex>;
  using ReadLockGuard = std::shared_lock<std::shared_mutex>;

  size_t createKey() {
    WriteLockGuard guard{ mMutex };
    if(mFreeList.size()) {
      const size_t result = mFreeList.back();
      mFreeList.pop_back();
      return result;
    }
    const size_t result = mStableToUnstable.size();
    mStableToUnstable.push_back(INVALID);
    return result;
  }

  void insertKey(size_t stable, size_t unstable) {
    WriteLockGuard guard{ mMutex };
    mStableToUnstable[stable] = unstable;
  }

  bool tryUpdateKey(size_t stable, size_t unstable) {
    WriteLockGuard gaurd{ mMutex };
    if(size_t* value = mStableToUnstable.size() > stable ? &mStableToUnstable[stable] : nullptr; value && *value != INVALID) {
      *value = unstable;
      return true;
    }
    return false;
  }

  bool tryEraseKey(size_t stable) {
    WriteLockGuard guard{ mMutex };
    if(size_t* value = mStableToUnstable.size() > stable ? &mStableToUnstable[stable] : nullptr; value && *value != INVALID) {
      mFreeList.push_back(stable);
      *value = INVALID;
      return true;
    }
    return false;
  }

  std::optional<std::pair<size_t, size_t>> findKey(size_t stable) const {
    ReadLockGuard guard{ mMutex };
    if(const size_t* value = mStableToUnstable.size() > stable ? &mStableToUnstable[stable] : nullptr; value && *value != INVALID) {
      return std::make_pair(stable, *value);
    }
    return {};
  }

  size_t size() const {
    ReadLockGuard guard{ mMutex };
    return mStableToUnstable.size() - mFreeList.size();
  }

  bool empty() const {
    return size() == 0;
  }

private:
  std::vector<size_t> mStableToUnstable;
  std::vector<size_t> mFreeList;
  mutable std::shared_mutex mMutex;
};

struct StableInfo {
  StableIDRow* row{};
  StableElementMappings* mappings{};
  DatabaseDescription description{};
};

struct ConstStableInfo {
  const StableIDRow* row{};
  const StableElementMappings* mappings{};
  const DatabaseDescription description{};
};

//A stable id is used to reference an element in a table that might move
//If the id is still valid the unstable index can be used to look it up
//Otherwise, the stable id is used to ubdate the unstable index using the global mappings
struct StableElementID {
  bool operator==(const StableElementID& id) const {
    return mUnstableIndex == id.mUnstableIndex && mStableID == id.mStableID;
  }

  bool operator!=(const StableElementID& id) const {
    return !(*this == id);
  }

  static constexpr StableElementID invalid() {
    return { std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max() };
  }

  static StableElementID fromStableID(size_t stableId) {
    //Table index is not known here, so a resolve will be needed to compute it
    return { dbDetails::INVALID_VALUE, stableId };
  }

  //For if the caller wants to get the stable element at the index that they know is correct
  static StableElementID fromStableRow(size_t index, const StableIDRow& row) {
    return fromStableID(row.at(index));
  }

  UnpackedDatabaseElementID toUnpacked(const DatabaseDescription& description) const {
    return { mUnstableIndex, description.elementIndexBits };
  }

  template<class DB>
  UnpackedDatabaseElementID toUnpacked() const {
    return toUnpacked(DB::getDescription());
  }

  template<class DB>
  typename DB::ElementID toPacked() const {
    return typename DB::ElementID{ mUnstableIndex };
  }

  //ElementID of database, meaning a combination of the table index and element index
  size_t mUnstableIndex{};
  size_t mStableID{};
};

//For convenience in std::find
struct StableElementFind {
  bool operator()(const StableElementID& i) const {
    return id.mStableID == i.mStableID;
  }
  const StableElementID& id;
};

namespace std {
  template<>
  struct hash<StableElementID> {
    size_t operator()(const StableElementID& id) const {
      return std::hash<size_t>{}(id.mStableID);
    }
  };
}

struct DBEvents {
  //Creating an element is from an invalid source to a valid destination
  //Destroying an element is from a valid source to an invalid destination
  //Moving an element is from a valid source to a valid destination
  //In the case of moving only the unstable index is used to determine the destination table,
  //the stable part doesn't matter
  struct MoveCommand {
    bool isDestroy() const { return destination == StableElementID::invalid(); }
    bool isCreate() const { return source == StableElementID::invalid(); };
    bool isMove() const { return source != StableElementID::invalid() && destination.mStableID == dbDetails::INVALID_VALUE; }

    StableElementID source;
    StableElementID destination;
  };
  std::vector<MoveCommand> toBeMovedElements;
};
