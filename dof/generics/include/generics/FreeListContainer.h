#pragma once

#include <iterator>

namespace gnx {
  //TODO: this is wonky, should probably be an intrusive linked list
  template<class T>
  struct DefaultFreeOps {
    //If this element has been marked as free
    static bool isFree(const T& value) {
      return value.isFree();
    }
    //Do something to the value so it will return isFree. It will not appear in iteration
    static void markAsFree(T& value, bool isFree) {
      value.markAsFree(isFree);
    }
  };
  template<auto MemberPtr, auto FreeValue>
  struct MemberFreeOps;
  template<class ValueT, class IndexT, IndexT(ValueT::*MemberPtr), IndexT FreeValue>
  struct MemberFreeOps<MemberPtr, FreeValue> {
    static bool isFree(const ValueT& value) {
      return value.*MemberPtr == FreeValue;
    }
    //Do something to the value so it will return isFree. It will not appear in iteration
    static void markAsFree(ValueT& value, bool isFree) {
      //TODO: get rid of + 1 hack and have two constants
      value.*MemberPtr = isFree ? FreeValue : static_cast<IndexT>(FreeValue + static_cast<IndexT>(1));
    }
  };

  struct NoVersion{};

  //Specialize to provide your own
  template<class T>
  struct FreeListTraits {
    using ValueT = T;
    using IndexT = size_t;
    using Ops = DefaultFreeOps<T>;
    using VersionT = NoVersion;
  };

  namespace details {
    template<class T>
    concept IsUnversionedStorage = std::same_as<typename T::VersionT, NoVersion>;
    template<class T>
    concept IsVersionedStorage = !IsUnversionedStorage<T>;

    //Storage to allow opting in to features without spending additional memory for unused features
    template<class T>
    struct VFLStorage {};

    template<class T> requires IsUnversionedStorage<T>
    struct VFLStorage<T> {
      auto tryGetVersions() const { return nullptr; }

      std::vector<typename T::ValueT> values;
      std::vector<typename T::IndexT> freeList;
    };

    template<class T> requires IsVersionedStorage<T>
    struct VFLStorage<T> {
      auto tryGetVersions() const { return &version; }
      auto tryGetVersions() { return &version; }

      std::vector<typename T::ValueT> values;
      std::vector<typename T::IndexT> freeList;
      std::vector<typename T::VersionT> version;
    };
  }

  template<class T>
  struct VectorFreeList {
    using Traits = FreeListTraits<T>;
    using Value = typename Traits::ValueT;
    using Index = typename Traits::IndexT;
    using Version = typename Traits::VersionT;
    using Ops = typename Traits::Ops;
    static constexpr bool IsVersioned = details::IsVersionedStorage<Traits>;

    struct Iterator {
      using iterator_category = std::forward_iterator_tag;
      using value_type        = Value;
      using difference_type   = size_t;
      using pointer           = Value*;
      using reference         = Value&;
      using iterator = Iterator;

      reference operator*() {
        return *begin;
      }

      pointer operator->() {
        return &operator*();
      }

      iterator& operator++() {
        ++begin;
        while(begin != end && Ops::isFree(*begin)) {
          ++begin;
        }
        return *this;
      }

      iterator operator++(int) {
        auto temp = *this;
        ++*this;
        return temp;
      }

      bool operator==(const iterator& _Right) const {
          return begin == _Right.begin;
      }

      bool operator!=(const iterator& _Right) const noexcept {
          return !(*this == _Right);
      }

      bool operator<(const iterator& _Right) const noexcept {
          return begin < _Right.begin;
      }

      bool operator>(const iterator& _Right) const noexcept {
          return _Right < *this;
      }

      bool operator<=(const iterator& _Right) const noexcept {
          return !(_Right < *this);
      }

      bool operator>=(const iterator& _Right) const noexcept {
        return !(*this < _Right);
      }
      //Both are needed if the free list is not intrusive because iteration needs to seek forward an
      //arbitrary amount without going off the end
      Value* begin{};
      Value* end{};
    };

