#pragma once
#include "SyxHandles.h"

namespace Syx {
  class PhysicsObject;
  class ModelInstance;
  struct Transformer;
  class Model;
  struct Vec3;

  class CasterContext {
  public:
    const std::vector<CastResult>& GetResults(void) { return mResults; }
    void PushResult(const CastResult& result) { mResults.push_back(result); }
    void ClearResults(void) { mResults.clear(); }
    void SortResults(void);

    const Vec3* mWorldStart;
    const Vec3* mWorldEnd;
    PhysicsObject* mCurObj;
  private:
    std::vector<CastResult> mResults;
  };

  class Caster {
  public:
    void LineCast(PhysicsObject& obj, const Vec3& start, const Vec3& end, CasterContext& context) const;

  private:
    // Model space raycast, then toWorld is used to put the final results in world space
    void LineCastGJK(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void LineCastCube(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void LineCastLocal(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void LineCastComposite(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void LineCastEnvironment(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
  };
}