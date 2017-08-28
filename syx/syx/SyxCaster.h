#pragma once

namespace Syx {
  class PhysicsObject;
  class ModelInstance;
  struct Transformer;
  class Model;
  struct Vec3;

  class CasterContext {
  public:
    const std::vector<CastResult>& getResults(void) { return mResults; }
    void pushResult(const CastResult& result) { mResults.push_back(result); }
    void clearResults(void) { mResults.clear(); }
    void sortResults(void);

    const Vec3* mWorldStart;
    const Vec3* mWorldEnd;
    PhysicsObject* mCurObj;
  private:
    std::vector<CastResult> mResults;
  };

  class Caster {
  public:
    void lineCast(PhysicsObject& obj, const Vec3& start, const Vec3& end, CasterContext& context) const;

  private:
    // Model space raycast, then toWorld is used to put the final results in world space
    void _lineCastGJK(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void _lineCastCube(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void _lineCastLocal(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void _lineCastComposite(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
    void _lineCastEnvironment(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const;
  };
}