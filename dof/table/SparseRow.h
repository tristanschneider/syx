#pragma once

#include "IRow.h"
#include "generics/DynamicBitset.h"
#include <cassert>

class PackedIndexArray {
public:
  using IndexBase = size_t;

  //Min nonzero size this can be. The capacity that will result in push_back of an empty PackedIndexArray
  static constexpr size_t MIN_SIZE = 10;

  class ConstIterator {
  public:
    using value_type = IndexBase;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    ConstIterator(const uint8_t* buff, uint8_t width)
      : buffer{ buff }
      , mask{ (static_cast<size_t>(1) << (width * 8)) - 1 }
      , byteWidth{ width }
    {
    }

    ConstIterator& operator++() {
      advance(1);
      return *this;
    }

    ConstIterator operator++(int) {
      ConstIterator tmp{ *this };
      ++*this;
      return tmp;
    }

    value_type operator*() const {
      return reinterpret_cast<const IndexBase&>(*buffer) & mask;
    }

    auto operator<=>(const ConstIterator&) const = default;

    std::ptrdiff_t operator-(const ConstIterator& rhs) const {
      return (buffer - rhs.buffer) / byteWidth;
    }

    ConstIterator operator+(size_t i) const {
      ConstIterator res{ *this };
      res.advance(i);
      return res;
    }

  protected:
    void advance(IndexBase i) {
      buffer += (i*byteWidth);
    }

    const uint8_t* buffer{};
    size_t mask{};
    uint8_t byteWidth{};
  };

  class Iterator : public ConstIterator {
  public:
    using value_type = IndexBase;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    Iterator(uint8_t* buff, uint8_t width)
      : ConstIterator{ buff, width }
    {
    }

    Iterator& operator++() {
      advance(1);
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp{ *this };
      ++*this;
      return tmp;
    }

    Iterator operator+(size_t i) const {
      Iterator res{ *this };
      res.advance(i);
      return res;
    }

    void operator=(IndexBase i) {
      size_t& v = reinterpret_cast<size_t&>(*getBuffer());
      //Set just the masked bits to i
      v = (v & ~this->mask) | (i & this->mask);
    }

  private:
    uint8_t* getBuffer() {
      return const_cast<uint8_t*>(buffer);
    }
  };

  PackedIndexArray() = default;
  ~PackedIndexArray() {
    reset();
  }
  PackedIndexArray(PackedIndexArray&& rhs) {
    swap(rhs);
  }

  PackedIndexArray& operator=(PackedIndexArray&& rhs) {
    if(this != &rhs) {
      PackedIndexArray temp{ std::move(rhs) };
      swap(temp);
    }
    return *this;
  }

  void swap(PackedIndexArray& rhs) {
    std::swap(buffer, rhs.buffer);
    std::swap(bufferSize, rhs.bufferSize);
    std::swap(bufferCapacity, rhs.bufferCapacity);
    std::swap(byteWidth, rhs.byteWidth);
  }

  ConstIterator at(IndexBase i) const {
    assert(i < size());
    return cbegin() + i;
  }

  Iterator at(IndexBase i) {
    assert(i < size());
    return begin() + i;
  }

  ConstIterator operator[](IndexBase i) const {
    return at(i);
  }

  Iterator operator[](IndexBase i) {
    return at(i);
  }

  void resize(IndexBase newSize, size_t maxIndex) {
    //If there is enough capacity for the size, use the current buffer
    if(canReuseForCapacity(newSize, maxIndex)) {
      //Reset any values going out of bounds. This allows resizing up to always assume values are default initialized
      for(IndexBase i = newSize; i < size(); ++i) {
        at(i) = 0;
      }
      bufferSize = newSize;
      return;
    }

    //This must allocate a new array which means values beyond previous size are default initialized
    reallocate(newSize, maxIndex);
    bufferSize = newSize;
  }

  void reserve(IndexBase newCapacity, size_t maxIndex) {
    if(!canReuseForCapacity(newCapacity, maxIndex)) {
      reallocate(newCapacity, maxIndex);
    }
  }

