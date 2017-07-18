#include "Precompile.h"

#ifdef SENABLED
namespace Syx {
  const static int nOne = -1;
  const static float AllOne = *reinterpret_cast<const float*>(&nOne);

  const SFloats SVec3::UnitX = SLoadFloats(1.0f, 0.0f, 0.0f, 0.0f);
  const SFloats SVec3::UnitY = SLoadFloats(0.0f, 1.0f, 0.0f);
  const SFloats SVec3::UnitZ = SLoadFloats(0.0f, 0.0f, 1.0f);
  const SFloats SVec3::Zero = SLoadSplatFloats(0.0f);
  const SFloats SVec3::Identity = SLoadFloats(1.0f, 1.0f, 1.0f, 1.0f);
  const SFloats SVec3::Epsilon = SLoadFloats(SYX_EPSILON, SYX_EPSILON, SYX_EPSILON, SYX_EPSILON);
  const SFloats SVec3::BitsX = SLoadFloats(AllOne, 0.0f, 0.0f, 0.0f);
  const SFloats SVec3::BitsY = SLoadFloats(0.0f, AllOne, 0.0f, 0.0f);
  const SFloats SVec3::BitsZ = SLoadFloats(0.0f, 0.0f, AllOne, 0.0f);
  const SFloats SVec3::BitsW = SLoadFloats(0.0f, 0.0f, 0.0f, AllOne);
  const SFloats SVec3::BitsAll = SLoadFloats(AllOne, AllOne, AllOne, AllOne);
}
#endif