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

  template<class SourceT>
  static QueryAlias create() {
    static_assert(std::is_convertible_v<SourceT*, ResultT*>);
    static_assert(std::is_const_v<SourceT> == std::is_const_v<ResultT>);
    struct Caster {
      static ResultT* cast(void* p) {
        return static_cast<ResultT*>(static_cast<SourceT*>(p));
      }
      static const ResultT* constCast(void* p) {
        return cast(p);
      }
    };
    QueryAlias result;
    result.cast = &Caster::cast;
    result.constCast = &Caster::constCast;
    result.type = DBTypeID::get<std::decay_t<SourceT>>();
    result.isConst = std::is_const_v<SourceT>;
    return result;
  }

  //Alias of itself
  static QueryAlias create() {
    return create<ResultT>();
  }

  QueryAlias<const ResultT> read() const {
    QueryAlias<const ResultT> result;
    result.type = type;
    result.cast = constCast;
    result.constCast = constCast;
    result.isConst = true;
    return result;
  }

  ResultT*(*cast)(void*){};
  //Hack to enable the convenience of .read
  const ResultT*(*constCast)(void*){};
};

using FloatQueryAlias = QueryAlias<Row<float>>;
using ConstFloatQueryAlias = QueryAlias<const Row<float>>;