  IndexBase size() const {
    return bufferSize;
  }

  IndexBase capacity() const {
    return bufferCapacity;
  }

  ConstIterator cbegin() const {
    return { buffer, getByteWidth() };
  }

  ConstIterator cend() const {
    const IndexBase w = getByteWidth();
    return { buffer + w*size(), static_cast<uint8_t>(w) };
  }

  Iterator begin() {
    return { buffer, getByteWidth() };
  }

  Iterator end() {
    const IndexBase w = getByteWidth();
    return { buffer + w*size(), static_cast<uint8_t>(w) };
  }

  void pop_back() {
    at(size() - 1) = 0;
    --bufferSize;
  }

  void push_back(IndexBase i) {
    if(capacity() <= size()) {
      const size_t newSize = std::max(MIN_SIZE, capacity()*2);
      reallocate(newSize, byteWidth);
    }
    const size_t e = size();
    ++bufferSize;
    at(e) = i;
  }

  void clear() {
    if(buffer) {
      std::memset(buffer, 0, bufferSize*static_cast<uint32_t>(getByteWidth()));
    }
    bufferSize = 0;
  }

private:
  bool canReuseForCapacity(size_t newSize, size_t maxIndex) const {
    return newSize <= capacity() && getByteWidth(maxIndex) == byteWidth;
  }

  void reallocate(size_t newSize, size_t maxIndex) {
    reallocateWidth(newSize, getByteWidth(maxIndex));
  }

  void reallocateWidth(size_t newSize, size_t newWidth) {
    PackedIndexArray old;
    old.swap(*this);

    byteWidth = newWidth;
    buffer = new uint8_t[static_cast<IndexBase>(byteWidth)*newSize]{};
    bufferSize = old.size();
    bufferCapacity = newSize;
    for(IndexBase i = 0; i < old.size(); ++i) {
      at(i) = *old.at(i);
    }
  }

  static constexpr uint8_t getByteWidth(IndexBase size) {
    return static_cast<uint8_t>((std::bit_width(size) + 7) / 8);
  }

  uint8_t getByteWidth() const {
    return getByteWidth(bufferCapacity);
  }

  void reset() {
    if(buffer) {
      delete [] buffer;
      release();
    }
  }

  void release() {
    buffer = nullptr;
    bufferSize = bufferCapacity = 0;
  }

  uint8_t* buffer{};
  uint32_t bufferSize{};
  uint32_t bufferCapacity{};
  uint8_t byteWidth = 1;
};

class SparseRowBase : public IRow {
public:
  using IteratorBase = PackedIndexArray::Iterator;
  using ConstIteratorBase = PackedIndexArray::ConstIterator;

  static constexpr size_t SENTINEL_OFFSET = 1;

  SparseRowBase() {
    //Reverse sentinel so zero can be invalid index
    denseToSparse.push_back(0);
  }

  IteratorBase beginBase() { return denseToSparse.begin() + SENTINEL_OFFSET; }
  IteratorBase endBase() { return denseToSparse.end(); }
  ConstIteratorBase beginBase() const { return denseToSparse.cbegin() + SENTINEL_OFFSET; }
  ConstIteratorBase endBase() const { return denseToSparse.cend(); }

  ConstIteratorBase findBase(size_t sparse) const {
    const PackedIndexArray::IndexBase denseIndex = *sparseToDense.at(sparse);
    return denseIndex ? denseToSparse.at(denseIndex) : denseToSparse.cend();
  }

  size_t size() const { return denseToSparse.size() - SENTINEL_OFFSET; }

protected:
  virtual void onMove(size_t from, size_t to, size_t count) = 0;
  virtual void onReset(size_t index, size_t count) = 0;
  virtual void onResize(size_t oldSize, size_t newSize) = 0;

