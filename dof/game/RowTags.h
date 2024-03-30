#pragma once

#include "Table.h"

template<class A, class B = void>
struct FloatRow : Row<float> {
  //Types are intended as tags, if they have values it's likely accidental
  static_assert(std::is_empty_v<A>);
  static_assert(std::is_empty_v<B>);
};

namespace Tags {
  //Position in X and Y
  struct Pos{};
  //Rotation is stored as cos(angle) sin(angle), which is the first column of the rotation matrix and can be used to construct the full
  //rotation matrix since it's symmetric
  //They must be initialized to valid values, like cos 1 sin 0
  struct Rot{};
  //Linear velocity in X and Y
  struct LinVel{};
  //Angular velocity in Angle
  struct AngVel{};
  struct Scale{};

  struct GPos{};
  struct GRot{};
  struct GLinVel{};
  struct GAngVel{};

  //Impulses from gameplay to apply. In other words a desired change to LinVel
  //Equivalent to making a velocity stat effect targeting this elemtn of lifetime 1
  struct GLinImpulse{};
  struct GAngImpulse{};

  //The goal coordinates in X and Y that the given fragment wants to go to which will cause it to change to a static gameobject
  struct FragmentGoal{};

  struct X{};
  struct Y{};
  struct Z{};
  struct Angle{};
  struct CosAngle{};
  struct SinAngle{};

  using GLinImpulseXRow = FloatRow<GLinImpulse, X>;
  using GLinImpulseYRow = FloatRow<GLinImpulse, Y>;
  using GAngImpulseRow = FloatRow<GAngImpulse, Angle>;

  using FragmentGoalXRow = FloatRow<FragmentGoal, X>;
  using FragmentGoalYRow = FloatRow<FragmentGoal, Y>;

  using GLinVelXRow = FloatRow<GLinVel, X>;
  using GLinVelYRow = FloatRow<GLinVel, Y>;
  using GAngVelRow = FloatRow<GAngVel, Angle>;
  using GPosXRow = FloatRow<GPos, X>;
  using GPosYRow = FloatRow<GPos, Y>;
  using GPosZRow = FloatRow<GPos, Z>;
  using GRotXRow = FloatRow<GRot, CosAngle>;
  using GRotYRow = FloatRow<GRot, SinAngle>;

  using LinVelXRow = FloatRow<LinVel, X>;
  using LinVelYRow = FloatRow<LinVel, Y>;
  using AngVelRow = FloatRow<AngVel, Angle>;
  using PosXRow = FloatRow<Pos, X>;
  using PosYRow = FloatRow<Pos, Y>;
  using PosZRow = FloatRow<Pos, Z>;
  using RotXRow = FloatRow<Rot, CosAngle>;
  using RotYRow = FloatRow<Rot, SinAngle>;
  using ScaleXRow = FloatRow<Scale, X>;
  using ScaleYRow = FloatRow<Scale, Y>;

  struct TerrainRow : TagRow {};
  struct DynamicPhysicsObjectsTag : TagRow {};
};
