#pragma once
#include <memory>

#include "RuntimeTraits.h"

namespace ecx {
  //Vector where each object is a block of a consistent size
  template<class Allocator = std::allocator<uint8_t>>
  class BlockVector {
  public:
    using ITraits = IRuntimeTraits;

    class Iterator {
    public:
      using value_type = void*;
      using difference_type = std::ptrdiff_t;
      using pointer = value_type*;
      using reference = value_type&;
      using iterator_category = std::forward_iterator_tag;

      Iterator(uint8_t* data, size_t elementSize)
        : mData(data)
        , mElementSize(elementSize) {
      }

      Iterator(const Iterator&) = default;
      Iterator& operator=(const Iterator&) = default;

      Iterator& operator++() {
        mData += mElementSize;
        return *this;
      }

      Iterator& operator++(int) {
        Iterator result = *this;
        ++(*this);
        return result;
      }

      bool operator==(const Iterator& rhs) const {
        return mData == rhs.mData;
      }

      bool operator!=(const Iterator& rhs) const {
        return !(*this == rhs);
      }

      value_type operator*() {
        return mData;
      }

    private:
      size_t mElementSize = 0;
      uint8_t* mData = nullptr;
    };

    BlockVector(const ITraits& traits)
      : mTraits(&traits) {
    }

    BlockVector(const BlockVector& rhs) {
      *this = rhs;
    }

    BlockVector(BlockVector&& rhs) {
      swap(rhs);
    }

    ~BlockVector() {
      if(mTraits) {
        for(void* v : *this) {
          mTraits->destruct(v);
        }
      }
      if(mBegin) {
        _getAllocator().deallocate(mBegin, mCapacity - mBegin);
      }
    }

    BlockVector& operator=(const BlockVector& rhs) {
      assert(mTraits == rhs.mTraits);
      clear();

      const size_t newSize = rhs.size();
      if(newSize > capacity()) {
        _grow(newSize * mTraits->size());
      }

      mEnd = mBegin + _elementSize()*newSize;
      for(size_t i = 0; i < newSize; ++i) {
        mTraits->copyConstruct(rhs.at(i), at(i));
      }

      return  *this;
    }

    BlockVector& operator=(BlockVector&& rhs) {
      swap(rhs);
      return *this;
    }

    void* at(size_t index) {
      return mBegin + mTraits->size()*index;
    }

    const void* at(size_t index) const {
      return mBegin + mTraits->size()*index;
    }

    void* operator[](size_t index) {
      return at(index);
    }

    const void* operator[](size_t index) const {
      return at(index);
    }

    void* front() {
      return mBegin;
    }

    void* back() {
      return mEnd - mTraits->size();
    }

    void* data() {
      return front();
    }

    Iterator begin() {
      return { mBegin, mTraits->size() };
    }

    Iterator end() {
      return { mEnd, mTraits->size() };
    }

    bool empty() const {
      return mBegin == mEnd;
    }

    size_t size() const {
      return (mEnd - mBegin) / mTraits->size();
    }

    void reserve(size_t sz) {
      if(sz > capacity()) {
        _grow(sz * mTraits->size());
      }
    }

    size_t capacity() const {
      return (mCapacity - mBegin) / mTraits->size();
    }

    void shrink_to_fit() {
      if(size_t s = mEnd - mBegin; s > 0) {
        _grow(s);
      }
    }

    void clear() {
      for(void* d : *this) {
        mTraits->destruct(d);
      }
      mEnd = mBegin;
    }

    void* emplace_back() {
      auto newEnd = mEnd + _elementSize();
      if(newEnd > mCapacity) {
        constexpr size_t MIN_SIZE = 10;
        constexpr size_t GROWTH_FACTOR = 2;
        const size_t newBytes = mBegin == nullptr ? (MIN_SIZE * _elementSize()) : ((mCapacity - mBegin) * GROWTH_FACTOR);
        _grow(newBytes);
        newEnd = mEnd + _elementSize();
      }
      mTraits->defaultConstruct(mEnd);
      void* result = mEnd;
      mEnd = newEnd;
      return result;
    }

    void pop_back() {
      mEnd -= _elementSize();
      mTraits->destruct(mEnd);
    }

    void resize(size_t newSize) {
      if(newSize <= size()) {
        const size_t eSize = _elementSize();
        //Destroy all the elements past the old end
        for(size_t i = newSize; i < size(); ++i) {
          mTraits->destruct(mBegin + eSize*i);
        }
        //Move the end marker back to the new smaller end
        mEnd = mBegin + eSize*newSize;
      }
      else {
        _grow(newSize * _elementSize());
        const size_t oldSize = size();
        mEnd = mBegin + _elementSize()*newSize;
        for (size_t i = oldSize; i < newSize; ++i) {
          mTraits->defaultConstruct(at(i));
        }
      }
    }

    void swap(BlockVector& rhs) {
      std::swap(mBegin, rhs.mBegin);
      std::swap(mEnd, rhs.mEnd);
      std::swap(mCapacity, rhs.mCapacity);
      std::swap(mTraits, rhs.mTraits);
    }

    const ITraits* getTraits() const {
      return mTraits;
    }

  private:
    void _grow(size_t newBytes) {
      assert((newBytes % mTraits->size()) == 0);
      auto alloc = _getAllocator();
      //Multiply by growth factor or start at MIN_SIZE
      const size_t elementSize = _elementSize();
      const size_t newBytesSize = newBytes;
      const size_t sz = size();
      auto newBegin = alloc.allocate(newBytesSize);
      auto newEnd = newBegin + std::min(sz * elementSize, newBytes);

      //TODO: optimization for if traits support memcpy
      for(size_t i = 0; i < sz; ++i) {
        uint8_t* from = mBegin + i*elementSize;
        uint8_t* to = newBegin + i*elementSize;
        if(to < newEnd) {
          mTraits->moveConstruct(from, to);
        }
        mTraits->destruct(from);
      }

      auto* toDelete = mBegin;
      const size_t oldBytes = mCapacity - mBegin;
      mBegin = newBegin;
      mEnd = newEnd;
      mCapacity = mBegin + newBytesSize;

      if(toDelete) {
        alloc.deallocate(toDelete, oldBytes);
      }
    }

    Allocator _getAllocator() const {
      return {};
    }

    size_t _elementSize() const {
      return mTraits->size();
    }

    const ITraits* mTraits = nullptr;
    uint8_t* mBegin = nullptr;
    uint8_t* mEnd = nullptr;
    uint8_t* mCapacity = nullptr;
  };
}