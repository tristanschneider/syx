#pragma once

//Default implementation of row uses a vector. Custom implementations can use something else as long as they match the interface the templates use
template<class Element>
struct BasicRow {
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

  void resize(size_t size) {
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

  std::vector<Element> mElements;
  Element mDefaultValue{};
};

//For sharing a value between all elements in a table\
//Keeps track of size for consistency but doesn't mean anything
//Value must be set explicitly with at
template<class Element>
struct SharedRow {
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

  void resize(size_t size) {
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

  Element mValue{};
  size_t mSize = 0;
};

template<class T>
using Row = BasicRow<T>;

//A row whose presence in the table is meaningful bug whose value isn't
struct TagRow : SharedRow<char>{};

template<size_t I, class T>
struct DupleElement {
  T mValue;
};

template<class...>
struct TypeList {};

//Tuple that allows elements of duplicate types
template<class... Elements>
struct Duple {
  template<class X, class Y>
  struct Impl {};
  template<size_t... I, class... E>
  struct Impl<std::index_sequence<I...>, TypeList<E...>> {
    using type = std::tuple<DupleElement<I, E>...>;

    static type create(Elements&&... elements) {
      return { DupleElement<I, E>{ elements }... };
    }
  };

  using ImplT = Impl<std::index_sequence_for<Elements...>, TypeList<Elements...>>;
  using TupleT = typename ImplT::type;

  static Duple create(Elements&&... elements) {
    return { ImplT::create(std::forward<Elements&&>(elements)...) };
  }

  template<size_t I>
  auto& get() {
    return *std::get<I>(mValues).mValue;
  }

  template<size_t I>
  auto& get() const {
    return *std::get<I>(mValues).mValue;
  }

  TupleT mValues;
};

template<class... Args>
Duple<std::decay_t<Args>...> make_duple(Args&&... args) {
  return Duple<std::decay_t<Args>...>::create(std::forward<Args>(args)...);
}

template<class... Rows>
struct Table {
  using ElementRef = Duple<typename Rows::ElementPtr...>;

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