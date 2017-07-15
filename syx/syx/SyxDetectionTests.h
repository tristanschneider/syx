#pragma once

namespace Syx {
  bool TestAllDetection(void);
  bool TestModel(void);
  bool TestTransform(void);
  bool TestSimplex(void);

  class NarrowphaseTest {
  public:
    static bool Run(void);
  };
}