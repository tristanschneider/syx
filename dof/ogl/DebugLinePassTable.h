#pragma once
#include "Table.h"

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

struct DebugLinePassTable {
  struct Point {
    glm::vec2 pos{};
    glm::vec3 color{};
  };

  struct Text {
    glm::vec2 pos{};
    std::string text;
  };

  struct Points : Row<Point>{};
  struct Texts : Row<Text>{};

  using PointsTable = Table<
    Points
  >;
  using TextTable = Table<
    Texts
  >;
};