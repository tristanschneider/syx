#pragma once

namespace Syx {
  bool testAllDetection();
  bool testModel();
  bool testTransform();
  bool testSimplex();

  class NarrowphaseTest {
  public:
    static bool run();
  };
}