#pragma once
#include "ContainerTraits.h"

#include <tuple>
#include <vector>
#include <limits>

namespace gnx {
  template<class K, class V, class Traits = DefaultContainerTraits<K, V>>
  class HashMap {
  public:
    using Pair = std::pair<K, V>;
    using BucketKey = size_t;
    static constexpr BucketKey EMPTY_BUCKET = std::numeric_limits<BucketKey>::max();
    static constexpr float MAX_LOAD_FACTOR = Traits::MAX_LOAD_FACTOR;

    template<class Ty>
    class Iterator {
    public:
      using Pointer = std::add_pointer_t<typename Ty::value_type>;
      using Reference = typename Ty::value_type;

      using iterator_category = std::bidirectional_iterator_tag;
      using value_type        = typename Ty::value_type;
      using difference_type   = size_t;
      using pointer           = Pointer;
      using reference         = Reference;

      Iterator(const Iterator&) = default;
      Iterator& operator=(const Iterator&) = default;

      Pointer operator->() const { return wrappedIt.operator->(); }
      Reference operator*() const { return *wrappedIt; }
      auto operator<=>(const Iterator&) const = default;

      Iterator& operator++() {
        return ++wrappedIt;
      }

      Iterator operator++(int) {
        return wrappedIt++;
      }

      Iterator& operator--() {
        return --wrappedIt;
      }

      Iterator operator--(int) {
        return wrappedIt--;
      }

    private:
      Ty wrappedIt;
    };
    using MIterator = typename std::vector<Pair>::iterator;
    using CIterator = typename std::vector<Pair>::const_iterator;

    HashMap() = default;
    HashMap(const HashMap& rhs) {
      *this = rhs;
    }
    HashMap(HashMap&& rhs) {
      swap(rhs);
    }

    HashMap& operator=(const HashMap& rhs) {
      keyMask = rhs.keyMask;
      buckets = rhs.buckets;
      values = rhs.values;
      return *this;
    }

    HashMap& operator=(HashMap&& rhs) {
      swap(rhs);
      return *this;
    }

    MIterator begin() {
      return values.begin();
    }

    CIterator cbegin() {
      return values.cbegin();
    }

    MIterator end() {
      return values.end();
    }

    CIterator cend() {
      return values.cend();
    }

    bool empty() const {
      return !size();
    }

    size_t size() const {
      return values.size();
    }

    void clear() {
      //TODO: sometimes probably faster to memset all values
      for(const Pair& p : values) {
        removeFromBucket(findBucket(p.first).first);
      }
      values.clear();
    }

    std::pair<MIterator, bool> insert(const Pair& v) {
      return insert(Pair{ v });
    }

    std::pair<MIterator, bool> insert(Pair&& v) {
      auto&& [pair, found] = getOrCreate(v.first);
      if(!found) {
        pair.second = std::move(v.second);
      }
      size_t index = &pair - values.data();
      return std::make_pair(MIterator{ values.begin() + index }, !found);
    }

    MIterator erase(MIterator it) {
      //Swap remove
      const K& key = it->first;
      if(values.size() > 1) {
        const BucketKey oldBucket = findBucket(key).first;
        const BucketKey swapBucket = findBucket(values.back().first).first;
        //Point the bucket to be swapped at the removed value index
        buckets[swapBucket] = buckets[oldBucket];
        removeFromBucket(oldBucket);
        //Swap the value itself into the slot that's being erased
        *it = values.back();
      }
      ++it;
      values.pop_back();
      return it;
    }

    V& operator[](const K& k) {
      return getOrCreate(k).first.second;
    }

    bool contains(const K& k) const {
      return tryFind(k) != nullptr;
    }

    MIterator find(const K& k) {
      if(const Pair* result = tryFind(k)) {
        const size_t index = result - values.data();
        return values.begin() + index;
      }
      return values.end();
    }

    CIterator find(const K& k) const {
      if(const Pair* result = tryFind(k)) {
        const size_t index = result - values.data();
        return { values.cbegin() + index };
      }
      return { values.cend() };
    }

    void swap(HashMap& rhs) {
      std::swap(keyMask, rhs.keyMask);
      std::swap(buckets, rhs.buckets);
      std::swap(values, rhs.values);
    }

    void reserve(size_t count) {
      values.reserve(count);
      if(count > keyMask) {
        grow(maskForSize(count));
      }
    }

    size_t bucket_count() const {
      return buckets.size();
    }

    float load_factor() const {
      return bucket_count() ? static_cast<float>(size())/static_cast<float>(bucket_count()) : std::numeric_limits<float>::max();
    }

  private:
    BucketKey toBucketKey(const K& k) const {
      return std::hash<K>{}(k) & keyMask;
    }

    //Set the index to empty then fill holes until an empty bucket is found
    void removeFromBucket(size_t bucket) {
      buckets[bucket] = EMPTY_BUCKET;
      size_t holeToFill = bucket;
      bucket = (bucket + 1) & keyMask;
      //For all keys offset by collisions, shift them into the hole created by the removal
      while(buckets[bucket] != EMPTY_BUCKET) {
        if(toBucketKey(values[buckets[bucket]].first) <= holeToFill) {
          buckets[holeToFill] = buckets[bucket];
          buckets[bucket] = EMPTY_BUCKET;
          holeToFill = bucket;
        }
        bucket = (bucket + 1) & keyMask;
      }
    }

    void grow(size_t toSize) {
      keyMask = toSize;
      //Set all current keys to invalid key
      std::memset(buckets.data(), 0xff, sizeof(BucketKey)*buckets.size());
      buckets.resize(keyMask + 1, EMPTY_BUCKET);
      //Rehash keys
      for(size_t i = 0; i < values.size(); ++i) {
        assignKey(values[i].first, i);
      }
    }

    void assignKey(const K& k, size_t valueIndex) {
      BucketKey bucket = toBucketKey(k);
      //Linear probing
      while(buckets[bucket] != EMPTY_BUCKET) {
        bucket = (bucket + 1) & keyMask;
      }
      buckets[bucket] = valueIndex;
    }

    std::pair<BucketKey, bool> findBucket(const K& k) const {
      BucketKey bucket = toBucketKey(k);
      if(!keyMask) {
        return std::make_pair(bucket, false);
      }
      //Linear probing
      while(buckets[bucket & keyMask] != EMPTY_BUCKET) {
        if(values[buckets[bucket]].first == k) {
          return std::make_pair(bucket, true);
        }
        bucket = (bucket + 1) & keyMask;
      }
      return std::make_pair(bucket, false);
    }

    const Pair* tryFind(const K& k) const {
      const auto [bucket, found] = findBucket(k);
      return found ? &values[buckets[bucket]] : nullptr;
    }

    std::pair<Pair&, bool> getOrCreate(const K& k) {
      const auto [bucket, found] = findBucket(k);
      if(found) {
        return { values[buckets[bucket]], true };
      }
      if(load_factor() > MAX_LOAD_FACTOR) {
        grow(maskForSize(keyMask << 1));
        return getOrCreate(k);
      }
      buckets[bucket] = values.size();
      values.emplace_back();
      Pair& result = values.back();
      result.first = k;
      return { result, false };
    }

    static constexpr size_t maskForSize(size_t size) {
      //Start with this minimum size
      size_t result = 0xFF;
      //Keep the size a multiple of 2 so it can always be used as a mask instead of modulo
      while(result < size) {
        result = (result << 1) | 1;
      }
      return result;
    }

    size_t keyMask{};
    std::vector<BucketKey> buckets;
    std::vector<Pair> values;
  };
}