#pragma once

//Put this in the public section of your class to make it useable as a type category
#define DECLARE_TYPE_CATEGORY static size_t _genTypeId() { static size_t id = 0; return id++; }

struct DefaultTypeCategory {
  DECLARE_TYPE_CATEGORY
};

//Get unique id for type, generated sequentially. Category can be used to subcategorize the ids,
//keeping the ranges of relevant values small. For example, use a base class to categorize the derived types:
//typeId<EventA, Event>(), typeId<EventB, Event>(), so all events can be in a small sequential range
template<typename T, typename Category = DefaultTypeCategory>
size_t typeId() {
  static size_t id = Category::_genTypeId();
  return id;
}

//Map of type id to T. Since ids are generated consecutively and there shouldn't be many of them,
//use a vector and index by id. If the map only containes certain types, they could be generated with a unique IdGen
//so that this container can contain only the relevant types
template<typename T>
class TypeMap {
public:
  template<typename K>
  const T& get() const {
    return get(typeId<K>());
  }

  template<typename K, typename Category = DefaultTypeCategory>
  T* get() {
    return get(typeId<K, Category>());
  }

  const T* get(size_t key) const {
    return key < mValues.size() ? &mValues[key] : nullptr;
  }

  T* get(size_t key) {
    return key < mValues.size() ? &mValues[key] : nullptr;
  }

  template<typename K, typename Category = DefaultTypeCategory>
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

private:
  void _growToFit(size_t key) {
    if(key >= mValues.size())
      mValues.resize(key + 1);
  }

  std::vector<T> mValues;
};