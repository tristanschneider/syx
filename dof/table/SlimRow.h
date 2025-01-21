#pragma once

#include "IRow.h"
#include "generics/ValueConstant.h"

template<class T, class V>
concept ValueGen = requires {
  { T{}() } -> std::convertible_to<V>;
};

//Row implementation that is as small as possible
//It doesn't hold its own size, the table knows it
//Default value is baked in as a template argument
//This means the row is the size of a pointer, plus the unfortunate vptr
//Since it doesn't know its capacity it doesn't resize down, only up, as otherwise adding or removing
//single elements would be very expensive. This also means removed elements don't have a destructor
//called on them until a resize or the table is destroyed. They are moved from though.
//As such, this is best suited for plain types that don't imply retaining something with their lifetime
template<class Element, ValueGen<Element> Gen = gnx::value_constant<Element>>
class SlimRow : public IRow {
public:
  using ElementT = Element;
  using ElementPtr = Element*;
  using SelfT = SlimRow<Element, Gen>;

  SlimRow() = default;

  ~SlimRow() {
    reset();
  };

  SlimRow(SlimRow&& rhs)
    : values{ rhs.values }
  {
    rhs.release();
  }

  SlimRow& operator=(SlimRow&& rhs) {
    if(&rhs != this) {
      SlimRow temp{ std::move(rhs) };
      temp.swap(*this);
    }
    return *this;
  }

  void swap(SlimRow& rhs) {
    std::swap(values, rhs.values);
  }

  class ConstIt {
  public:
    using value_type = ElementT;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    ConstIt(const ElementT* p)
      : value{ p }
    {
    }

    ConstIt& operator++() {
      ++value;
      return *this;
    }

    ConstIt operator++(int) {
      ConstIt tmp{ *this };
      ++*this;
      return tmp;
    }

    ConstIt operator+(size_t i) const {
      return { value + i };
    }

    const value_type& operator*() const {
      return *value;
    }

    const value_type* operator->() const {
      return value;
    }

    std::ptrdiff_t operator-(const ConstIt& rhs) const {
      return value - rhs.value;
    }

    auto operator<=>(const ConstIt&) const = default;

  protected:
    const ElementT* value{};
  };

  class It : public ConstIt {
  public:
    using value_type = ElementT;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;

    It(ElementT* p)
      : ConstIt{ p }
    {
    }

    value_type& operator*() const {
      return const_cast<value_type&>(*(this->value));
    }

    value_type* operator->() const {
      return &**this;
    }

    It operator+(size_t i) const {
      return { &**this + i };
    }
  };

  using IteratorT = It;
  using ConstIteratorT = ConstIt;

  Element& at(size_t i) {
    return data()[i];
  }

  const Element& at(size_t i) const {
    return data()[i];
  }

  void resize(size_t oldSize, size_t newSize) final {
    Gen gen;
    //If the current size is greater, leave it as-is to avoid needing to reallocate
    //Reset them to default value so they don't have garbage when resizing back up
    //Elements are not fully destroyed as that would require tracking capacity
    if(oldSize >= newSize && data()) {
      for(size_t i = newSize; i < oldSize; ++i) {
        at(i) = gen();
      }
      return;
    }

    SelfT old;
    old.swap(*this);

    values = new Element[newSize];

    //Copy over old values or defaults if there were none
    if(Element* oldValues = old.data()) {
      for(size_t i = 0; i < std::min(oldSize, newSize); ++i) {
        at(i) = std::move(old.at(i));
      }
    }
    else {
      for(size_t i = 0; i < oldSize; ++i) {
        at(i) = gen();
      }
    }

    //Default initialize new values
    for(size_t i = oldSize; i < newSize; ++i) {
      at(i) = gen();
    }
  }

  IteratorT begin() {
    return { values };
  }

  IteratorT end(size_t tableSize) {
    return { values + tableSize };
  }

  ConstIteratorT begin() const {
    return { values };
  }

  ConstIteratorT end(size_t tableSize) const {
    return { values + tableSize };
  }

  Element* data() {
    return values;
  }

  const Element* data() const {
    return values;
  }

  RowBuffer getElements() final {
    return { data() };
  }

  ConstRowBuffer getElements() const final {
    return { data() };
  }

  void swapRemove(size_t begin, size_t end, size_t tableSize) final {
    //Move end elements to remove location
    //This means elements at the end won't be destroyed until the next resize, but they have been moved-from
    //It shouldn't matter because the table won't access them
    for(size_t i = begin; i < end; ++i) {
      at(i) = std::move(at(--tableSize));
    }
  }

  void migrateElements(const MigrateArgs& args) final {
    if(SelfT* cast = static_cast<SelfT*>(args.fromRow)) {
      for(size_t i = 0; i < args.count; ++i) {
        at(args.toIndex + i) = std::move(cast->at(args.fromIndex + i));
      }
    }
    else {
      Gen gen;
      for(size_t i = 0; i < args.count; ++i) {
        at(args.toIndex + i) = gen();
      }
    }
  }

private:
  void reset() {
    if(values) {
      delete [] values;
      values = nullptr;
    }
  }

  void release() {
    values = nullptr;
  }

  Element* values{};
};
