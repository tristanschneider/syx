#pragma once
#include "SyxVector3.h"
#include <vector>

#define SyxInvalidHandle static_cast<size_t>(-1)

namespace Syx {
  typedef size_t Handle;

  class HandleGenerator {
  public:
    HandleGenerator() {
      Reset();
    }

    void Reset() {
      mNewKey = 0;
    }

    Handle Next() {
      Handle result = mNewKey++;
      if(mNewKey == SyxInvalidHandle)
        ++mNewKey;
      return result;
    }

  private:
    Handle mNewKey;
  };

  struct CastResult {
    CastResult(void): mObj(SyxInvalidHandle) {}
    CastResult(Handle obj, const Vector3& point, const Vector3& normal, float distSq)
      : mObj(obj)
      , mPoint(point)
      , mNormal(normal)
      , mDistSq(distSq) {
    }
    CastResult(const std::vector<CastResult>* results): mResults(results) {}

    bool operator<(const CastResult& rhs) { return mDistSq < rhs.mDistSq; }

    //Which is used is determined by the cast call, first vs all
    union {
      Handle mObj;
      const std::vector<CastResult>* mResults;
    };
    Vector3 mPoint;
    Vector3 mNormal;
    float mDistSq;
  };
}