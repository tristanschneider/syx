#pragma once

struct CurveDefinition;

//Takes a curve definition and the desired uniforms and computes the final output value
//This means computing the time value and applying it to the input value to produce the interpolated output
namespace CurveSolver {
  struct CurveUniforms {
    size_t count{};
  };
  struct CurveVaryings {
    const float* inT{};
    float* outT{};
  };

  float getDeltaTime(float curveDuration, float realTimeSeconds);
  float getDeltaTime(const CurveDefinition& definition, float realTimeSeconds);
  void advanceTime(const CurveDefinition& definition, const CurveUniforms& uniforms, CurveVaryings& varyings, float realTimeSeconds);
  void advanceTimeDT(float deltaTime, const CurveUniforms& uniforms, CurveVaryings& varyings);
  float advanceTimeDT(float toAdvance, float deltaTime);

  void solve(const CurveDefinition& definition, const CurveUniforms& uniforms, CurveVaryings varyings);
  float solve(float t, const CurveDefinition& definition);
}