#pragma once
#include "Database.h"
#include "Table.h"

#include <cassert>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include "generics/PagedVector.h"
#include <variant>

using StableElementVersion = uint16_t;
struct StableElementMapping {
  size_t unstableIndex{};
  StableElementVersion version{};
};

struct StableElementMappings;

//Wrapper to only allow mutable access within StableElementMappings where thread safety can be assured while still exposing the values themselves
struct StableElementMappingPtr {
public:
  friend struct StableElementMappings;

  StableElementMappingPtr(StableElementMapping* m = nullptr)
    : mValue{ m } {}

  explicit operator bool() const {
    return mValue != nullptr;
  }

  const StableElementMapping* get() const {
    return mValue;
  }

  const StableElementMapping* operator->() const {
    return mValue;
  }

  const StableElementMapping& operator*() const {
    return *mValue;
  }

  auto operator<=>(const StableElementMappingPtr&) const = default;

private:
  StableElementMapping* mValue{};
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

  size_t createRawKey() {
    WriteLockGuard guard{ mMutex };
    return createRawKey(guard);
  }

  StableElementMappingPtr createKey() {
    WriteLockGuard guard{ mMutex };
    size_t result = createRawKey(guard);
    return &mStableToUnstable[result];
  }

  void insertKey(size_t stable, size_t unstable) {
    WriteLockGuard guard{ mMutex };
    mStableToUnstable[stable].unstableIndex = unstable;
  }

  //Mapping obtained from createKey
  void insertKey(const StableElementMappingPtr& key, size_t unstable) {
    assert(key);
    WriteLockGuard guard{ mMutex };
    key.mValue->unstableIndex = unstable;
  }

  //This assumes the caller knows it's pointing at the correct version
  bool tryUpdateKey(size_t stable, size_t unstable) {
    WriteLockGuard guard{ mMutex };
    if(StableElementMapping* v = tryGet(stable)) {
      v->unstableIndex = unstable;
      return true;
    }
    return false;
  }

  void updateKey(const StableElementMappingPtr& mapping, size_t unstable) {
    WriteLockGuard guard{ mMutex };
    mapping.mValue->unstableIndex = unstable;
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

  void eraseKey(const StableElementMappingPtr& mapping) {
    assert(mapping);
    WriteLockGuard guard{ mMutex };
    const size_t stable = mStableToUnstable.indexOf(*mapping.mValue);
    mFreeList.push_back(stable);
    mapping.mValue->version++;
    mapping.mValue->unstableIndex = INVALID;
  }

  size_t getStableID(const StableElementMapping& key) const {
    ReadLockGuard guard{ mMutex };
    return mStableToUnstable.indexOf(key);
  }

  std::pair<size_t, StableElementMappingPtr> findKey(size_t stable) {
    //This doesn't use a lock due to how common it is. The scheduler is responsible for ensuring this never happens in a table being modified
    if(StableElementMapping* v = tryGet(stable)) {
      return std::make_pair(stable, StableElementMappingPtr{ v });
    }
    return std::make_pair<size_t, StableElementMappingPtr>(0, nullptr);
  }

  size_t size() const {
    ReadLockGuard guard{ mMutex };
    return mStableToUnstable.size() - mFreeList.size();
  }

  bool empty() const {
    return size() == 0;
  }

private:
  size_t createRawKey(const WriteLockGuard&) {
    if(mFreeList.size()) {
      const size_t result = mFreeList.back();
      mFreeList.pop_back();
      return result;
    }
    const size_t result = mStableToUnstable.size();
    mStableToUnstable.push_back({ INVALID, 0 });
    return result;
  }

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
  ElementRef(StableElementMappingPtr mapping)
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
    return std::hash<const void*>()(ref.get());
  }

  const StableElementMappingPtr& getMapping() const {
    return ref;
  }

  bool operator<(const ElementRef& rhs) const {
    return ref.get() < rhs.ref.get();
  }

  bool operator>(const ElementRef& rhs) const {
    return ref.get() > rhs.ref.get();
  }

  bool isSet() const {
    return static_cast<bool>(getMapping());
  }

  //For checking validity when empty refs or valid refs are okay, but expired refs are not
  bool isUnsetOrValid() const {
    return !isSet() || tryGet();
  }

private:
  StableElementMappingPtr ref;
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

struct StableIDRow : Row<ElementRef> {};

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

//Same as UnpackedDatabaseElementID mUnstableIndex of the table, intended for when referring to table vs element in table
struct TableID : UnpackedDatabaseElementID {
};

struct DBEvents {
  using Variant = std::variant<std::monostate, TableID, ElementRef>;

  //Creating an element is from an invalid source to a valid destination
  //Destroying an element is from a valid source to an invalid destination
  //Moving an element is from a valid source to a valid destination
  //In the case of moving only the unstable index is used to determine the destination table,
  //the stable part doesn't matter
  struct MoveCommand {
    bool isDestroy() const { return std::holds_alternative<std::monostate>(destination); }
    bool isCreate() const { return std::holds_alternative<std::monostate>(source); };
    bool isMove() const { return std::holds_alternative<ElementRef>(source) && std::holds_alternative<TableID>(destination); }

    Variant source;
    Variant destination;
  };
  std::vector<MoveCommand> toBeMovedElements;
};