    Index newIndex() requires details::IsUnversionedStorage<Traits> {
      return newIndexUnversioned();
    }

    std::pair<Index, Version> getHandle(Index i) const requires details::IsVersionedStorage<Traits> {
      return std::make_pair(i, getVersion(i));
    }

    T* tryGet(const std::pair<Index, Version>& handle) requires details::IsVersionedStorage<Traits> {
      if (getValues().size() > handle.first) {
        return getVersion(handle.first) == handle.second ? &getValues()[handle.first] : nullptr;
      }
      return nullptr;
    }

    const T* tryGet(const std::pair<Index, Version>& handle) const requires details::IsVersionedStorage<Traits> {
      if (getValues().size() > handle.first) {
        return getVersion(handle.first) == handle.second ? &getValues()[handle.first] : nullptr;
      }
      return nullptr;
    }

    std::pair<Index, Version> newIndex() requires details::IsVersionedStorage<Traits> {
      std::pair<Index, Version> result;
      result.first = newIndexUnversioned();
      result.second = getOrCreateVersion(result.first);
      return result;
    }

    //Delete, checking that the version matches
    //Deletion increments the version
    void deleteIndex(Index index, Version v) requires details::IsVersionedStorage<Traits> {
      Version& oldVersion = getOrCreateVersion(index);

      //If it's a bad version, exit, this must have already been deleted
      if(oldVersion != v) {
        return;
      }
      //Version is good, increment for next time
      ++oldVersion;

      Ops::markAsFree(getValues()[index], true);
      getFreeList().push_back(index);
    }

    void deleteIndex(Index index) requires details::IsUnversionedStorage<Traits> {
      Ops::markAsFree(getValues()[index], true);
      getFreeList().push_back(index);
    }

    bool isFree(Index index) const {
      return index < getValues().size() && Ops::isFree(getValues()[index]);
    }

    bool isValid(Index index) const {
      return index < getValues().size() && !Ops::isFree(getValues()[index]);
    }

    size_t activeSize() const {
      return getValues().size() - getFreeList().size();
    }

    size_t addressableSize() const {
      return getValues().size();
    }

    void clear() {
      //When clearing everything can be emptied rather than adding everything to the free list
      //Keep the versions though to avoid false positives on version checks
      getValues().clear();
      getFreeList().clear();
    }

    Iterator begin() {
      Iterator result{ getValues().data(), getValues().data() + getValues().size() };
      //Seek forward in case the first element is freed
      if(getValues().size() && Ops::isFree(*result)) {
        ++result;
      }
      return result;
    }

    Iterator end() {
      return { getValues().data() + getValues().size(), getValues().data() + getValues().size() };
    }

    //This is a raw index into the values container, meaning that if it is 5 iterating from begin
    //5 times might not land on this index if there are free elements inbetween
    Index rawIndex(const Iterator& it) {
      return static_cast<Index>(it.begin - getValues().data());
    }

    Value& operator[](Index i) {
      return getValues()[i];
    }

    const Value& operator[](Index i) const {
      return getValues()[i];
    }

    std::vector<Value>& getValues() { return storage.values; };
    const std::vector<Value>& getValues() const { return storage.values; };
    std::vector<Index>& getFreeList() { return storage.freeList; };
    const std::vector<Index>& getFreeList() const { return storage.freeList; };

    Version& getOrCreateVersion(Index i) requires details::IsVersionedStorage<Traits> {
      if(storage.version.size() <= i) {
        storage.version.resize(i + 1);
      }
      return storage.version[i];
    }

    Version getVersion(Index i) const requires details::IsVersionedStorage<Traits> {
      return storage.version.size() <= i ? static_cast<Version>(0) : storage.version[i];
    }

    Index newIndexUnversioned() {
      Index result{};
      if(getFreeList().empty()) {
        result = static_cast<Index>(getValues().size());
        getValues().emplace_back();
      }
      else {
        result = getFreeList().back();
        getFreeList().pop_back();
      }

      Ops::markAsFree(getValues()[result], false);

      return result;
    }

    details::VFLStorage<Traits> storage;
  };
};