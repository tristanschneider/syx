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

template<class T>
struct TypeName {
  constexpr static std::string_view get() {
    // Hacks to parse the pretty string, likely needs to be implemented per compiler:
    // class std::basic_string_view<char,struct std::char_traits<char> > __cdecl Namespace::TypeName2<class MyClass >::get(void)
    constexpr std::string_view base{ FUNC_NAME };
    constexpr std::string_view beginMarker{ "TypeName<" };
    constexpr std::string_view endMarker{ ">::get" };
    constexpr std::string_view classMarker{ "class " };
    constexpr std::string_view structMarker{ "struct " };
    constexpr size_t rawBegin = base.find(beginMarker) + beginMarker.size();
    // Offset by struct or class if found, otherwise zero
    constexpr size_t offset = base.substr(rawBegin, classMarker.size()) == classMarker ?
      classMarker.size() :
      (base.substr(rawBegin, structMarker.size()) == structMarker ? structMarker.size() : 0);
    constexpr size_t begin = rawBegin + offset;
    constexpr size_t end = base.find(endMarker);
    return base.substr(begin, end - begin);
  }
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