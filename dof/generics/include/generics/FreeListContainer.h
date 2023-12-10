#pragma once

#include <iterator>

namespace gnx {
  template<class T>
  struct DefaultFreeOps {
    //If this element has been marked as free
    static bool isFree(const T& value) {
      return value.isFree();
    }
    //Do something to the value so it will return isFree. It will not appear in iteration
    static void markAsFree(T& value) {
      value.markAsFree();
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
    static void markAsFree(ValueT& value) {
      value.*MemberPtr = FreeValue;
    }
  };

  //Specialize to provide your own
  template<class T>
  struct FreeListTraits {
    using ValueT = T;
    using IndexT = size_t;
    using Ops = DefaultFreeOps<T>;
  };

  template<class T>
  struct VectorFreeList {
    using Traits = FreeListTraits<T>;
    using Value = typename Traits::ValueT;
    using Index = typename Traits::IndexT;
    using Ops = typename Traits::Ops;

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

    Index newIndex() {
      if(freeList.empty()) {
        const Index newIndex = static_cast<Index>(values.size());
        values.emplace_back();
        return newIndex;
      }
      Index freeIndex = freeList.back();
      freeList.pop_back();
      return freeIndex;
    }

    void deleteIndex(Index index) {
      Ops::markAsFree(values[index]);
      freeList.push_back(index);
    }

    void clear() {
      //When clearing everything can be emptied rather than adding everything to the free list
      values.clear();
      freeList.clear();
    }

    Iterator begin() {
      Iterator result{ values.data(), values.data() + values.size() };
      //Seek forward in case the first element is freed
      if(values.size() && Ops::isFree(*result)) {
        ++result;
      }
      return result;
    }

    Iterator end() {
      return { values.data() + values.size(), values.data() + values.size() };
    }

    //This is a raw index into the values container, meaning that if it is 5 iterating from begin
    //5 times might not land on this index if there are free elements inbetween
    Index rawIndex(const Iterator& it) {
      return static_cast<Index>(it.begin - values.data());
    }

    Value& operator[](Index i) {
      return values[i];
    }

    const Value& operator[](Index i) const {
      return values[i];
    }

    std::vector<Value> values;
    std::vector<Index> freeList;
  };
};