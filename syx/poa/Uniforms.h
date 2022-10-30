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

struct UniformQuat {
  uniform float* i;
  uniform float* j;
  uniform float* k;
  uniform float* w;
};