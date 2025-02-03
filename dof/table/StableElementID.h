#pragma once
#include "DatabaseID.h"
#include "Table.h"

#include <cassert>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include "generics/PagedVector.h"

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

//Scheduling ensures that anything adding to a table has exclusive access to a table
//These mappings apply across all tables
//This means anyone looking up a key wouldn't be looking up one in a table being modified
//Multiple tables could be modifying at the same time
//This means the thread safety is needed for the updates but not for the lookups since none of the lookups
//would have shared access to the mappings while modifications are happening to that table
//This works because the contents are reserved so that no growths will ever be in the way of the lookups
struct StableElementMappings {
public:
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

  void insertKey(size_t stable, StableElementMapping mapping) {
    WriteLockGuard guard{ mMutex };
    mStableToUnstable[stable].setIgnoreVersion(mapping);
  }

  //Mapping obtained from createKey
  void insertKey(const StableElementMappingPtr& key, StableElementMapping mapping) {
    assert(key);
    WriteLockGuard guard{ mMutex };
    key.mValue->setIgnoreVersion(mapping);
  }

  //This assumes the caller knows it's pointing at the correct version
  bool tryUpdateKey(size_t stable, StableElementMapping mapping) {
    WriteLockGuard guard{ mMutex };
    if(StableElementMapping* v = tryGet(stable)) {
      v->setIgnoreVersion(mapping);
      return true;
    }
    return false;
  }

  void updateKey(const StableElementMappingPtr& mapping, StableElementMapping m) {
    WriteLockGuard guard{ mMutex };
    mapping.mValue->setIgnoreVersion(m);
  }

  bool tryEraseKey(size_t stable) {
    WriteLockGuard guard{ mMutex };
    if(StableElementMapping* v = tryGet(stable)) {
      mFreeList.push_back(stable);
      v->invalidate();
      return true;
    }
    return false;
  }

  void eraseKey(const StableElementMappingPtr& mapping) {
    assert(mapping);
    WriteLockGuard guard{ mMutex };
    const size_t stable = mStableToUnstable.indexOf(*mapping.mValue);
    mFreeList.push_back(stable);
    mapping.mValue->invalidate();
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
    mStableToUnstable.push_back({});
    return result;
  }

  StableElementMapping* tryGet(size_t stable) {
    if(mStableToUnstable.size() > stable) {
      StableElementMapping& result = mStableToUnstable[stable];
      return result.isValid() ? &result : nullptr;
    }
    return nullptr;
  }

  const StableElementMapping* tryGet(size_t stable) const {
    if(mStableToUnstable.size() > stable) {
      const StableElementMapping& result = mStableToUnstable[stable];
      return result.isValid() ? &result : nullptr;
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
    , expectedVersion{ mapping ? mapping->getVersion() : StableElementVersion{} } {
  }

  explicit operator bool() const {
    return ref && ref->getVersion() == expectedVersion;
  }

  const StableElementMapping* tryGet() const {
    return operator bool() ? ref.get() : nullptr;
  }

  const StableElementMapping* uncheckedGet() const {
    return ref.get();
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

  StableElementVersion getExpectedVersion() const {
    return expectedVersion;
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
