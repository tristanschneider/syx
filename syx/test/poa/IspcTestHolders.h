#pragma once

#include "out_ispc/Inertia.h"
#include "out_ispc/Integrator.h"

namespace poa {
  static constexpr inline uint32_t SIZE = 5;
  using FloatArray = std::array<float, size_t(SIZE)>;

  struct SymmetricMatrixHolder {
    FloatArray a, b, c, d, e, f;
    ispc::UniformSymmetricMatrix mValue{ a.data(), b.data(), c.data(), d.data(), e.data(), f.data() };
  };

  struct Vec3Holder {
    void set(size_t i, float a, float b, float c) {
      x[i] = a;
      y[i] = b;
      z[i] = c;
    }

    FloatArray x, y, z;
    ispc::UniformVec3 mValue{ x.data(), y.data(), z.data() };
    ispc::UniformConstVec3 mConstValue{ x.data(), y.data(), z.data() };
  };

  struct QuatHolder {
    void set(size_t idx, float a, float b, float c, float d) {
      i[idx] = a;
      j[idx] = b;
      k[idx] = c;
      w[idx] = d;
    }

    FloatArray i, j, k, w;
    ispc::UniformQuat mValue{ i.data(), j.data(), k.data(), w.data() };
    ispc::UniformConstQuat mConstValue{ i.data(), j.data(), k.data(), w.data() };
  };
}