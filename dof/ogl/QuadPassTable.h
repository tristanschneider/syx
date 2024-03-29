#pragma once
#include "Table.h"
#include "Quad.h"
#include "glm/vec4.hpp"

struct QuadPassTable {
  struct QuadUV {
    float uMin = 0;
    float vMin = 0;
    float uMax = 1;
    float vMax = 1;
  };

  struct PosX : Row<float>{};
  struct PosY : Row<float>{};
  struct PosZ : Row<float>{};
  struct RotX : Row<float>{};
  struct RotY : Row<float>{};
  struct ScaleX : Row<float>{};
  struct ScaleY : Row<float>{};
  struct LinVelX : Row<float>{};
  struct LinVelY : Row<float>{};
  struct AngVel : Row<float>{};
  struct Tint : Row<glm::vec4>{};
  struct IsImmobile : SharedRow<bool>{};
  struct UV : Row<QuadUV>{};
  struct Texture : SharedRow<size_t>{};
  struct Pass : SharedRow<QuadPass>{};

  using Type = Table<
    PosX,
    PosY,
    PosZ,
    RotX,
    RotY,
    ScaleX,
    ScaleY,
    LinVelX,
    LinVelY,
    AngVel,
    Tint,
    UV,
    Texture,
    IsImmobile,
    Pass
  >;
};