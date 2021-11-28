#pragma once

namespace ecx {
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

    bool operator<(const typeId_t<Category>& rhs) const {
      return mId < rhs.mId;
    }

    template<class T>
    static constexpr typeId_t<Category> get() {
      static typeId_t<Category> t(idgen++);
      return t;
    }

    inline static size_t idgen = 0;
    size_t mId = std::numeric_limits<size_t>::max();
  };

  struct DefaultTypeCategory {
  };
  //Get unique id for type, generated sequentially. Category can be used to subcategorize the ids,
  //keeping the ranges of relevant values small. For example, use a base class to categorize the derived types:
  //typeId<EventA, Event>(), typeId<EventB, Event>(), so all events can be in a small sequential range
  template<typename T, typename Category = DefaultTypeCategory>
  typeId_t<Category> typeId() {
    return typeId_t<Category>::get<T>();
  }
}