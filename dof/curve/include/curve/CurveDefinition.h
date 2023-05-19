#pragma once

#include "curve/CurveMath.h"

//For convenience all of the parameters are bundled here, although any of them could be supplied
//externally to allow them to differ per instances of the ccurve
struct CurveParameters {
  //Scale applied to the computed time value before offset
  std::optional<float> scale;
  //Offset applied after scaling
  std::optional<float> offset;
  //How long it should take for time to go from 0 to 1
  std::optional<float> duration;
};

//This represents everything needed to solve a curve that is common across all instances of the same curve
//The intention is for it to have more than what CurveMath does by allowing composition of curves
//At the moment it's just the one
struct CurveDefinition {
  CurveParameters params;
  CurveMath::CurveFunction function;
};