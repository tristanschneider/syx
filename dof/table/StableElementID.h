#pragma once
#include "Database.h"
#include "Table.h"

#include <cassert>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include "generics/PagedVector.h"

struct StableIDRow : Row<size_t> {};

using StableElementVersion = uint16_t;
struct StableElementMapping {
  size_t unstableIndex{};
  StableElementVersion version{};
};

//Scheduling ensures that anythign adding to a table has exclusive access to a table
//These mappings apply across all tables
//This means anyone looking up a key wouldn't be looking up one in a table being modified
//Multiple tables could be modifying at the same time
//This means the thread safety is needed for the updates but not for the lookups since none of the lookups
//would have shared access to the mappings while modifications are happening to that table
//This works because the contents are reserved so that no growths will ever be in the way of the lookups
struct StableElementMappings {
public:
  static constexpr size_t INVALID = std::numeric_limits<size_t>::max();
  using WriteLockGuard = std::unique_lock<std::mutex>;
  using ReadLockGuard = std::unique_lock<std::mutex>;

  StableElementMappings() {
    //Ridiculous amount because resize would invalidate thread safety
    //It would be possible to allow growth with some complicated pointer management but this should be good enough
    mStableToUnstable.reserve(1000000);
  }

  size_t createKey() {
    WriteLockGuard guard{ mMutex };
    if(mFreeList.size()) {
      const size_t result = mFreeList.back();
      mFreeList.pop_back();
      return result;
    }
    const size_t result = mStableToUnstable.size();
    mStableToUnstable.push_back({ INVALID, 0 });
    return result;
  }

  void insertKey(size_t stable, size_t unstable) {
    WriteLockGuard guard{ mMutex };
    mStableToUnstable[stable].unstableIndex = unstable;
  }

  //This assumes the caller knows it's pointing at the correct version
  bool tryUpdateKey(size_t stable, size_t unstable) {
    WriteLockGuard gaurd{ mMutex };
    if(StableElementMapping* v = tryGet(stable)) {
      v->unstableIndex = unstable;
      return true;
    }
    return false;
  }

  bool tryEraseKey(size_t stable) {
    WriteLockGuard guard{ mMutex };
    if(StableElementMapping* v = tryGet(stable)) {
      mFreeList.push_back(stable);
      v->version++;
      v->unstableIndex = INVALID;
      return true;
    }
    return false;
  }

  std::pair<size_t, const StableElementMapping*> findKey(size_t stable) const {
    //This doesn't use a lock due to how common it is. The scheduler is responsible for ensuring this never happens in a table being modified
    if(const StableElementMapping* v = tryGet(stable)) {
      return std::make_pair(stable, v);
    }
    return std::make_pair<size_t, const StableElementMapping*>(0, nullptr);
  }

  size_t size() const {
    ReadLockGuard guard{ mMutex };
    return mStableToUnstable.size() - mFreeList.size();
  }

  bool empty() const {
    return size() == 0;
  }

private:
  StableElementMapping* tryGet(size_t stable) {
    if(mStableToUnstable.size() > stable) {
      StableElementMapping& result = mStableToUnstable[stable];
      return result.unstableIndex != INVALID ? &result : nullptr;
    }
    return nullptr;
  }

  const StableElementMapping* tryGet(size_t stable) const {
    if(mStableToUnstable.size() > stable) {
      const StableElementMapping& result = mStableToUnstable[stable];
      return result.unstableIndex != INVALID ? &result : nullptr;
    }
    return nullptr;
  }

  gnx::PagedVector<StableElementMapping> mStableToUnstable;
  std::vector<size_t> mFreeList;
  mutable std::mutex mMutex;
};

class ElementRef {
public:
  ElementRef() = default;
  ElementRef(const StableElementMapping* mapping)
    : ref{ mapping }
    , expectedVersion{ mapping ? mapping->version : StableElementVersion{} } {
  }

  explicit operator bool() const {
    return ref && ref->version == expectedVersion;
  }

  const size_t* tryGet() const {
    return operator bool() ? &ref->unstableIndex : nullptr;
  }

  bool operator==(const ElementRef& rhs) const {
    return ref == rhs.ref && expectedVersion == rhs.expectedVersion;
  }

  //To be used in contexts where stale versions wouldn't also be hashed
  size_t unversionedHash() const {
    return std::hash<const void*>()(ref);
  }

private:
  const StableElementMapping* ref{};
  StableElementVersion expectedVersion{};
};

namespace std {
  template<>
  struct hash<ElementRef> {
    size_t operator()(const ElementRef& ref) const {
      return ref.unversionedHash();
    }
  };
}

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
