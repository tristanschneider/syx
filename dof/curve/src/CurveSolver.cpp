#include "Precompile.h"
#include "curve/CurveSolver.h"

#include "curve/CurveDefinition.h"

namespace CurveSolver {
  float getDeltaTime(float curveDuration, float realTimeSeconds) {
    return curveDuration == 0.0f ? 0.0f : realTimeSeconds / curveDuration;
  }

  float getDeltaTime(const CurveDefinition& definition, float realTimeSeconds) {
    return getDeltaTime(definition.params.duration.value_or(1.0f), realTimeSeconds);
  }

  void advanceTimeDT(float deltaTime, const CurveUniforms& uniforms, CurveVaryings& varyings) {
    //TODO: ispc
    if(deltaTime > 0.0f) {
      for(size_t i = 0; i < uniforms.count; ++i) {
        varyings.outT[i] = std::min(1.0f, varyings.inT[i] + deltaTime);
      }
    }
    else {
      for(size_t i = 0; i < uniforms.count; ++i) {
        varyings.outT[i] = std::max(0.0f, varyings.inT[i] + deltaTime);
      }
    }
  }

  float advanceTimeDT(float toAdvance, float deltaTime) {
    float result{};
    CurveVaryings varyings{ &toAdvance, &result };
    CurveUniforms uniforms{ 1 };
    advanceTimeDT(deltaTime, uniforms, varyings);
    return result;
  }

  void advanceTime(const CurveDefinition& definition, const CurveUniforms& uniforms, CurveVaryings& varyings, float realTimeSeconds) {
    advanceTimeDT(getDeltaTime(definition, realTimeSeconds), uniforms, varyings);
  }

  void scale(float scale, const float* input, float* output, size_t count) {
    for(size_t i = 0; i < count; ++i) {
      output[i] = input[i]*scale;
    }
  }

  void offset(float offset, const float* input, float* output, size_t count) {
    for(size_t i = 0; i < count; ++i) {
      output[i] = input[i] + offset;
    }
  }

  void scaleAndOffset(float scale, float offset, const float* input, float* output, size_t count) {
    for(size_t i = 0; i < count; ++i) {
      output[i] = input[i]*scale + offset;
    }
  }

  void scaleAndOffset(const CurveDefinition& definition, const float* input, float* output, size_t count) {
    if(definition.params.offset && definition.params.scale) {
      scaleAndOffset(*definition.params.scale, *definition.params.offset, input, output, count);
    }
    else if(definition.params.offset) {
      offset(*definition.params.offset, input, output, count);
    }
    else if(definition.params.scale) {
      scale(*definition.params.scale, input, output, count);
    }
  }

  void scaleAndOffset(const CurveDefinition& definition, const CurveUniforms& uniforms, CurveVaryings& varyings) {
    scaleAndOffset(definition, varyings.inT, varyings.outT, uniforms.count);
  }

  void flip(const float* input, float* output, size_t count) {
    for(size_t i = 0; i < count; ++i) {
      output[i] = 1.0f - input[i];
    }
  }

  void solve(const CurveDefinition& definition, const CurveUniforms& uniforms, CurveVaryings varyings) {
    //Still need to write something even if the function is missing in case the output buffer was uninitialized
    auto fn = definition.function.function ? definition.function.function : CurveMath::getFunction(CurveMath::CurveType::LinearInterpolation).function;
    if(definition.params.flipInput) {
      flip(varyings.inT, varyings.outT, uniforms.count);
      //Do the rest in place now that the flipped values are there
      varyings.inT = varyings.outT;
    }

    fn(varyings.inT, varyings.outT, uniforms.count);

    //In place edit the output
    varyings.inT = varyings.outT;
    if(definition.params.flipOutput) {
      flip(varyings.inT, varyings.outT, uniforms.count);
    }
    scaleAndOffset(definition, uniforms, varyings);
  }

  float solve(float t, const CurveDefinition& definition) {
    float result{};
    CurveVaryings varyings{ &t, &result };
    CurveUniforms uniforms{ 1 };
    solve(definition, uniforms, varyings);
    return result;
  }
}
