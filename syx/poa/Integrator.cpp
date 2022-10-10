#include "Integrator.h"

#include "test.h"

//    extern void simple(float * vin, float * vout, int32_t count);


void integratorTest() {
  constexpr int32_t SIZE = 5;
  float i[SIZE] = { 1, 2, 3, 4, 5 };
  float o[SIZE] = { 0 };

  ispc::simple(i, o , SIZE);
}
