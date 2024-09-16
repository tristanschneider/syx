#pragma once
#include "TableOperations.h"
#include "TypeId.h"

using DBTypeID = TypeID<struct DBTypeT>;

struct QueryAliasBase {
  DBTypeID type;
  bool isConst{};
};

//Alias for a query whose original type is stored in ID but unkown to the caller,
//which is cast to ResultT
template<class ResultT>
struct QueryAlias : QueryAliasBase {
  using RowT = ResultT;
  static_assert(isRow<RowT>(), "Should only be used for rows");
  static_assert(!isNestedRow<RowT>(), "Nested row is likely unintentional");

  using MutableT = std::remove_const_t<ResultT>;
  using ConstT = const ResultT;
  using MutableCast = MutableT*(*)(void*);
  using ConstCast = ConstT*(*)(void*);

  QueryAlias() = default;

  template<std::convertible_to<ConstT> T>
  QueryAlias(const QueryAlias<T>& rhs)
    : QueryAliasBase{ rhs.type, isConstT() }
    , mutableCast{ rhs.mutableCast }
    , constCast{ rhs.constCast }
  {
  }

  template<std::convertible_to<ResultT> SourceT>
  static QueryAlias create() {
    static_assert(std::is_const_v<SourceT> == std::is_const_v<ResultT>);
    using MutableSourceT = std::remove_const_t<SourceT>;

    struct Caster {
      static MutableT* cast(void* p) {
        return static_cast<MutableT*>(static_cast<MutableSourceT*>(p));
      }
      static ConstT* constCast(void* p) {
        return cast(p);
      }
    };
    QueryAlias result;
    result.mutableCast = &Caster::cast;
    result.constCast = &Caster::constCast;
    result.type = DBTypeID::get<std::decay_t<SourceT>>();
    result.isConst = std::is_const_v<SourceT>;
    return result;
  }

  //Alias of itself
  static QueryAlias create() {
    return create<ResultT>();
  }

  QueryAlias<ConstT> read() const {
    QueryAlias<ConstT> result;
    result.type = type;
    result.mutableCast = mutableCast;
    result.constCast = constCast;
    result.isConst = true;
    return result;
  }

  QueryAlias<MutableT> write() const {
    QueryAlias<MutableT> result;
    result.type = type;
    result.mutableCast = mutableCast;
    result.constCast = constCast;
    result.isConst = false;
    return result;
  }

  static constexpr bool isConstT() {
    return std::is_const_v<ResultT>;
  }

  explicit operator bool() const {
    return mutableCast != nullptr;
  }

  ResultT* cast(void* p) const {
    if constexpr(isConstT()) {
      return constCast(p);
    }
    else {
      return mutableCast(p);
    }
  }

  MutableCast mutableCast{};
  //Hack to enable the convenience of .read
  ConstCast constCast{};
};

using FloatQueryAlias = QueryAlias<Row<float>>;
using ConstFloatQueryAlias = QueryAlias<const Row<float>>;
