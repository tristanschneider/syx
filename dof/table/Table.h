#pragma once

#include "IRow.h"

//Default implementation of row uses a vector. Custom implementations can use something else as long as they match the interface the templates use
template<class Element>
struct BasicRow : IRow {
  using ElementT = Element;
  using ElementPtr = Element*;
  using IsBasicRow = std::true_type;

  using IteratorT = typename std::vector<Element>::iterator;
  using ConstIteratorT = typename std::vector<Element>::const_iterator;

  size_t size() const {
    return mElements.size();
  }

  Element& at(size_t i) {
    return mElements.at(i);
  }

  const Element& at(size_t i) const {
    return mElements.at(i);
  }

  template<class... Args>
  Element& emplaceBack(Args&&... args) {
    if constexpr(sizeof...(args) > 0) {
      return mElements.emplace_back(std::forward<Args>(args)...);
    }
    else {
      if constexpr(std::is_copy_constructible_v<ElementT>) {
        return mElements.emplace_back(mDefaultValue);
      }
      else {
        return mElements.emplace_back();
      }
    }
  }

  void resize(size_t, size_t size) final {
    if constexpr(std::is_copy_constructible_v<Element>) {
      mElements.resize(size, mDefaultValue);
    }
    else {
      mElements.resize(size);
    }
  }

  IteratorT begin() {
    return mElements.begin();
  }

  IteratorT end() {
    return mElements.end();
  }

  ConstIteratorT begin() const {
    return mElements.begin();
  }

  ConstIteratorT end() const {
    return mElements.end();
  }

  Element* data() {
    return mElements.data();
  }

  const Element* data() const {
    return mElements.data();
  }

  void setDefaultValue(Element value) {
    mDefaultValue = value;
  }

  RowBuffer getElements() final {
    return { mElements.data() };
  }

  ConstRowBuffer getElements() const final {
    return { mElements.data() };
  }

  void swapRemove(size_t b, size_t e, size_t) final {
    for(size_t i = b; i < e; ++i) {
      mElements[i] = std::move(mElements.back());
      mElements.pop_back();
    }
  }

  void migrateElements(const MigrateArgs& args) final {
    if(BasicRow<Element>* cast = static_cast<BasicRow<Element>*>(args.fromRow)) {
      for(size_t i = 0; i < args.count; ++i) {
        at(args.toIndex + i) = std::move(cast->at(args.fromIndex + i));
      }
    }
    else {
      for(size_t i = 0; i < args.count; ++i) {
        if constexpr(std::is_copy_constructible_v<ElementT>) {
          at(args.toIndex + i) = mDefaultValue;
        }
        else {
          at(args.toIndex + i) = {};
        }
      }
    }
  }

  void popBack() {
    mElements.pop_back();
  }

private:
  std::vector<Element> mElements;
  Element mDefaultValue{};
};

//For sharing a value between all elements in a table
//Keeps track of size for consistency but doesn't mean anything
//Value must be set explicitly with at
template<class Element>
struct SharedRow : IRow {
  using ElementT = Element;
  using ElementPtr = Element*;
  using NoOpIterator = size_t;
  using IteratorT = NoOpIterator;
  using ConstIteratorT = NoOpIterator;

  using IsSharedRow = std::true_type;

  size_t size() const {
    return mSize;
  }

  Element& at(size_t = 0) {
    return mValue;
  }

  const Element& at(size_t = 0) const {
    return mValue;
  }

  template<class... Args>
  Element& emplaceBack(Args&&...) {
    ++mSize;
    return mValue;
  }

  void resize(size_t, size_t size) final {
    mSize = size;
  }

  NoOpIterator begin() {
    return {};
  }

  NoOpIterator end() {
    return {};
  }

  void setDefaultValue(Element value) {
    mValue = value;
  }

  RowBuffer getElements() final {
    return { &mValue };
  }

  ConstRowBuffer getElements() const final {
    return { &mValue };
  }

  void swapRemove(size_t b, size_t e, size_t) final {
    //Since element is a singleton it doesn't change but the tracked size does to match the other rows in the table
    mSize -= (e - b);
  }

  //Not relevant to update the shared value for an entire row when one element moves
  void migrateElements(const MigrateArgs&) final {
  }

  Element mValue{};
  size_t mSize = 0;
};

template<class T, class Enabled = void>
struct IsSharedRowT : std::false_type {};
template<class T>
struct IsSharedRowT<T, std::enable_if_t<std::is_same_v<std::true_type, typename T::IsSharedRow>>> : std::true_type {};

template<class T, class Enabled = void>
struct IsBasicRowT : std::false_type {};
template<class T>
struct IsBasicRowT<T, std::enable_if_t<std::is_same_v<std::true_type, typename T::IsBasicRow>>> : std::true_type {};

template<class T>
concept IsRow = std::is_base_of_v<IRow, T>;
static_assert(IsRow<BasicRow<int>>);
static_assert(IsRow<SharedRow<int>>);

template<class T>
constexpr bool isRow() {
  return IsRow<T>;
}

template<class T>
constexpr bool isNestedRow() {
  if constexpr(isRow<T>()) {
    return isRow<typename T::ElementT>();
  }
  else {
    return false;
  }
}


template<class T>
using Row = BasicRow<T>;

//A row whose presence in the table is meaningful bug whose value isn't
struct TagRow : SharedRow<char>{};
//Alias because current bool specialization of vector messes up row template wrappers but this could be a nice optimization in the future
struct BoolRow : Row<uint8_t>{};

template<class... Rows>
struct Table {
  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) {
    (visitor(std::get<Rows>(mRows), args...), ...);
  }

  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) const {
    (visitor(std::get<Rows>(mRows), args...), ...);
  }

  std::tuple<Rows...> mRows;
};

// Table can choose to expose a name for debugging purposes
struct TableName {
  std::string name;
};
struct TableNameRow : SharedRow<TableName> {
  static constexpr std::string_view KEY = "TableName";
};