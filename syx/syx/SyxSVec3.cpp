#include "Precompile.h"

#ifdef SENABLED
namespace Syx {
  const static int nOne = -1;
  const static float AllOne = *reinterpret_cast<const float*>(&nOne);

  const SFloats SVec3::UnitX = sLoadFloats(1.0f, 0.0f, 0.0f, 0.0f);
  const SFloats SVec3::UnitY = sLoadFloats(0.0f, 1.0f, 0.0f);
  const SFloats SVec3::UnitZ = sLoadFloats(0.0f, 0.0f, 1.0f);
  const SFloats SVec3::Zero = sLoadSplatFloats(0.0f);
  const SFloats SVec3::Identity = sLoadFloats(1.0f, 1.0f, 1.0f, 1.0f);
  const SFloats SVec3::Epsilon = sLoadFloats(SYX_EPSILON, SYX_EPSILON, SYX_EPSILON, SYX_EPSILON);
  const SFloats SVec3::BitsX = sLoadFloats(AllOne, 0.0f, 0.0f, 0.0f);
  const SFloats SVec3::BitsY = sLoadFloats(0.0f, AllOne, 0.0f, 0.0f);
  const SFloats SVec3::BitsZ = sLoadFloats(0.0f, 0.0f, AllOne, 0.0f);
  const SFloats SVec3::BitsW = sLoadFloats(0.0f, 0.0f, 0.0f, AllOne);
  const SFloats SVec3::BitsAll = sLoadFloats(AllOne, AllOne, AllOne, AllOne);
}
#endif