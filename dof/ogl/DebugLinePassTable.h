#pragma once
#include "Table.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

struct DebugLinePassTable {
  struct Point {
    glm::vec2 pos{};
    glm::vec3 color{};
  };

  struct Points : Row<Point>{};

  using Type = Table<
    Points
  >;
};