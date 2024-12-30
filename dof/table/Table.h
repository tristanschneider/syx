#pragma once

#include "IRow.h"

//Default implementation of row uses a vector. Custom implementations can use something else as long as they match the interface the templates use
template<class Element>
struct BasicRow : IRow {
  using ElementT = Element;
  using ElementPtr = Element*;
  using IsBasicRow = std::true_type;

  using IteratorT = typename std::vector<Element>::iterator;

  size_t size() const {
    return mElements.size();
  }

  Element& at(size_t i) {
    return mElements.at(i);
  }

  const Element& at(size_t i) const {
    return mElements.at(i);
  }

  void swap(size_t a, size_t b) {
    std::swap(at(a), at(b));
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

  template<class T, class... Args>
  Element& insert(const T& atIt, Args&&... args) {
    return *mElements.insert(atIt, std::forward<Args>(args)...);
  }

  void erase(const IteratorT& it) {
    mElements.erase(it);
  }

  void resize(size_t size) final {
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

  void swapRemove(size_t i) final {
    mElements[i] = std::move(mElements.back());
    mElements.pop_back();
  }

  size_t migrateElements(size_t fromIndex, IRow* fromRow, size_t count) final {
    size_t result = size();
    for(size_t i = 0; i < count; ++i) {
      if(BasicRow<Element>* cast = static_cast<BasicRow<Element>*>(fromRow)) {
        emplaceBack(std::move(cast->mElements[fromIndex + i]));
      }
      else {
        emplaceBack();
      }
    }
    return result;
  }

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

  //Has no meaning because there is only one
  void swap(size_t, size_t) {
  }

  template<class... Args>
  Element& emplaceBack(Args&&...) {
    ++mSize;
    return mValue;
  }

  void resize(size_t size) final {
    mSize = size;
  }

  template<class... Args>
  Element& insert(NoOpIterator, Args&&...) {
    ++mSize;
    return mValue;
  }

  void erase(NoOpIterator) {
    --mSize;
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

  void swapRemove(size_t) final {
    //Since element is a singleton it doesn't change but the tracked size does to match the other rows in the table
    --mSize;
  }

  //Not relevant to update the shared value for an entire row when one element moves
  size_t migrateElements(size_t, IRow*, size_t count) final {
    size_t result = size();
    mSize += count;
    return result;
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
constexpr bool isRow() {
  return IsSharedRowT<T>::value || IsBasicRowT<T>::value;
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
concept IsRow = requires(T t) {
  typename T::ElementT;
  typename T::ElementPtr;
  t.at(0);
  t.size();
  t.swap(0, 1);
};
static_assert(IsRow<BasicRow<int>>);
static_assert(IsRow<SharedRow<int>>);

template<class T>
using Row = BasicRow<T>;

//A row whose presence in the table is meaningful bug whose value isn't
struct TagRow : SharedRow<char>{};
//Alias because currentl bool specialization of vector messes up row template wrappers but this could be a nice optimization in the future
struct BoolRow : Row<uint8_t>{};

template<class... Rows>
struct Table {
  template<class T>
  constexpr static bool HasRow = (std::is_same_v<std::decay_t<T>, Rows> || ...);

  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) {
    (visitor(std::get<Rows>(mRows), args...), ...);
  }

  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) const {
    (visitor(std::get<Rows>(mRows), args...), ...);
  }

  //Call a multi-argument visitor with all rows
  template<class Visitor, class... Args>
  constexpr auto visitAll(const Visitor& visitor, Args&&... args) {
    return visitor(args..., std::get<Rows>(mRows)...);
  }

  template<class Visitor, class... Args>
  constexpr auto visitAll(const Visitor& visitor, Args&&... args) const {
    return visitor(args..., std::get<Rows>(mRows)...);
  }

  std::tuple<Rows...> mRows;
};