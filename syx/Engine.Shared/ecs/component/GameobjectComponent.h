#pragma once
#include "TypeInfo.h"

struct GameobjectComponent {
};

struct GameobjectInitializedComponent {
};

struct NameTagComponent {
  std::string mName;
};

struct SerializeIDComponent {
  uint32_t mId{};
};

namespace ecx {
  template<>
  struct StaticTypeInfo<NameTagComponent> : StructTypeInfo<
    StaticTypeInfo<NameTagComponent>,
    AutoTypeList<&NameTagComponent::mName>,
    AutoTypeList<>
  > {
    static inline const std::array<std::string, 1> MemberNames = { "name" };
    static inline constexpr const char* SelfName = "NameTag";
  };
}