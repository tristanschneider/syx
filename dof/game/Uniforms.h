#pragma once

struct UniformVec2 {
  uniform float* x;
  uniform float* y;
};

struct UniformConstVec2 {
  uniform const float* x;
  uniform const float* y;
};

struct UniformRotation {
  uniform float* cosAngle;
  uniform float* sinAngle;
};

struct UniformConstRotation {
  uniform const float* cosAngle;
  uniform const float* sinAngle;
};
