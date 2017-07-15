#include "Precompile.h"
#include "SyxSVector3.h"
#include "SyxSIMD.h"
#include "SyxVector3.h"

#ifdef SENABLED
namespace Syx
{
  const static int nOne = -1;
  const static float AllOne = *reinterpret_cast<const float*>(&nOne);

  const SFloats SVector3::UnitX = SLoadFloats(1.0f, 0.0f, 0.0f, 0.0f);
  const SFloats SVector3::UnitY = SLoadFloats(0.0f, 1.0f, 0.0f);
  const SFloats SVector3::UnitZ = SLoadFloats(0.0f, 0.0f, 1.0f);
  const SFloats SVector3::Zero = SLoadSplatFloats(0.0f);
  const SFloats SVector3::Identity = SLoadFloats(1.0f, 1.0f, 1.0f, 1.0f);
  const SFloats SVector3::Epsilon = SLoadFloats(SYX_EPSILON, SYX_EPSILON, SYX_EPSILON, SYX_EPSILON);
  const SFloats SVector3::BitsX = SLoadFloats(AllOne, 0.0f, 0.0f, 0.0f);
  const SFloats SVector3::BitsY = SLoadFloats(0.0f, AllOne, 0.0f, 0.0f);
  const SFloats SVector3::BitsZ = SLoadFloats(0.0f, 0.0f, AllOne, 0.0f);
  const SFloats SVector3::BitsW = SLoadFloats(0.0f, 0.0f, 0.0f, AllOne);
  const SFloats SVector3::BitsAll = SLoadFloats(AllOne, AllOne, AllOne, AllOne);
}
#endif