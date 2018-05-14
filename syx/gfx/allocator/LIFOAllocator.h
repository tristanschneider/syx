#pragma once
//Thread local allocators intended to be used for temporary allocations in LIFO order
//Use StrictLIFOAllocator when possible, as this is the simplest but requires the allocations to be exactly in LIFO order
//LIFOAllocator is optimized for LIFO access but allows non-LIFO allocations

struct LIFOAllocatorFooter {
  LIFOAllocatorFooter* prev;
};

struct LIFOAllocatorPage {
  LIFOAllocatorPage(size_t size) {
    mSize = size;
    //After null footer
    mTopFooter = nullptr;
    mBuffer = new uint8_t[size];
  }
  ~LIFOAllocatorPage() {
    delete [] mBuffer;
  }

  size_t mSize;
  LIFOAllocatorFooter* mTopFooter;
  uint8_t* mBuffer;
};

extern thread_local std::unique_ptr<LIFOAllocatorPage> gLifoAllocatorPage;

template <class T>
class LIFOAllocator {
public:
  static const size_t SIZE = 1024*1024;

  typedef T value_type;

  LIFOAllocator() = default;

  template <class U>
  constexpr LIFOAllocator(const LIFOAllocator<U>&) noexcept {
  }

  T* allocate(std::size_t n) {
    size_t objSize = sizeof(T)*n;
    //If there's no space left fall back to malloc
    if(_getAvailableSize() < objSize) {
      return static_cast<T*>(std::malloc(objSize));
    }

    Page& page = _getPage();
    size_t blockSize = objSize + FOOTER_SIZE;
    uint8_t* result = _getTop();

    //Add to head of linked list
    Footer* newTopFooter = reinterpret_cast<Footer*>(result + objSize);
    newTopFooter->prev = page.mTopFooter ? page.mTopFooter : nullptr;
    page.mTopFooter = newTopFooter;

    return reinterpret_cast<T*>(result);
  }

  void deallocate(T* p, std::size_t) noexcept {
    Footer* nextFooter = nullptr;
    Footer* curFooter = _getPage().mTopFooter;
    //If there is no footer then this must have been allocated with malloc
    if(!curFooter) {
      std::free(p);
      return;
    }

    //Walk from top to bottom to find p
    while(curFooter->prev) {
      nextFooter = curFooter;
      curFooter = curFooter->prev;
      //Compare with block directly after footer of previous block
      if(reinterpret_cast<T*>(curFooter + 1) == p) {
        //If this was the top block, decrement the page's top
        if(nextFooter == _getPage().mTopFooter) {
          _getPage().mTopFooter = curFooter;
        }
        else {
          //Remove cur from the linked list
          nextFooter->prev = curFooter->prev;
        }
        //p was found and removed from list
        return;
      }
    }

    if(p == reinterpret_cast<T*>(_getPage().mBuffer)) {
      _getPage().mTopFooter = nullptr;
      return;
    }

    //p was not found, must have been allocated with malloc
    std::free(p);
  }

private:
  using Page = LIFOAllocatorPage;
  using Footer = LIFOAllocatorFooter;
  static const size_t FOOTER_SIZE;

  Page& _getPage() const {
    return *gLifoAllocatorPage;
  }

  uint8_t* _getTop() const {
    //If there is a footer the next free block is right after the footer, follow it and get the address in buffer. If no footers, get start of buffer
    return _getPage().mTopFooter ? reinterpret_cast<uint8_t*>(_getPage().mTopFooter + 1) : _getPage().mBuffer;
  }

  size_t _getAvailableSize() const {
    return _getPage().mSize - (_getTop() - _getPage().mBuffer);
  }
};

template<typename T>
const size_t LIFOAllocator<T>::FOOTER_SIZE = sizeof(LIFOAllocator::Footer);

template <class T, class U>
bool operator==(const LIFOAllocator<T>&, const LIFOAllocator<U>&) {
  return true;
}

template <class T, class U>
bool operator!=(const LIFOAllocator<T>&, const LIFOAllocator<U>&) {
  return false;
}

#ifndef NDEBUG
#define STRICT_LIFO_VALIDATE
#endif

struct StrictLIFOAllocatorPage {
  static const size_t SIZE = 1024*1024;

  uint8_t mBuffer[SIZE];
  size_t mTop = 0;
  std::stack<void*> mAllocations;
};

extern thread_local std::unique_ptr<StrictLIFOAllocatorPage> gStrictLIFOAllocatorPage;

template <class T>
class StrictLIFOAllocator {
public:
  typedef T value_type;

  StrictLIFOAllocator() = default;

  template <class U>
  constexpr StrictLIFOAllocator(const StrictLIFOAllocator<U>&) noexcept {
  }

  T* allocate(std::size_t n) {
    auto& page = gStrictLIFOAllocatorPage;
    size_t availableSize = StrictLIFOAllocatorPage::SIZE - page->mTop;
    size_t objSize = sizeof(T)*n;
    if(objSize > availableSize)
      throw std::exception("Out of memory");

    T* result = reinterpret_cast<T*>(&page->mBuffer[page->mTop]);
    page->mTop += objSize;

#ifdef STRICT_LIFO_VALIDATE
    page->mAllocations.push(result);
#endif

    return result;
  }

  void deallocate(T* p, std::size_t n) noexcept {
    auto& page = gStrictLIFOAllocatorPage;
    page->mTop -= sizeof(T)*n;
#ifdef STRICT_LIFO_VALIDATE
    assert(page->mAllocations.top() == p && "Allocations using StrictLIFO should be in LIFO order");
    page->mAllocations.pop();
#endif
  }
};

template <class T, class U>
bool operator==(const StrictLIFOAllocator<T>&, const StrictLIFOAllocator<U>&) {
  return true;
}

template <class T, class U>
bool operator!=(const StrictLIFOAllocator<T>&, const StrictLIFOAllocator<U>&) {
  return false;
}