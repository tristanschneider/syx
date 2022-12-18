#pragma once
#include "TypeInfo.h"

struct GameobjectComponent {
};

struct GameobjectInitializedComponent {
};

//Added by a system that wants to destroy the game object. This gives other systems a chance to act before the entity is destroyed
//by GameobjectInitializerSystem
struct DestroyGameobjectComponent {
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