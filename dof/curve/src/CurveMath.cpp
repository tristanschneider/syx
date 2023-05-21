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

  void one(const float * inT, float * outT, int32_t count) {
    ispc::one(inT, outT, count);
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

  void zero(const float * inT, float * outT, int32_t count) {
    ispc::zero(inT, outT, count);
  }

  const auto& getFunctions() {
    static std::array functions {
      CurveFunction{ &backEaseIn, "BackEaseIn", CurveType::BackEaseIn },
      CurveFunction{ &backEaseInOut, "BackEaseInOut", CurveType::BackEaseInOut },
      CurveFunction{ &backEaseOut, "BackEaseOut", CurveType::BackEaseOut },
      CurveFunction{ &bounceEaseIn, "BounceEaseIn", CurveType::BounceEaseIn },
      CurveFunction{ &bounceEaseInOut, "BounceEaseInOut", CurveType::BounceEaseInOut },
      CurveFunction{ &bounceEaseOut, "BounceEaseOut", CurveType::BounceEaseOut },
      CurveFunction{ &circularEaseIn, "CircularEaseIn", CurveType::CircularEaseIn },
      CurveFunction{ &circularEaseInOut, "CircularEaseInOut", CurveType::CircularEaseInOut },
      CurveFunction{ &circularEaseOut, "CircularEaseOut", CurveType::CircularEaseOut },
      CurveFunction{ &cubicEaseIn, "CubicEaseIn", CurveType::CubicEaseIn },
      CurveFunction{ &cubicEaseInOut, "CubicEaseInOut", CurveType::CubicEaseInOut },
      CurveFunction{ &cubicEaseOut, "CubicEaseOut", CurveType::CubicEaseOut },
      CurveFunction{ &elasticEaseIn, "ElasticEaseIn", CurveType::ElasticEaseIn },
      CurveFunction{ &elasticEaseInOut, "ElasticEaseInOut", CurveType::ElasticEaseInOut },
      CurveFunction{ &elasticEaseOut, "ElasticEaseOut", CurveType::ElasticEaseOut },
      CurveFunction{ &exponentialEaseIn, "ExponentialEaseIn", CurveType::ExponentialEaseIn },
      CurveFunction{ &exponentialEaseInOut, "ExponentialEaseInOut", CurveType::ExponentialEaseInOut },
      CurveFunction{ &exponentialEaseOut, "ExponentialEaseOut", CurveType::ExponentialEaseOut },
      CurveFunction{ &linearInterpolation, "LinearInterpolation", CurveType::LinearInterpolation },
      CurveFunction{ &one, "One", CurveType::One },
      CurveFunction{ &quadraticEaseIn, "QuadraticEaseIn", CurveType::QuadraticEaseIn },
      CurveFunction{ &quadraticEaseInOut, "QuadraticEaseInOut", CurveType::QuadraticEaseInOut },
      CurveFunction{ &quadraticEaseOut, "QuadraticEaseOut", CurveType::QuadraticEaseOut },
      CurveFunction{ &quarticEaseIn, "QuarticEaseIn", CurveType::QuarticEaseIn },
      CurveFunction{ &quarticEaseInOut, "QuarticEaseInOut", CurveType::QuarticEaseInOut },
      CurveFunction{ &quarticEaseOut, "QuarticEaseOut", CurveType::QuarticEaseOut },
      CurveFunction{ &quinticEaseIn, "QuinticEaseIn", CurveType::QuinticEaseIn },
      CurveFunction{ &quinticEaseInOut, "QuinticEaseInOut", CurveType::QuinticEaseInOut },
      CurveFunction{ &quinticEaseOut, "QuinticEaseOut", CurveType::QuinticEaseOut },
      CurveFunction{ &sineEaseIn, "SineEaseIn", CurveType::SineEaseIn },
      CurveFunction{ &sineEaseInOut, "SineEaseInOut", CurveType::SineEaseInOut },
      CurveFunction{ &sineEaseOut, "SineEaseOut", CurveType::SineEaseOut },
      CurveFunction{ &zero, "Zero", CurveType::Zero },
    };
    return functions;
  }

  CurveFunction tryGetFunction(CurveType type) {
    assert(static_cast<int>(type) < static_cast<int>(CurveType::Count));
    return getFunction(type);
  }

  CurveFunction getFunction(CurveType type) {
    const auto& functions = getFunctions();
    const size_t index = static_cast<size_t>(type);
    assert(index < functions.size());
    return index < functions.size() ? functions[index] : CurveFunction{};
  }

  CurveFunction tryGetFunction(const std::string& name) {
    const auto& functions = getFunctions();
    auto it = std::find_if(functions.begin(), functions.end(), [&](const CurveFunction& fn) { return fn.name == name; });
    return it != functions.end() ? *it : CurveFunction{};
  }
}