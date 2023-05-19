#pragma once
#include "Precompile.h"

#include <cassert>
#include "curve/CurveMath.h"

#include "unity.h"

namespace CurveMath {
  void backEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::backEaseIn(inT, outT, count);
  }

  void backEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::backEaseInOut(inT, outT, count);
  }

  void backEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::backEaseOut(inT, outT, count);
  }

  void bounceEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::bounceEaseIn(inT, outT, count);
  }

  void bounceEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::bounceEaseInOut(inT, outT, count);
  }

  void bounceEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::bounceEaseOut(inT, outT, count);
  }

  void circularEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::circularEaseIn(inT, outT, count);
  }

  void circularEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::circularEaseInOut(inT, outT, count);
  }

  void circularEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::circularEaseOut(inT, outT, count);
  }

  void cubicEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::cubicEaseIn(inT, outT, count);
  }

  void cubicEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::cubicEaseInOut(inT, outT, count);
  }

  void cubicEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::cubicEaseOut(inT, outT, count);
  }

  void elasticEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::elasticEaseIn(inT, outT, count);
  }

  void elasticEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::elasticEaseInOut(inT, outT, count);
  }

  void elasticEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::elasticEaseOut(inT, outT, count);
  }

  void exponentialEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::exponentialEaseIn(inT, outT, count);
  }

  void exponentialEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::exponentialEaseInOut(inT, outT, count);
  }

  void exponentialEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::exponentialEaseOut(inT, outT, count);
  }

  void linearInterpolation(const float * inT, float * outT, int32_t count) {
    ispc::linearInterpolation(inT, outT, count);
  }

  void quadraticEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::quadraticEaseIn(inT, outT, count);
  }

  void quadraticEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::quadraticEaseInOut(inT, outT, count);
  }

  void quadraticEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::quadraticEaseOut(inT, outT, count);
  }

  void quarticEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::quarticEaseIn(inT, outT, count);
  }

  void quarticEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::quarticEaseInOut(inT, outT, count);
  }

  void quarticEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::quarticEaseOut(inT, outT, count);
  }

  void quinticEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::quinticEaseIn(inT, outT, count);
  }

  void quinticEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::quinticEaseInOut(inT, outT, count);
  }

  void quinticEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::quarticEaseOut(inT, outT, count);
  }

  void sineEaseIn(const float * inT, float * outT, int32_t count) {
    ispc::sineEaseIn(inT, outT, count);
  }

  void sineEaseInOut(const float * inT, float * outT, int32_t count) {
    ispc::sineEaseInOut(inT, outT, count);
  }

  void sineEaseOut(const float * inT, float * outT, int32_t count) {
    ispc::sineEaseOut(inT, outT, count);
  }

  CurveFunction getFunction(CurveType type) {
    static std::array functions {
      CurveFunction{ &backEaseIn, "BackEaseIn" },
      CurveFunction{ &backEaseInOut, "BackEaseInOut" },
      CurveFunction{ &backEaseOut, "BackEaseOut" },
      CurveFunction{ &bounceEaseIn, "BounceEaseIn" },
      CurveFunction{ &bounceEaseInOut, "BounceEaseInOut" },
      CurveFunction{ &bounceEaseOut, "BounceEaseOut" },
      CurveFunction{ &circularEaseIn, "CircularEaseIn" },
      CurveFunction{ &circularEaseInOut, "CircularEaseInOut" },
      CurveFunction{ &circularEaseOut, "CircularEaseOut" },
      CurveFunction{ &cubicEaseIn, "CubicEaseIn" },
      CurveFunction{ &cubicEaseInOut, "CubicEaseInOut" },
      CurveFunction{ &cubicEaseOut, "CubicEaseOut" },
      CurveFunction{ &elasticEaseIn, "ElasticEaseIn" },
      CurveFunction{ &elasticEaseInOut, "ElasticEaseInOut" },
      CurveFunction{ &elasticEaseOut, "ElasticEaseOut" },
      CurveFunction{ &exponentialEaseIn, "ExponentialEaseIn" },
      CurveFunction{ &exponentialEaseInOut, "ExponentialEaseInOut" },
      CurveFunction{ &exponentialEaseOut, "ExponentialEaseOut" },
      CurveFunction{ &linearInterpolation, "LinearInterpolation" },
      CurveFunction{ &quadraticEaseIn, "QuadraticEaseIn" },
      CurveFunction{ &quadraticEaseInOut, "QuadraticEaseInOut" },
      CurveFunction{ &quadraticEaseOut, "QuadraticEaseOut" },
      CurveFunction{ &quarticEaseIn, "QuarticEaseIn" },
      CurveFunction{ &quarticEaseInOut, "QuarticEaseInOut" },
      CurveFunction{ &quarticEaseOut, "QuarticEaseOut" },
      CurveFunction{ &quinticEaseIn, "QuinticEaseIn" },
      CurveFunction{ &quinticEaseInOut, "QuinticEaseInOut" },
      CurveFunction{ &quinticEaseOut, "QuinticEaseOut" },
      CurveFunction{ &sineEaseIn, "SineEaseIn" },
      CurveFunction{ &sineEaseInOut, "SineEaseInOut" },
      CurveFunction{ &sineEaseOut, "SineEaseOut" }
    };
    const size_t index = static_cast<size_t>(type);
    assert(index < functions.size());
    return index < functions.size() ? functions[index] : CurveFunction{};
  }
}