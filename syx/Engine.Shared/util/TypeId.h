#pragma once

//Put this in the public section of your class to make it useable as a type category
#define DECLARE_TYPE_CATEGORY /*static size_t _genTypeId() { static size_t id = 0; return id++; }*/

struct DefaultTypeCategory {
};

template<class Category>
struct typeId_t {
  typeId_t() = default;

  constexpr explicit typeId_t(size_t id)
    : mId(id) {
  }

  constexpr typeId_t(const typeId_t&) = default;
  typeId_t& operator=(const typeId_t&) = default;

  bool operator==(const typeId_t<Category>& rhs) const {
    return mId == rhs.mId;
  }

  bool operator!=(const typeId_t<Category>& rhs) const {
    return mId != rhs.mId;
  }

  operator size_t() const {
    return mId;
  }

  template<class T>
  static constexpr typeId_t<Category> get() {
    static typeId_t<Category> t(idgen++);
    return t;
  }

  inline static size_t idgen = 0;
  size_t mId = std::numeric_limits<size_t>::max();
};

//Get unique id for type, generated sequentially. Category can be used to subcategorize the ids,
//keeping the ranges of relevant values small. For example, use a base class to categorize the derived types:
//typeId<EventA, Event>(), typeId<EventB, Event>(), so all events can be in a small sequential range
template<typename T, typename Category = DefaultTypeCategory>
typeId_t<Category> typeId() {
  return typeId_t<Category>::get<T>();
}

//Map of type id to T. Since ids are generated consecutively and there shouldn't be many of them,
//use a vector and index by id. If the map only containes certain types, they could be generated with a unique IdGen
//so that this container can contain only the relevant types
template<typename T, typename Category = DefaultTypeCategory>
class TypeMap {
public:
  //Forward iterator, requires that default value has implicit conversion to false
  template<typename Value, typename Container>
  class Iterator {
  public:
    Iterator(size_t index, Container* container)
      : mIndex(index)
      , mContainer(container) {
    }

    Iterator<Value, Container> operator++(int) {
      Iterator<Value, Container> result(mIndex, mContainer);
      _next();
      return result;
    }

    Iterator<Value, Container>& operator++() {
      _next();
      return *this;
    }

    Value& operator*() {
      return (*mContainer)[mIndex];
    }

    bool operator==(const Iterator<Value, Container>& rhs) {
      return mIndex == rhs.mIndex && mContainer == rhs.mContainer;
    }

    bool operator!=(const Iterator<Value, Container>& rhs) {
      return !(*this == rhs);
    }

  private:
    void _next() {
      ++mIndex;
      //Keep going until a non-default value is found
      while(mIndex < mContainer->size() && !(*mContainer)[mIndex])
        ++mIndex;
    }

    size_t mIndex;
    Container* mContainer;
  };

  using Iterator_t = Iterator<T, std::vector<T>>;
  using Const_Iterator_t = Iterator<const T, const std::vector<T>>;

  template<typename K>
  const T* get() const {
    return get(typeId<K, Category>());
  }

  template<typename K>
  T* get() {
    return get(typeId<K, Category>());
  }

  const T* get(size_t key) const {
    return key < mValues.size() ? &mValues[key] : nullptr;
  }

  T* get(size_t key) {
    return key < mValues.size() ? &mValues[key] : nullptr;
  }

  template<typename K>
  void set(T&& value) {
    set(typeId<K, Category>(), std::move(value));
  }

  void set(size_t key, T&& value) {
    _growToFit(key);
    mValues[key] = std::move(value);
  }

  void clear() {
    mValues.clear();
  }

  const T& operator[](size_t key) const {
    return mValues[key];
  }

  T& operator[](size_t key) {
    _growToFit(key);
    return mValues[key];
  }

  Iterator_t begin() {
    Iterator_t result(0, &mValues);
    if(!mValues.empty() && !*result)
      ++result;
    return result;
  }

  Const_Iterator_t begin() const {
    Const_Iterator_t result(0, &mValues);
    if(!mValues.empty() && !*result)
      ++result;
    return result;
  }

  Iterator_t end() {
    return Iterator_t(mValues.size(), &mValues);
  }

  Const_Iterator_t end() const {
    return Const_Iterator_t(mValues.size(), &mValues);
  }

private:
  void _growToFit(size_t key) {
    if(key >= mValues.size())
      mValues.resize(key + 1);
  }

  std::vector<T> mValues;
};