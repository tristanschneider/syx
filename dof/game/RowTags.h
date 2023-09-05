#pragma once

#include "Table.h"

template<class, class = void>
struct FloatRow : Row<float> {};

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
  struct Angle{};
  struct CosAngle{};
  struct SinAngle{};
};
