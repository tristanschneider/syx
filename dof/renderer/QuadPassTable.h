#pragma once
#include "Table.h"
#include "Quad.h"
#include "glm/vec4.hpp"

namespace QuadPassTable {
  //TODO: from shader description
  struct Transform{};
  struct UVOffset{};

  struct TransformRow : Row<Transform>{};
  struct UVOffsetRow : Row<UVOffset>{};
  struct TintRow : Row<glm::vec4>{};
  struct IsImmobileRow : SharedRow<bool>{};
  struct PassRow : SharedRow<QuadPass>{};
  struct TextureIDRow : SharedRow<size_t>{};

  using Type = Table<
    TransformRow,
    UVOffsetRow,
    TintRow,
    IsImmobileRow,
    PassRow,
    TextureIDRow
  >;
};