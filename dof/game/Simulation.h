#pragma once

#include "Database.h"
#include "Table.h"

template<class, class = void>
struct FloatRow : Row<float> {};

namespace Tags {
  struct Pos{};
  struct Rot{};

  struct X{};
  struct Y{};
  struct Angle{};
};

using GameObjectTable = Table<
  FloatRow<Tags::Pos, Tags::X>,
  FloatRow<Tags::Pos, Tags::Y>,
  FloatRow<Tags::Rot, Tags::Angle>
>;

using GameDatabase = Database<
  GameObjectTable
>;

struct Simulation {
  static void update(GameDatabase& db);
};