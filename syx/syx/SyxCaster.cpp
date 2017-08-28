#include "Precompile.h"
#include "SyxCaster.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"
#include "SyxSimplex.h"

namespace Syx {
  void CasterContext::sortResults(void) {
    std::sort(mResults.begin(), mResults.end());
  }

  void Caster::lineCast(PhysicsObject& obj, const Vec3& start, const Vec3& end, CasterContext& context) const {
    Collider* collider = obj.getCollider();
    if(!collider)
      return;

    ModelInstance& inst = collider->getModelInstance();
    SAlign Vec3 localStart = inst.getWorldToModel().transformPoint(start);
    SAlign Vec3 localEnd = inst.getWorldToModel().transformPoint(end);
    context.mWorldStart = &start;
    context.mWorldEnd = &end;
    context.mCurObj = &obj;
    _lineCastLocal(inst.getModel(), inst.getModelToWorld(), localStart, localEnd, context);
  }

  void Caster::_lineCastLocal(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const {
    switch(model.getType()) {
      case ModelType::Cube: _lineCastCube(model, toWorld, start, end, context); break;
      case ModelType::Composite: _lineCastComposite(model, toWorld, start, end, context); break;
      case ModelType::Environment: _lineCastEnvironment(model, toWorld, start, end, context); break;
      default: _lineCastGJK(model, toWorld, start, end, context); break;
    }
  }

  void Caster::_lineCastGJK(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const {
    SAlign Vec3 curNormal;
    Vec3 rayDir = end - start;
    //start - Arbitrary point in object. Support towards the start of the ray
    Vec3 curSearchDir = start - model.getSupport(start);
    SupportPoint curSupport;
    Simplex simplex;
    //A safeguard against infinite loops. Shouldn't happen, but just in case
    unsigned iteration = 0;
    float lowerBound = 0.0f;

    while(iteration++ < 20) {
      Vec3 lowerBoundPoint = Vec3::lerp(start, end, lowerBound);

      //Support is in CSO, this is just on the collider
      Vec3 supportOnC = model.getSupport(curSearchDir);
      curSupport.mPointB = supportOnC;
      curSupport.mSupport = lowerBoundPoint - supportOnC;

      float searchDotSupport = curSearchDir.dot(curSupport.mSupport);
      float searchDotRay = curSearchDir.dot(rayDir);

      if(searchDotSupport > 0.0f) {
        if(searchDotRay >= 0.0)
          return;

        lowerBound -= searchDotSupport/searchDotRay;
        if(lowerBound > 1.0f)
          return;
        curNormal = curSearchDir;
        Vec3 newLowerBoundPoint = Vec3::lerp(start, end, lowerBound);

        //Lower bound updated, translate support points along ray
        for(unsigned i = 0; i < simplex.size(); ++i) {
          SupportPoint& support = simplex.getSupport(i);
          support.mSupport = newLowerBoundPoint - support.mPointB;
        }
        curSupport.mSupport = newLowerBoundPoint - curSupport.mPointB;
      }

      //Add to simplex and evaluate it, removing points in the wrong direction and finding new search direction
      simplex.add(curSupport, true);
      //Solve returns vector from closest point to origin, so subtract origin back off
      Vec3 closestToOrigin = -simplex.solve();

      //Simplex contains origin, hit found
      if(simplex.containsOrigin() || simplex.isDegenerate())
        break;

      curSearchDir = closestToOrigin;
    }

    SAlign Vec3 localPoint = Vec3::lerp(start, end, lowerBound);
    Vec3 worldPoint = toWorld.transformPoint(localPoint);
    Vec3 worldNormal = toWorld.transformVector(curNormal).normalized();
    float dist2 = worldPoint.distance2(*context.mWorldStart);
    context.pushResult(CastResult(context.mCurObj->getHandle(), worldPoint, worldNormal, dist2));
  }

  void Caster::_lineCastCube(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const {
    float t;
    int normalIndex, normalSign;
    //Use model's aabb, which is in model space
    if(model.getAABB().lineIntersect(start, end, &t, &normalIndex, &normalSign)) {
      CastResult result;
      result.mPoint = Vec3::lerp(*context.mWorldStart, *context.mWorldEnd, t);
      result.mDistSq = result.mPoint.distance2(*context.mWorldStart);
      //Local space normal from index transformed into world
      SAlign Vec3 normal = Vec3::Zero;
      normal[normalIndex] = static_cast<float>(normalSign);
      //Nonuniform scale would be a problem here if the normal wasn't axis aligned, but it will be since it's a cube
      result.mNormal = toWorld.transformVector(normal).normalized();
      result.mObj = context.mCurObj->getHandle();
      context.pushResult(result);
    }
  }

  void Caster::_lineCastComposite(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const {
    const auto& submodels = model.getSubmodelInstances();
    for(const ModelInstance& subModel : submodels) {
      SAlign Vec3 localStart = subModel.getWorldToModel().transformPoint(start);
      SAlign Vec3 localEnd = subModel.getWorldToModel().transformPoint(end);
      Transformer localToWorld = Transformer::combined(subModel.getModelToWorld(), toWorld);
      _lineCastLocal(subModel.getModel(), localToWorld, localStart, localEnd, context);
    }
  }

  void Caster::_lineCastEnvironment(const Model& model, const Transformer& toWorld, const Vec3& start, const Vec3& end, CasterContext& context) const {
    const auto& tris = model.getTriangles();
    for(size_t i = 0; i + 2 < tris.size(); i += 3) {
      const Vec3& a = tris[i];
      const Vec3& b = tris[i + 1];
      const Vec3& c = tris[i + 2];
      float t = triangleLineIntersect(a, b, c, start, end);
      if(t < 0.0f)
        continue;

      CastResult result;
      result.mPoint = Vec3::lerp(*context.mWorldStart, *context.mWorldEnd, t);
      SAlign Vec3 normal = triangleNormal(a, b, c);
      //Make sure normal is facing start point
      if((start - a).dot(normal) < 0.0f)
        normal = -normal;
      result.mNormal = toWorld.transformVector(normal).normalized();
      result.mDistSq = context.mWorldStart->distance2(result.mPoint);
      result.mObj = context.mCurObj->getHandle();
      context.pushResult(result);
    }
  }
}