#include "Precompile.h"
#include "SyxCaster.h"
#include "SyxPhysicsObject.h"
#include "SyxModel.h"
#include "SyxSimplex.h"

namespace Syx {
  void CasterContext::SortResults(void) {
    std::sort(mResults.begin(), mResults.end());
  }

  void Caster::LineCast(PhysicsObject& obj, const Vector3& start, const Vector3& end, CasterContext& context) const {
    Collider* collider = obj.GetCollider();
    if(!collider)
      return;

    ModelInstance& inst = collider->GetModelInstance();
    SAlign Vector3 localStart = inst.GetWorldToModel().TransformPoint(start);
    SAlign Vector3 localEnd = inst.GetWorldToModel().TransformPoint(end);
    context.mWorldStart = &start;
    context.mWorldEnd = &end;
    context.mCurObj = &obj;
    LineCastLocal(inst.GetModel(), inst.GetModelToWorld(), localStart, localEnd, context);
  }

  void Caster::LineCastLocal(const Model& model, const Transformer& toWorld, const Vector3& start, const Vector3& end, CasterContext& context) const {
    switch(model.GetType()) {
      case ModelType::Cube: LineCastCube(model, toWorld, start, end, context); break;
      case ModelType::Composite: LineCastComposite(model, toWorld, start, end, context); break;
      case ModelType::Environment: LineCastEnvironment(model, toWorld, start, end, context); break;
      default: LineCastGJK(model, toWorld, start, end, context); break;
    }
  }

  void Caster::LineCastGJK(const Model& model, const Transformer& toWorld, const Vector3& start, const Vector3& end, CasterContext& context) const {
    SAlign Vector3 curNormal;
    Vector3 rayDir = end - start;
    //start - Arbitrary point in object. Support towards the start of the ray
    Vector3 curSearchDir = start - model.GetSupport(start);
    SupportPoint curSupport;
    Simplex simplex;
    //A safeguard against infinite loops. Shouldn't happen, but just in case
    unsigned iteration = 0;
    float lowerBound = 0.0f;

    while(iteration++ < 20) {
      Vector3 lowerBoundPoint = Vector3::Lerp(start, end, lowerBound);

      //Support is in CSO, this is just on the collider
      Vector3 supportOnC = model.GetSupport(curSearchDir);
      curSupport.mPointB = supportOnC;
      curSupport.mSupport = lowerBoundPoint - supportOnC;

      float searchDotSupport = curSearchDir.Dot(curSupport.mSupport);
      float searchDotRay = curSearchDir.Dot(rayDir);

      if(searchDotSupport > 0.0f) {
        if(searchDotRay >= 0.0)
          return;

        lowerBound -= searchDotSupport/searchDotRay;
        if(lowerBound > 1.0f)
          return;
        curNormal = curSearchDir;
        Vector3 newLowerBoundPoint = Vector3::Lerp(start, end, lowerBound);

        //Lower bound updated, translate support points along ray
        for(unsigned i = 0; i < simplex.Size(); ++i) {
          SupportPoint& support = simplex.GetSupport(i);
          support.mSupport = newLowerBoundPoint - support.mPointB;
        }
        curSupport.mSupport = newLowerBoundPoint - curSupport.mPointB;
      }

      //Add to simplex and evaluate it, removing points in the wrong direction and finding new search direction
      simplex.Add(curSupport, true);
      //Solve returns vector from closest point to origin, so subtract origin back off
      Vector3 closestToOrigin = -simplex.Solve();

      //Simplex contains origin, hit found
      if(simplex.ContainsOrigin() || simplex.IsDegenerate())
        break;

      curSearchDir = closestToOrigin;
    }

    SAlign Vector3 localPoint = Vector3::Lerp(start, end, lowerBound);
    Vector3 worldPoint = toWorld.TransformPoint(localPoint);
    Vector3 worldNormal = toWorld.TransformVector(curNormal).Normalized();
    float dist2 = worldPoint.Distance2(*context.mWorldStart);
    context.PushResult(CastResult(context.mCurObj->GetHandle(), worldPoint, worldNormal, dist2));
  }

  void Caster::LineCastCube(const Model& model, const Transformer& toWorld, const Vector3& start, const Vector3& end, CasterContext& context) const {
    float t;
    int normalIndex, normalSign;
    //Use model's aabb, which is in model space
    if(model.GetAABB().LineIntersect(start, end, &t, &normalIndex, &normalSign)) {
      CastResult result;
      result.mPoint = Vector3::Lerp(*context.mWorldStart, *context.mWorldEnd, t);
      result.mDistSq = result.mPoint.Distance2(*context.mWorldStart);
      //Local space normal from index transformed into world
      SAlign Vector3 normal = Vector3::Zero;
      normal[normalIndex] = static_cast<float>(normalSign);
      //Nonuniform scale would be a problem here if the normal wasn't axis aligned, but it will be since it's a cube
      result.mNormal = toWorld.TransformVector(normal).Normalized();
      result.mObj = context.mCurObj->GetHandle();
      context.PushResult(result);
    }
  }

  void Caster::LineCastComposite(const Model& model, const Transformer& toWorld, const Vector3& start, const Vector3& end, CasterContext& context) const {
    const auto& submodels = model.GetSubmodelInstances();
    for(const ModelInstance& subModel : submodels) {
      SAlign Vector3 localStart = subModel.GetWorldToModel().TransformPoint(start);
      SAlign Vector3 localEnd = subModel.GetWorldToModel().TransformPoint(end);
      Transformer localToWorld = Transformer::Combined(subModel.GetModelToWorld(), toWorld);
      LineCastLocal(subModel.GetModel(), localToWorld, localStart, localEnd, context);
    }
  }

  void Caster::LineCastEnvironment(const Model& model, const Transformer& toWorld, const Vector3& start, const Vector3& end, CasterContext& context) const {
    const auto& tris = model.GetTriangles();
    for(size_t i = 0; i + 2 < tris.size(); i += 3) {
      const Vector3& a = tris[i];
      const Vector3& b = tris[i + 1];
      const Vector3& c = tris[i + 2];
      float t = TriangleLineIntersect(a, b, c, start, end);
      if(t < 0.0f)
        continue;

      CastResult result;
      result.mPoint = Vector3::Lerp(*context.mWorldStart, *context.mWorldEnd, t);
      SAlign Vector3 normal = TriangleNormal(a, b, c);
      //Make sure normal is facing start point
      if((start - a).Dot(normal) < 0.0f)
        normal = -normal;
      result.mNormal = toWorld.TransformVector(normal).Normalized();
      result.mDistSq = context.mWorldStart->Distance2(result.mPoint);
      result.mObj = context.mCurObj->GetHandle();
      context.PushResult(result);
    }
  }
}