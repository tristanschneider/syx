#pragma once

struct UniformSymmetricMatrix {
  uniform float* a; uniform float* b; uniform float* c;
                    uniform float* d; uniform float* e;
                                      uniform float* f;
};

struct UniformVec3 {
  uniform float* x;
  uniform float* y;
  uniform float* z;
};

struct UniformConstVec3 {
  uniform const float* x;
  uniform const float* y;
  uniform const float* z;
};

struct UniformQuat {
  uniform float* i;
  uniform float* j;
  uniform float* k;
  uniform float* w;
};

struct UniformConstQuat {
  uniform const float* i;
  uniform const float* j;
  uniform const float* k;
  uniform const float* w;
};