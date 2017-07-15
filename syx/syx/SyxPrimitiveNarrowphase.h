#pragma once

namespace Syx {
  class ModelInstance;
  class Space;
  class Narrowphase;

  class PrimitiveNarrowphase {
  public:
    void Set(ModelInstance* a, ModelInstance* b, Space* space, Narrowphase* narrowphase);

    void SphereSphere(void);

  private:
    ModelInstance* mA;
    ModelInstance* mB;
    Space* mSpace;
    Narrowphase* mNarrowphase;
  };
}