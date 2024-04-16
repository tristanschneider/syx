#pragma once

#include <tuple>
#include <memory>
#include "ContainerTraits.h"

namespace gnx {
  template<class T, class Traits = DefaultContainerTraits<T>>
  class PagedVector {
  public:
    template<class Ty, class PagesT>
    class Iterator {
    public:
      using Pointer = std::add_pointer_t<Ty>;
      using Reference = std::add_lvalue_reference_t<Ty>;
      using PagePtr = std::add_pointer_t<Pointer>;
      using PageArray = PagesT;

      using iterator_category = std::bidirectional_iterator_tag;
      using value_type        = Ty;
      using difference_type   = size_t;
      using pointer           = Pointer;
      using reference         = Reference;

      Iterator(const Iterator&) = default;
      Iterator(PageArray pageList, size_t elementIndex)
        : pages{ pageList }
        , page{ elementIndex / PAGE_SIZE }
        , index{ elementIndex % PAGE_SIZE }
      {
      }

      Iterator(PageArray p, size_t pi, size_t i)
        : pages{ p }
        , page{ pi }
        , index{ i }
      {
      }

      Iterator& operator=(const Iterator&) = default;

      Pointer operator->() const { return &pages[page][index]; }
      Reference operator*() const { return pages[page][index]; }
      auto operator<=>(const Iterator&) const = default;

      Iterator& operator++() {
        ++index;
        if(index >= PAGE_SIZE) {
          ++page;
          index = 0;
        }
        return *this;
      }

      Iterator operator++(int) {
        auto result{ *this };
        ++(*this);
        return result;
      }

      Iterator& operator--() {
        if(index == 0) {
          index = PAGE_SIZE - 1;
          --page;
        }
        else {
          --index;
        }
        return *this;
      }

      Iterator operator--(int) {
        auto result{ *this };
        --(*this);
        return result;
      }

    private:
      PageArray pages{};
      size_t page{};
      size_t index{};
    };

    PagedVector() = default;
    ~PagedVector() {
      if(pages) {
        //Destructor on all elements. Split out to separate iteration for simplicity
        for(size_t i = 0; i < size(); ++i) {
          at(i).~T();
        }
        //Destroy pages, no destructor needed since they are plain arrays
        auto elementAllocator = getElementAllocator();
        for(size_t i = 0; i < pageCount; ++i) {
          elementAllocator.deallocate(pages[i], PAGE_SIZE);
        }
        getPageAllocator().deallocate(pages, pageCount);
        //Not necessary but makes debugging less confusing
        pages = nullptr;
      }
    }

    PagedVector(const PagedVector& rhs) {
      *this = rhs;
    }
    PagedVector(PagedVector&& rhs)
      : pages{ rhs.pages }
      , pageCount{ rhs.pageCount }
      , elementCount{ rhs.elementCount }
    {
      rhs.pages = nullptr;
    }

    PagedVector& operator=(const PagedVector& rhs) {
      resize(rhs.size());
      for(size_t i = 0; i < rhs.size(); ++i) {
        at(i) = rhs.at(i);
      }
      return *this;
    }

    PagedVector& operator=(PagedVector&& rhs) {
      PagedVector temp{ std::move(rhs) };
      swap(temp);
      return *this;
    }

    T& at(size_t i) {
      auto [page, index] = getPageAndIndex(i);
      return getPage(page)[index];
    }

    const T& at(size_t i) const {
      auto [page, index] = getPageAndIndex(i);
      return getPage(page)[index];
    }

    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }
    T& front() { return at(0); }
    const T& front() const { return at(0); }
    T& back() { return at(size() - 1); }
    const T& back() const { return at(size() - 1); }

    auto begin() {
      return Iterator<T, decltype(pages)>{ pages, static_cast<size_t>(0) };
    }

    auto cbegin() const {
      return Iterator<const T, decltype(pages)>{ pages, static_cast<size_t>(0) };
    }

    auto end() {
      return Iterator<T, decltype(pages)>{ pages, size() };
    }

    auto cend() const {
      return Iterator<const T, decltype(pages)>{ pages, size() };
    }

    bool empty() const { return size() == 0; }
    size_t size() const { return elementCount; }

    void reserve(size_t elements) {
      const size_t currentSize = size();
      if(elements > currentSize) {
        resizeExact(elements, (elements / PAGE_SIZE) + 1);
        //Hack to undo the set by resizeExact
        elementCount = currentSize;
      }
    }

    size_t capacity() const {
      return PAGE_SIZE*pageCount;
    }

    void clear() {
      resize(0);
      elementCount = 0;
    }

    void push_back(const T& v) { push_back(T{ v }); }

    void push_back(T&& v) {
      const size_t currentSize = size();
      auto [p, i] = getPageAndIndex(currentSize);
      if(p >= pageCount) {
        //Figure out new page count from requested vs growth factor
        const size_t newPageCount = std::max(p + 1, static_cast<size_t>((static_cast<float>(currentSize) * Traits::GROWTH_FACTOR) / PAGE_SIZE));
        resizeExact(currentSize + 1, newPageCount);
      }
      else {
        ++elementCount;
      }
      new (&pages[p][i])T(std::move(v));
    }

    void pop_back() {
      resize(size() - 1);
    }

    void resize(size_t s) {
      const size_t oldSize = size();
      resizeExact(s, (s / PAGE_SIZE) + 1);
      //Default initialize new elements, which is not necessarily all pages depending on growth factor
      for(size_t i = oldSize; i < s; ++i) {
        auto [pi, ii] = getPageAndIndex(i);
        T* value = &pages[pi][ii];
        new (value)T();
      }
    }

    void swap(PagedVector& rhs) {
      std::swap(pages, rhs.pages);
      std::swap(pageCount, rhs.pageCount);
      std::swap(elementCount, rhs.elementCount);
    }

  private:
    using PageT = T*;
    static constexpr size_t PAGE_SIZE = Traits::PAGE_SIZE;

    static constexpr std::pair<size_t, size_t> getPageAndIndex(size_t elementIndex) {
      return std::make_pair(elementIndex / PAGE_SIZE, elementIndex % PAGE_SIZE);
    }

    static constexpr std::allocator<T> getElementAllocator() {
      return {};
    }

    static constexpr std::allocator<PageT> getPageAllocator() {
      return {};
    }

    void resizeExact(size_t desiredElements, size_t desiredPages) {
      if(desiredPages > pageCount) {
        //Create new array of page pointers and copy over the old page points. Element locations are unchanged
        auto pageAllocator = getPageAllocator();
        auto elementAllocator = getElementAllocator();
        PageT* newPages = pageAllocator.allocate(desiredPages);
        for(size_t i = 0; i < desiredPages; ++i) {
          //Point at existing pages
          newPages[i] = i < pageCount ? pages[i] : elementAllocator.allocate(PAGE_SIZE);
        }
        //newPages now is pointing at all available pages, assign it over
        std::swap(pages, newPages);
        //Delete the old array of page pointers, not the pages themselves
        if(newPages) {
          pageAllocator.deallocate(newPages, pageCount);
        }
        pageCount = desiredPages;
      }
      else {
        //Destruct elements past the new desired end
        for(size_t i = desiredElements; i < size(); ++i) {
          auto [pi, ii] = getPageAndIndex(i);
          pages[pi][ii].~T();
        }
      }
      elementCount = desiredElements;
    }

    PageT& getPage(size_t p) {
      return pages[p];
    }

    const PageT& getPage(size_t p) const {
      return pages[p];
    }

    PageT* pages{};
    size_t pageCount{};
    size_t elementCount{};
  };
}