#pragma once

namespace CurveMath {
  void backEaseIn(const float * inT, float * outT, int32_t count);
  void backEaseInOut(const float * inT, float * outT, int32_t count);
  void backEaseOut(const float * inT, float * outT, int32_t count);
  void bounceEaseIn(const float * inT, float * outT, int32_t count);
  void bounceEaseInOut(const float * inT, float * outT, int32_t count);
  void bounceEaseOut(const float * inT, float * outT, int32_t count);
  void circularEaseIn(const float * inT, float * outT, int32_t count);
  void circularEaseInOut(const float * inT, float * outT, int32_t count);
  void circularEaseOut(const float * inT, float * outT, int32_t count);
  void cubicEaseIn(const float * inT, float * outT, int32_t count);
  void cubicEaseInOut(const float * inT, float * outT, int32_t count);
  void cubicEaseOut(const float * inT, float * outT, int32_t count);
  void elasticEaseIn(const float * inT, float * outT, int32_t count);
  void elasticEaseInOut(const float * inT, float * outT, int32_t count);
  void elasticEaseOut(const float * inT, float * outT, int32_t count);
  void exponentialEaseIn(const float * inT, float * outT, int32_t count);
  void exponentialEaseInOut(const float * inT, float * outT, int32_t count);
  void exponentialEaseOut(const float * inT, float * outT, int32_t count);
  void linearInterpolation(const float * inT, float * outT, int32_t count);
  void one(const float * inT, float * outT, int32_t count);
  void quadraticEaseIn(const float * inT, float * outT, int32_t count);
  void quadraticEaseInOut(const float * inT, float * outT, int32_t count);
  void quadraticEaseOut(const float * inT, float * outT, int32_t count);
  void quarticEaseIn(const float * inT, float * outT, int32_t count);
  void quarticEaseInOut(const float * inT, float * outT, int32_t count);
  void quarticEaseOut(const float * inT, float * outT, int32_t count);
  void quinticEaseIn(const float * inT, float * outT, int32_t count);
  void quinticEaseInOut(const float * inT, float * outT, int32_t count);
  void quinticEaseOut(const float * inT, float * outT, int32_t count);
  void sineEaseIn(const float * inT, float * outT, int32_t count);
  void sineEaseInOut(const float * inT, float * outT, int32_t count);
  void sineEaseOut(const float * inT, float * outT, int32_t count);
  void zero(const float * inT, float * outT, int32_t count);

  enum class CurveType : uint8_t {
    BackEaseIn,
    BackEaseInOut,
    BackEaseOut,
    BounceEaseIn,
    BounceEaseInOut,
    BounceEaseOut,
    CircularEaseIn,
    CircularEaseInOut,
    CircularEaseOut,
    CubicEaseIn,
    CubicEaseInOut,
    CubicEaseOut,
    ElasticEaseIn,
    ElasticEaseInOut,
    ElasticEaseOut,
    ExponentialEaseIn,
    ExponentialEaseInOut,
    ExponentialEaseOut,
    LinearInterpolation,
    One,
    QuadraticEaseIn,
    QuadraticEaseInOut,
    QuadraticEaseOut,
    QuarticEaseIn,
    QuarticEaseInOut,
    QuarticEaseOut,
    QuinticEaseIn,
    QuinticEaseInOut,
    QuinticEaseOut,
    SineEaseIn,
    SineEaseInOut,
    SineEaseOut,
    Zero,

    Count
  };

  struct CurveFunction {
    void(*function)(const float*, float*, int32_t){};
    const char* name{};
    CurveType type{};
  };
  CurveFunction getFunction(CurveType type);
  CurveFunction tryGetFunction(CurveType type);
  CurveFunction tryGetFunction(const std::string& name);
}