  void resizeBase(size_t newSize) {
    if(newSize < sparseToDense.size()) {
      const size_t toRemove = sparseToDense.size() - newSize;
      const size_t existing = denseToSparse.size();
      //Sizing down is inefficient to determine which of the removed elements had sparse entries
      //Do the least traversal by either traversing removed entries or current entries based on which is smaller
      if(toRemove < existing) {
        for(auto it = sparseToDense.at(newSize); it != sparseToDense.end(); ++it) {
          if(*it) {
            erase(it - sparseToDense.begin());
          }
        }
      }
      else {
        for(auto it = beginBase(); it != endBase();) {
          if(*it >= newSize) {
            erase(*it);
          }
          else {
            ++it;
          }
        }
      }
    }

    sparseToDense.resize(newSize, denseToSparse.size());
  }

  size_t erase(size_t sparseIndex) {
    auto it = sparseToDense.at(sparseIndex);
    if(*it) {
      const PackedIndexArray::IndexBase freeDenseIndex = *it;
      const PackedIndexArray::IndexBase swapDenseIndex = denseToSparse.size() - 1;

      onMove(swapDenseIndex - SENTINEL_OFFSET, freeDenseIndex - SENTINEL_OFFSET, 1);

      auto denseIt = denseToSparse.at(*it);
      //Erase the mapping of the sparse id to the dense index
      it = 0;

      auto toSwap = denseToSparse.at(denseToSparse.size() - 1);
      if(toSwap != denseIt) {
        //Point the dense mapping of the erased element to the sparse index of the swap element
        denseIt = *toSwap;
        //Point the sparse mapping at the dense slot that was just erased
        sparseToDense.at(*toSwap) = freeDenseIndex;
      }
      denseToSparse.pop_back();
      return freeDenseIndex;
    }
    return {};
  }

  void clear() {
    //Erase sparse mappings
    for(auto it = beginBase(); it != endBase(); ++it) {
      sparseToDense.at(*it) = 0;
    }
    //Reset packed values
    onReset(0, denseToSparse.size() - SENTINEL_OFFSET);
    //Erase packed mappings
    denseToSparse.clear();
    //Put back the sentinel element that was cleared
    denseToSparse.push_back(0);
  }

  //Reassociate the dense mapping with a new sparse mapping
  //This means the table index changed but the value should remain the same. Intended for moving
  void remap(size_t fromSparse, size_t toSparse) {
    if(auto dense = sparseToDense.at(fromSparse); *dense) {
      //Point the new sparse mapping at the dense element the previous was pointing at
      sparseToDense.at(toSparse) = *dense;
      //Point the dense mapping at the sparse index that was remapped
      denseToSparse.at(*dense) = toSparse;
      //Clear the previous sparse mapping
      dense = 0;
    }
  }

  size_t emplace_back(size_t sparseIndex) {
    assert(sparseIndex < sparseToDense.size());
    CapacityEventScope scope{ *this };

    const size_t dense = denseToSparse.size();
    denseToSparse.push_back(sparseIndex);
    sparseToDense.at(sparseIndex) = dense;

    return dense - SENTINEL_OFFSET;
  }

  void trySwapRemove(size_t toRemove, size_t toSwap) {
    //Erase the original element
    size_t removed = erase(toRemove);
    if(removed) {
      //Remap the last element to the new swap location. Doesn't need to change the dense element itself
      if(toRemove != toSwap) {
        remap(toSwap, toRemove);
      }
    }
  }

  struct CapacityEventScope {
    CapacityEventScope(SparseRowBase& s)
      : self{ s }
    {
      prevCapacity = self.denseToSparse.capacity();
      prevSize = self.denseToSparse.size();
    }

    ~CapacityEventScope() {
      if(prevCapacity != self.denseToSparse.capacity()) {
        //Use size as previous because that's what needs to be copied, but total space should be capacity
        self.onResize(prevSize, self.denseToSparse.capacity());
      }
    }

    SparseRowBase& self;
    size_t prevCapacity{};
    size_t prevSize{};
  };

  PackedIndexArray sparseToDense;
  PackedIndexArray denseToSparse;
};

