#pragma once

#ifdef _MSC_VER
#define FUNC_NAME __FUNCSIG__
#elif
#define FNC_NAME __PRETTY_FUNCTION__
#else
static_assert(false, "implement this");
#endif

template<class Category>
struct TypeID {
  template<class T>
  static TypeID get() {
    static TypeID result{ std::hash<std::string>()(FUNC_NAME) };
    return result;
  }

  bool operator==(const TypeID& rhs) const {
    return value == rhs.value;
  }

  bool operator!=(const TypeID& rhs) const {
    return !(*this == rhs);
  }

  bool operator<(const TypeID& rhs) const {
    return value < rhs.value;
  }

  size_t value{};
};

namespace std {
  template<class Category>
  struct hash<TypeID<Category>> {
    size_t operator()(const TypeID<Category>& t) const {
      //Is already a hash so doesn't need to be rehashed
      return t.value;
    }
  };
}