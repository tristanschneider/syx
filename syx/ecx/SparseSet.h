#pragma once
#include <vector>
#include <memory>

namespace ecx {
  // Default traits, specialize for customization
  template<class T>
  struct SparseSetStorageTraits {
    static constexpr size_t PAGE_SIZE = 255;
    static constexpr T EMPTY_VALUE = std::numeric_limits<T>::max();
  };

  template<class T, size_t PageSize, T EmptyValue>
  class PagedVector {
  public:
    T& set(size_t index, T value) {
      return getOrCreate(index) = value;
    }

    void reset(size_t index) {
      getOrCreate(index) = EmptyValue;
    }

    void clear() {
      mPages.clear();
    }

    const T* tryGet(size_t index) const {
      const size_t pageIndex = index / PageSize;
      if(pageIndex < mPages.size()) {
        if(const Page& page = mPages[pageIndex]) {
          if(const T& result = (*page)[index % PageSize]; result != EmptyValue) {
            return &result;
          }
        }
      }
      return nullptr;
    }

    T* tryGet(size_t index) {
      const size_t pageIndex = index / PageSize;
      if(pageIndex < mPages.size()) {
        if(Page& page = mPages[pageIndex]) {
          if(T& result = (*page)[index % PageSize]; result != EmptyValue) {
            return &result;
          }
        }
      }
      return nullptr;
    }

    T& getOrCreate(size_t index) {
      const size_t pageIndex = index / PageSize;
      if(mPages.size() <= pageIndex) {
        mPages.resize(pageIndex + 1);
      }

      Page& page = mPages[pageIndex];
      if (!page) {
        page = std::make_unique<std::array<T, PageSize>>();
        page->fill(EmptyValue);
      }
      return (*page)[index % PageSize];
    }

  private:
    using Page = std::unique_ptr<std::array<T, PageSize>>;
    std::vector<Page> mPages;
  };

  template<class T>
  struct SparseValuePair {
    T mSparseId = {};
    //It's probably possible to make this a smaller type depending on page size
    //Due to padding that may not be any benefit
    T mPackedId = {};
  };

  template<class T>
  class SparseSet {
  public:
    class Iterator {
    public:
      friend class SparseSet<T>;
      using value_type = SparseValuePair<T>;
      using difference_type = std::ptrdiff_t;
      using pointer = SparseValuePair<T>*;
      using reference = SparseValuePair<T>&;
      using iterator_category = std::forward_iterator_tag;

      Iterator(const T* begin, T index)
        : mBegin(begin)
        , mIndex(index) {
      }

      Iterator(const Iterator&) = default;
      Iterator& operator=(const Iterator&) = default;

      Iterator& operator++() {
        ++mIndex;
        return *this;
      }

      Iterator operator++(int) {
        auto result = *this;
        ++(*this);
        return result;
      }

      Iterator& operator--() {
        --mIndex;
        return *this;
      }

      Iterator operator--(int) {
        auto result = *this;
        --(*this);
        return result;
      }

      bool operator==(const Iterator& other) const {
        return address() == other.address();
      }

      bool operator!=(const Iterator& other) const {
        return !(*this == other);
      }

      SparseValuePair<T> operator*() const {
        return { *(mBegin + mIndex), mIndex };
      }

      SparseValuePair<T> value() const {
        return **this;
      }

    private:
      const T* address() const {
        return mBegin + mIndex;
      }

      const T* mBegin = nullptr;
      T mIndex = {};
    };

    Iterator begin() const {
      return { mPacked.data(), T(0) };
    }

    Iterator end() const {
      return { mPacked.data(), T(mPacked.size()) };
    }

    SparseValuePair<T> insert(T value) {
      auto& packedIndex = mSparse.getOrCreate(static_cast<size_t>(value));
      //If value already exists, return as-is
      if(packedIndex != EMPTY_VALUE) {
        return { value, packedIndex };
      }

      //Value doesn't already exist, add a new packed value
      SparseValuePair<T> result{ value, static_cast<T>(mPacked.size()) };
      mPacked.push_back(value);
      packedIndex = result.mPackedId;
      return result;
    }

    Iterator erase(const Iterator& it) {
      assert(it.mBegin == mPacked.data() && "Iterator must be valid");
      const SparseValuePair<T> toRemove = *it;
      mSparse.reset(toRemove.mSparseId);
      //Swap remove
      if(mPacked.size()) {
        mPacked[toRemove.mPackedId] = mPacked.back();
        mPacked.pop_back();
        //Packed id will be out of bounds if the last element was removed
        if(static_cast<size_t>(toRemove.mPackedId) < mPacked.size()) {
          //Update sparse value for the value that just got swapped
          mSparse.set(static_cast<T>(mPacked[toRemove.mPackedId]), toRemove.mPackedId);
        }
      }
      //This is now either the end or pointing at a swapped in value
      return { mPacked.data(), T(toRemove.mPackedId) };
    }

    Iterator find(T value) const {
      const T* found = mSparse.tryGet(static_cast<size_t>(value));
      return found ? Iterator{ mPacked.data(), T(*found) } : end();
    }

    //Swap the order of these two values in packed storage.
    //Returns the swapped iterators: <b, a>
    std::pair<Iterator, Iterator> swap(T a, T b) {
      T* foundA = mSparse.tryGet(static_cast<size_t>(a));
      T* foundB = mSparse.tryGet(static_cast<size_t>(b));
      if(foundA && foundB) {
        std::swap(*foundA, *foundB);
        std::swap(mPacked[*foundA], mPacked[*foundB]);
        return std::make_pair(Iterator{ mPacked.data(), *foundA }, Iterator{ mPacked.data(), *foundB });
      }
      //Ignore the case where one was found since that's still nonsense for a swap
      return std::make_pair(end(), end());
    }

    Iterator reverseLookup(size_t index) const {
      return index < mPacked.size() ? Iterator{ mPacked.data(), T(index) } : end();
    }

    void clear() {
      mPacked.clear();
      mSparse.clear();
    }

    size_t size() const {
      return mPacked.size();
    }

    bool empty() const {
      return mPacked.empty();
    }

  private:
    static constexpr size_t PAGE_SIZE = SparseSetStorageTraits<T>::PAGE_SIZE;
    static constexpr T EMPTY_VALUE = SparseSetStorageTraits<T>::EMPTY_VALUE;

    using Page = std::unique_ptr<std::array<T, PAGE_SIZE>>;

    std::vector<T> mPacked;
    PagedVector<T, PAGE_SIZE, EMPTY_VALUE> mSparse;
  };
}