//Row containing optional values that is efficient to iterate through
//This only makes sense if wanting to iterate over all elements that contain an uncommon value in the table
//If iterating over the entire table then one might as well use a basic/slim row with a value to indicate empty
template<class Element>
class SparseRow : public SparseRowBase {
public:
  using ElementT = Element;
  using ElementPtr = Element*;
  using SelfT = SparseRow<ElementT>;

  class ConstIterator {
  public:
    using value_type = std::pair<size_t, const ElementT&>;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    ConstIterator(SparseRowBase::ConstIteratorBase it, const ElementT* buff)
      : wrapped{ it }
      , packed{ buff }
    {
    }

    ConstIterator& operator++() {
      advance(1);
      return *this;
    }

    ConstIterator operator++(int) {
      ConstIterator temp{ *this };
      advance(1);
      return temp;
    }

    std::ptrdiff_t operator-(const ConstIterator& rhs) const {
      return packed - rhs.packed;
    }

    auto operator<=>(const ConstIterator&) const = default;

    value_type operator*() const {
      return { *wrapped, *packed };
    }

    size_t key() const {
      return static_cast<size_t>(*wrapped);
    }

    const ElementT& value() const {
      return *packed;
    }

  protected:
    void advance(size_t i) {
      wrapped = wrapped + i;
      packed += i;
    }

    SparseRowBase::ConstIteratorBase wrapped;
    const ElementT* packed{};
  };


  class Iterator : public ConstIterator {
  public:
    using value_type = std::pair<size_t, ElementT&>;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    Iterator(SparseRowBase::ConstIteratorBase it, ElementT* buff)
      : ConstIterator{ it, buff }
    {
    }

    value_type operator*() const {
      return { *this->wrapped, *getPacked() };
    }

    const ElementT& value() const {
      return *getPacked();
    }

  private:
    ElementT* getPacked() const {
      return const_cast<ElementT*>(this->packed);
    }
  };

  using IteratorT = Iterator;
  using ConstIteratorT = ConstIterator;

  SparseRow() {
    //Account for the space caused by adding the sentinel value in SparseRowBase
    onResize(0, PackedIndexArray::MIN_SIZE);
  }

  SparseRow(SparseRow&& rhs) {
    swap(rhs);
  }

  void swap(SparseRow& rhs) {
    std::swap(sparseToDense, rhs.sparseToDense);
    std::swap(denseToSparse, rhs.denseToSparse);
    std::swap(packedValues, rhs.packedValues);
  }

  ~SparseRow() {
    delete [] packedValues;
    packedValues = nullptr;
  }

  SparseRow(const SparseRow&) = delete;

  RowBuffer getElements() final {
    return { packedValues };
  }

  ConstRowBuffer getElements() const final {
    return { packedValues };
  }

  void resize(size_t, size_t newSize) final {
    resizeBase(newSize);
  }

  void swapRemove(size_t begin, size_t end, size_t tableSize) final {
    for(size_t i = 0; i < end - begin; ++i) {
      trySwapRemove(end - (i + 1), --tableSize);
    }
  }

  Iterator begin() {
    return wrapIterator(beginBase());
  }

  Iterator end() {
    return wrapIterator(endBase());
  }

  ConstIterator begin() const {
    return wrapIterator(beginBase());
  }

  ConstIterator end() const {
    return wrapIterator(endBase());
  }

  size_t getPackedIndex(const ConstIteratorBase& it) const {
    return static_cast<size_t>(it - beginBase());
  }

  Iterator find(size_t sparse) {
    return wrapIterator(findBase(sparse));
  }

  ConstIterator find(size_t sparse) const {
    return wrapIterator(findBase(sparse));
  }

  void erase(size_t sparse) {
    SparseRowBase::erase(sparse);
  }

  void clear() {
    SparseRowBase::clear();
  }

  bool contains(size_t sparse) const {
    return find(sparse) != end();
  }

