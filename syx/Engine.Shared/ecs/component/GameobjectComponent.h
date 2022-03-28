#pragma once
#include "TypeInfo.h"

struct GameobjectComponent {
};

struct NameTagComponent {
  std::string mName;
};

namespace ecx {
  template<>
  struct StaticTypeInfo<NameTagComponent> : StructTypeInfo<
    StaticTypeInfo<NameTagComponent>,
    AutoTypeList<&NameTagComponent::mName>,
    AutoTypeList<>
  > {
    static inline const std::array<std::string, 1> MemberNames = { "name" };
    static inline const std::string SelfName = "NameTag";
  };
}