#pragma once

#include "Table.h"

template<class A, class B = void>
struct FloatRow : Row<float> {
  //Types are intended as tags, if they have values it's likely accidental
  static_assert(std::is_empty_v<A>);
  static_assert(std::is_empty_v<B>);
};

namespace Tags {
  struct GLinVel{};
  struct GAngVel{};

  //Impulses from gameplay to apply. In other words a desired change to LinVel
  //Equivalent to making a velocity stat effect targeting this element of lifetime 1
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
  using GLinImpulseZRow = FloatRow<GLinImpulse, Z>;
  using GAngImpulseRow = FloatRow<GAngImpulse, Angle>;

  using FragmentGoalXRow = FloatRow<FragmentGoal, X>;
  using FragmentGoalYRow = FloatRow<FragmentGoal, Y>;

  using GLinVelXRow = FloatRow<GLinVel, X>;
  using GLinVelYRow = FloatRow<GLinVel, Y>;
  using GAngVelRow = FloatRow<GAngVel, Angle>;
  using GVelXRow = GLinVelXRow;
  using GVelYRow = GLinVelYRow;
  using GVelZRow = FloatRow<GLinVel, Z>;
  using GVelARow = GAngVelRow;

  struct TerrainRow : TagRow {};
  struct InvisibleTerrainRow : TagRow {};
  struct DynamicPhysicsObjectsTag : TagRow {};
  struct DynamicPhysicsObjectsWithMotorTag : TagRow {};
  struct DynamicPhysicsObjectsWithZTag : TagRow {};

  struct ElementNeedsInitRow : BoolRow {};
};

struct IsPlayer : TagRow{};
//These are opposites of each-other, a fragment is either trying to find the goal or found it
struct FragmentGoalFoundTableTag : TagRow {};
struct FragmentSeekingGoalTagRow : TagRow {};