  ElementT& getOrAdd(size_t sparse) {
    if(auto it = find(sparse); it != end()) {
      return (*it).second;
    }
    size_t i = emplace_back(sparse);
    return packedValues[i];
  }

  void migrateElements(const MigrateArgs& args) final {
    //If from is null there's nothing to do as no packed elements would exist
    if(SelfT* from = static_cast<SelfT*>(args.fromRow)) {
      for(size_t i = 0; i < args.count; ++i) {
        const size_t fromSparse = i + args.fromIndex;
        const size_t toSparse = i + args.toIndex;
        if(auto it = from->find(fromSparse); it != from->end()) {
          auto&& [fs, fromElement] = *it;
          getOrAdd(toSparse) = std::move(fromElement);
        }
      }
    }
  }

protected:
  Iterator wrapIterator(const ConstIteratorBase& it) {
    return { it, packedValues + getPackedIndex(it) };
  }

  ConstIterator wrapIterator(const ConstIteratorBase& it) const {
    return { it, packedValues + getPackedIndex(it) };
  }

  void onReset(size_t index, size_t count) final {
    for(size_t i = 0; i < count; ++i) {
      packedValues[i + index] = {};
    }
  }

  void onMove(size_t from, size_t to, size_t count) final {
    for(size_t i = 0; i < count; ++i) {
      ElementT& toSwap = packedValues[from + i];
      packedValues[to + i] = std::move(toSwap);
      toSwap = {};
    }
  }

  void onResize(size_t oldSize, size_t newSize) {
    ElementT* prev = packedValues;
    packedValues = new ElementT[newSize]{};
    if(prev) {
      //This assumes size only ever goes up
      for(size_t i = 0; i < oldSize; ++i) {
        packedValues[i] = std::move(prev[i]);
      }
    }
    if(prev) {
      delete [] prev;
    }
  }

private:
  ElementT* packedValues{};
};

class SparseFlagRow : public SparseRowBase {
public:
  using ElementT = uint8_t;
  using ElementPtr = uint8_t*;
  using SelfT = SparseFlagRow;
  //Since iterators only contain mapping information there's no sense exposing mutable versions
  //The iterators are only used to determine which entities have the flags
  using IteratorT = SparseRowBase::ConstIteratorBase;
  using ConstIteratorT = SparseRowBase::ConstIteratorBase;

  SparseFlagRow() = default;
  SparseFlagRow(const SparseFlagRow&) = delete;

  RowBuffer getElements() final {
    return {};
  }

  ConstRowBuffer getElements() const final {
    return {};
  }

  void resize(size_t, size_t newSize) final {
    resizeBase(newSize);
  }

  void swapRemove(size_t begin, size_t end, size_t tableSize) final {
    for(size_t i = 0; i < end - begin; ++i) {
      trySwapRemove(end - (i + 1), --tableSize);
    }
  }

  ConstIteratorT begin() const {
    return beginBase();
  }

  ConstIteratorT end() const {
    return endBase();
  }

  ConstIteratorT find(size_t sparse) const {
    return findBase(sparse);
  }

  void erase(size_t sparse) {
    SparseRowBase::erase(sparse);
  }

  bool contains(size_t sparse) const {
    return find(sparse) != end();
  }

  //Adds the flag to the sparse element if it didn't have it and return true if it was added
  bool getOrAdd(size_t sparse) {
    return contains(sparse) ? false : emplace_back(sparse), true;
  }

  void migrateElements(const MigrateArgs& args) final {
    //If from is null there's nothing to do as no packed elements would exist
    if(SelfT* from = static_cast<SelfT*>(args.fromRow)) {
      for(size_t i = 0; i < args.count; ++i) {
        const size_t fromSparse = i + args.fromIndex;
        const size_t toSparse = i + args.toIndex;
        if(from->contains(fromSparse)) {
          getOrAdd(toSparse);
        }
      }
    }
  }

protected:
  void onReset(size_t, size_t) final {}
  void onMove(size_t, size_t, size_t) final {}
  void onResize(size_t, size_t) {}
};