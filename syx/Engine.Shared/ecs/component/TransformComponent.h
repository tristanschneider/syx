#pragma once

#include "SyxMat4.h"
#include "TypeInfo.h"

struct TransformComponent {
  Syx::Mat4 mValue = Syx::Mat4::identity();
};

namespace ecx {
  template<>
  struct StaticTypeInfo<TransformComponent> : StructTypeInfo<StaticTypeInfo<TransformComponent>,
    AutoTypeList<&TransformComponent::mValue>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "value" };
    static inline const std::string SelfName = "Transform";
  };
}