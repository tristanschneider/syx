#include <memory>

namespace ecx {
  struct IBlockVectorTraits {
    virtual ~IBlockVectorTraits() = default;
    virtual size_t size() const = 0;
    virtual void destruct(void* target) const = 0;
    virtual void defaultConstruct(void* target) const = 0;
    virtual void moveConstruct(void* from, void* to) const = 0;
    virtual void copyConstruct(const void* from, void* to) const = 0;
    virtual void moveAssign(void* from, void* to) const = 0;
    virtual void copyAssign(void* from, void* to) const = 0;
  };

  //Vector where each object is a block of a consistent size
  template<class Allocator = std::allocator<uint8_t>>
  class BlockVector {
  public:
    using ITraits = IBlockVectorTraits;

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
      clear();

      const size_t newSize = rhs.size();
      if(newSize > capacity()) {
        _grow(newSize);
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
      _grow(newSize * _elementSize());
      const size_t oldSize = size();
      mEnd = mBegin + _elementSize()*newSize;
      for (size_t i = oldSize; i < newSize; ++i) {
        mTraits->defaultConstruct(at(i));
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
      auto alloc = _getAllocator();
      //Multiply by growth factor or start at MIN_SIZE
      const size_t elementSize = _elementSize();
      const size_t newBytesSize = newBytes;
      const size_t sz = size();
      auto newBegin = alloc.allocate(newBytesSize);
      auto newEnd = newBegin + sz * elementSize;

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

      alloc.deallocate(toDelete, oldBytes);
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

  template<class T>
  struct BlockVectorTraits : public IBlockVectorTraits {
    size_t size() const override {
      return sizeof(T);
    }

    void destruct(void* target) const override {
      static_cast<T*>(target)->~T();
    }

    void defaultConstruct(void* target) const override {
      new (target) T();
    }

    void moveConstruct(void* from, void* to) const override {
      new (to) T(std::move(*static_cast<T*>(from)));
    }

    void copyConstruct(const void* from, void* to) const override {
      new (to) T(*static_cast<const T*>(from));
    }

    void moveAssign(void* from, void* to) const override {
      *static_cast<T*>(to) = std::move(*static_cast<T*>(from));
    }

    void copyAssign(void* from, void* to) const override {
      *static_cast<T*>(to) = *static_cast<T*>(from);
    }
  };

  struct CompositeBlockVectorTraits : public IBlockVectorTraits {
    CompositeBlockVectorTraits(const std::vector<const IBlockVectorTraits*>& traits)
      : mTraits(traits.size()) {
      for(size_t i = 0; i < traits.size(); ++i) {
        //TODO: padding for alignment?
        mTraits[i].mOffset = mTotalSize;
        mTraits[i].mTraits = traits[i];
        mTotalSize += traits[i]->size();
      }
    }

    size_t size() const override {
      return mTotalSize;
    }

    void destruct(void* target) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->destruct(_offset(target, t.mOffset));
      }
    }

    void defaultConstruct(void* target) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->defaultConstruct(_offset(target, t.mOffset));
      }
    }

    void moveConstruct(void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->moveConstruct(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    void copyConstruct(const void* from, void* to) const override {
      for(const Traits& t : mTraits) {
        t.mTraits->copyConstruct(_offset(from, t.mOffset), _offset(to, t.mOffset));
      }
    }

    static void* _offset(void* ptr, size_t o) {
      return static_cast<uint8_t*>(ptr) + o;
    }

    static const void* _offset(const void* ptr, size_t o) {
      return static_cast<const uint8_t*>(ptr) + o;
    }

    struct Traits {
      size_t mOffset = 0;
      const IBlockVectorTraits* mTraits = nullptr;
    };
    std::vector<Traits> mTraits;
    size_t mTotalSize = 0;
  };

  struct MemCopyTraits : IBlockVectorTraits {
    MemCopyTraits(size_t s)
      : mSize(s) {
    }

    size_t size() const override {
      return mSize;
    }

    void destruct(void*) const override {}

    void defaultConstruct(void*) const override {}

    void moveConstruct(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    void copyConstruct(const void* from, void* to) const override {
      std::memcpy(to, from, mSize);
    }

    void moveAssign(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    void copyAssign(void* from, void* to) const override {
      copyConstruct(from, to);
    }

    const size_t mSize = 0;
  };

}