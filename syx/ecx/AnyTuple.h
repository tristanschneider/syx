#pragma once

#include "AnyType.h"
#include "TypeId.h"

namespace ecx {
  struct DefaultTupleCategory {};

  //Tuple-like storage but for any types
  template<class Category = DefaultTupleCategory>
  class AnyTuple {
  public:
    using TypeIdT = typeId_t<Category>;

    AnyTuple() = default;
    AnyTuple(const AnyTuple&) = default;
    AnyTuple(AnyTuple&&) = default;

    AnyTuple& operator=(const AnyTuple&) = default;
    AnyTuple& operator=(AnyTuple&&) = default;

    template<class T>
    T& getOrCreate() {
      return emplace<T>();
    }

    template<class T, class... Args>
    T& emplace(Args&&... args) {
      const size_t index(_getId<T>());
      if(mValues.size() <= index) {
        mValues.resize(index + 1);
      }
      AnyType& result = mValues[index];
      if(!result) {
        result.emplace<T>(std::forward<Args>(args)...);
      }
      return result.get<T>();
    }

    template<class... Args>
    void reserve() {
      //Static so runtime cost of computing max is once per template instantiation
      static size_t maxTypeId = std::max({ static_cast<size_t>(ecx::typeId<Args, Category>())... });
      //+1 because the type id is the index
      reserve(maxTypeId + 1);
    }

    void reserve(size_t size) {
      mValues.reserve(size);
    }

  private:
    template<class T>
    static TypeIdT _getId() {
      return typeId<std::decay_t<T>, Category>();
    }

    std::vector<AnyType> mValues;
  };
}