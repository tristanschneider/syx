#include "SyxNarrowphase.h"
#include "SyxPhysicsObject.h"
#include "SyxCollider.h"
#include "SyxManifold.h"
#include "SyxSpace.h"
#include "SyxAABBTree.h"

//#define DEBUG_GJK
//#define DEBUG_EPA

#ifdef DEBUG_GJK
#include "SyxDebugSimplex.h"
#endif

namespace Syx {
  float Narrowphase::sepaEpsilon = SYX_EPSILON;

  Narrowphase::Narrowphase()
    : mBroadphaseContext(new AABBTreeContext())
    , mTempTri(ModelType::Triangle) {
    InitHandlers();
  }

  Narrowphase::~Narrowphase() {
    delete mBroadphaseContext;
  }

  Narrowphase::Narrowphase(const Narrowphase& other) {
    *this = other;
  }

  Narrowphase& Narrowphase::operator=(const Narrowphase& rhs) {
    mSimplex = rhs.mSimplex;
    mA = rhs.mA;
    mB = rhs.mB;
    mInstA = rhs.mInstA;
    mInstB = rhs.mInstB;
    mSpace = rhs.mSpace;
    mPrimitive = rhs.mPrimitive;
    mTris = rhs.mTris;
    mEdges = rhs.mEdges;
    mVerts = rhs.mVerts;
    mTempTri = rhs.mTempTri;
    mBroadphaseContext = new AABBTreeContext();
    InitHandlers();
    return *this;
  }

  SupportPoint Narrowphase::GetSupport(const Vec3& dir) {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::SupportPoints)
      return SGetSupport(dir);

    Vec3 supportA = mInstA->GetSupport(dir);
    SAlign Vec3 negDir = -dir;
    Vec3 supportB = mInstB->GetSupport(negDir);
    return SupportPoint(supportA, supportB);
  }

  bool Narrowphase::GJK(void) {
    mSimplex.Initialize();
    //Arbitrary start direction
    SAlign Vec3 curDir(Vec3::UnitY);
    SupportPoint support = GetSupport(curDir);

    int iterationCap = 100;
    int iteration = 0;

    while(iteration++ < iterationCap) {
      mSimplex.Add(support, false);
      curDir = mSimplex.Solve();

      if(mSimplex.ContainsOrigin())
        return true;
      else if(mSimplex.IsDegenerate())
        return false;

      support = GetSupport(curDir);

      // If we didn't pass over the origin, no collision. Only works when used for collision, not closest point or raycast
      // Dotting points looks weird, but it's actually vectors from origin to the supports, but that's subtraction by zero
      if(support.mSupport.Dot(curDir) < 0.0f)
        return false;

#ifdef DEBUG_GJK
      bool drawIteration = (iteration == gOptions.mTest && gOptions.mDebugFlags & SyxOptions::Debug::DrawGJK);
      if(drawIteration) {
        DebugDrawer& d = DebugDrawer::Get();
        d.SetColor(1.0f, 0.0f, 0.0f);
        DrawCube(support.mSupport, 0.1f);
        d.SetColor(0.0f, 1.0f, 1.0f);
        DrawCube(support.mPointA, 0.03f);
        DrawCube(support.mPointB, 0.03f);
      }
#endif
    }

    Interface::Log("GJK Iteration cap reached");
    return false;
  }

  Vec3 Narrowphase::EPA(ContactPoint& result) {
    InitEPASimplex();

    int iterationCap = 100;
    int iteration = 0;
    while(iteration++ < iterationCap) {
      SupportTri* bestTri = GetClosestTri();

      mVerts.push_back(GetSupport(bestTri->m_halfPlane));

      SupportPoint* newVert = &mVerts.back();
      float progress = bestTri->SignedNormalDist(newVert->mSupport);

#ifdef DEBUG_EPA
      if(iteration == gOptions.mTest && gOptions.mDebugFlags & SyxOptions::Debug::DrawEPA) {
        DebugDrawer::Get().SetColor(0.0f, 1.0f, 0.0f);
        DrawEPA(bestTri);
      }
#endif

      if(progress <= sepaEpsilon)
        return StoreEPAResult(result, bestTri);

      DeleteInteriorTris(newVert->mSupport);
      ReconstructTriangles();
    }

    Interface::Log("EPA Iteration cap reached");
    return Vec3::Zero;
  }

  Vec3 Narrowphase::StoreEPAResult(ContactPoint& result, SupportTri* bestTri) {
    Vec3 originOnTri;
    SupportPoint *a, *b, *c;
    Vec3 bary;
    bool first = true;

    if(!bestTri) {
      Interface::Log("Problems! No resulting triangle in StoreEPAResult");
      return Vec3::Zero;
    }

    do {
      //We're given the first one, but after that we need to throw away the invalid one and get a new one
      if(!first) {
        int index = bestTri - &*mTris.begin();
        SwapRemove(mTris, index);
        bestTri = GetClosestTri();
      }
      first = false;

      if(!bestTri) {
        Interface::Log("Problems! No resulting triangle in StoreEPAResult");
        return Vec3::Zero;
      }

      a = &mVerts[bestTri->m_verts[0]];
      b = &mVerts[bestTri->m_verts[1]];
      c = &mVerts[bestTri->m_verts[2]];

      originOnTri = bestTri->Project(Vec3::Zero);
      bary = PointToBarycentric(a->mSupport, b->mSupport, c->mSupport, originOnTri);
    }
    while(!ValidBarycentric(bary));

    result.mObjA.mStartingWorld = result.mObjA.mCurrentWorld = BarycentricToPoint(a->mPointA, b->mPointA, c->mPointA, bary);
    result.mObjB.mStartingWorld = result.mObjB.mCurrentWorld = BarycentricToPoint(a->mPointB, b->mPointB, c->mPointB, bary);

    result.mObjA.mModelPoint = mA->GetTransform().WorldToModel(result.mObjA.mStartingWorld);
    result.mObjB.mModelPoint = mB->GetTransform().WorldToModel(result.mObjB.mStartingWorld);
    result.mPenetration = (result.mObjB.mStartingWorld - result.mObjA.mStartingWorld).Dot(-bestTri->m_halfPlane);
    result.mWarmContact = result.mWarmFriction[0] = result.mWarmFriction[1] = 0.0f;
    return -bestTri->m_halfPlane;
  }

  void Narrowphase::InitEPASimplex(void) {
    mSimplex.GrowToFourPoints(*this);

    mVerts.clear();
    mEdges.clear();
    mTris.clear();

    mVerts.push_back(mSimplex.GetSupport(SupportID::A));
    mVerts.push_back(mSimplex.GetSupport(SupportID::B));
    mVerts.push_back(mSimplex.GetSupport(SupportID::C));
    mVerts.push_back(mSimplex.GetSupport(SupportID::D));

    //bad, cbd, acd, abc
    mTris.push_back(SupportTri(SupportID::B, SupportID::A, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::C, SupportID::B, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::A, SupportID::C, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::A, SupportID::B, SupportID::C, mVerts));
  }

  SupportTri* Narrowphase::GetClosestTri(void) {
    if(mTris.empty())
      return nullptr;

    //Always negating distance because it's signed and we know the origin is inside, meaning the distance is negative
    SupportTri* result = &mTris[0];
    float bestDist = std::numeric_limits<float>::max();
    for(size_t i = 0; i < mTris.size(); ++i) {
      float curDist = std::abs(mTris[i].OriginDist());
      if(curDist < bestDist) {
        result = &mTris[i];
        bestDist = curDist;
      }
    }
    return result;
  }

  void Narrowphase::DeleteInteriorTris(const Vec3& newPoint) {
    for(size_t i = 0; i < mTris.size();) {
      SupportTri& curTri = mTris[i];
      //If triangle is facing new vertex. Doesn't matter which point I choose, direction is similar enough
      if(curTri.SignedNormalDist(newPoint) > 0.0f) {
        //Push edges on the list so they can later be used to construct new triangles with new vertex
        curTri.AddEdges(mEdges);
        SwapRemove(mTris, i);
      }
      else
        ++i;
    }
  }

  void Narrowphase::ReconstructTriangles(void) {
    //Connect deleted triangles' edges to new face
    for(size_t i = 0; i < mEdges.size();) {
      auto& curEdge = mEdges[i];
      //See if an opposite edge to this one exists in the list, and if so, ignore both
      //I should use a hashmap for this search
      bool edgeRemoved = false;
      for(size_t j = i + 1; j < mEdges.size(); ++j) {
        auto& searchEdge = mEdges[j];
        if(searchEdge.m_from == curEdge.m_to &&
          searchEdge.m_to == curEdge.m_from) {
          SwapRemove(mEdges, j);
          SwapRemove(mEdges, i);
          edgeRemoved = true;
          break;
        }
      }
      if(edgeRemoved)
        continue;

      //Construct a triangle out of this edge with the new vertex
      mTris.push_back(SupportTri(curEdge.m_from, curEdge.m_to, mVerts.size() - 1, mVerts));
      ++i;
    }

    //Clear edge list for use next frame
    mEdges.clear();
  }

  void Narrowphase::GJKEPAHandler(void) {
    bool collision;
    if(gOptions.mSimdFlags & SyxOptions::SIMD::GJK)
      collision = SGJK();
    else
      collision = GJK();

    if(collision) {
      if(gOptions.mDebugFlags & SyxOptions::Debug::DrawCollidingPairs)
        DebugDrawer::Get().DrawLine(mA->GetTransform().mPos, mB->GetTransform().mPos);

      ContactPoint resultPoint;
      Vec3 normal;
      if(gOptions.mSimdFlags & SyxOptions::SIMD::EPA)
        normal = SEPA(resultPoint);
      else
        normal = EPA(resultPoint);

      if(normal != Vec3::Zero)
        SubmitContact(resultPoint, normal);
      else
        Interface::Log("Invalid contact normal");
    }
  }

  void Narrowphase::ProcessPairQuery(const BroadPairs& pairs, Space& space) {
    mSpace = &space;
    for(auto& pair : pairs) {
      mA = reinterpret_cast<PhysicsObject*>(pair.first.mUserdata);
      mB = reinterpret_cast<PhysicsObject*>(pair.second.mUserdata);
      //Eventually broadphase shouldn't even return these in the query
      if(mA->GetAsleep() && mB->GetAsleep())
        continue;
      //Ensure always consistent ordering of a and b
      if(mA->GetHandle() < mB->GetHandle())
        std::swap(mA, mB);

      //Update these values in case the primitive narrowphase is used for this
      mPrimitive.Set(mInstA, mInstB, mSpace, this);
      mInstA = &mA->GetCollider()->GetModelInstance();
      mInstB = &mB->GetCollider()->GetModelInstance();

      HandlePair();
    }
  }

  void Narrowphase::SwapAB() {
    std::swap(mA, mB);
    std::swap(mInstA, mInstB);
    mPrimitive.Set(mInstA, mInstB, mSpace, this);
  }

  void Narrowphase::ProcessRayQuery(const BroadResults& /*objs*/, const Vec3& /*start*/, const Vec3& /*end*/, Space& /*space*/) {

  }

  void Narrowphase::ProcessVolumeQuery(const BroadResults& /*objs*/, const BoundingVolume& /*volume*/, Space& /*space*/) {

  }

  void Narrowphase::DrawEPA(SupportTri* bestTri) {
    DebugDrawer& d = DebugDrawer::Get();
    bestTri = bestTri;
    d.SetColor(1.0f, 0.0f, 0.0f);
    for(auto& tri : mTris) {
      Vec3 a = mVerts[tri.m_verts[0]].mSupport;
      Vec3 b = mVerts[tri.m_verts[1]].mSupport;
      Vec3 c = mVerts[tri.m_verts[2]].mSupport;
      DrawTriangle(a, b, c, true);
    }

    d.SetColor(1.0f, 0.0f, 1.0f);
    DrawSphere(Vec3::Zero, 0.03f);
    d.DrawPoint(bestTri->Project(Vec3::Zero), 0.01f);
  }

  void Narrowphase::SphereSphereHandler() {
    mPrimitive.SphereSphere();
  }

  void Narrowphase::CompositeOtherHandler() {
    //Transform b's bounding box into a's space so it can test against all a's submodels
    AABB localB = mB->GetCollider()->GetAABB().Transform(mInstA->GetWorldToModel());
    ModelInstance* compositeRoot = mInstA;

    //This would be the place to have a local space broadphase for composite models, but we're doing them all now
    auto& subInsts = compositeRoot->GetModel().GetSubmodelInstances();
    for(size_t i = 0; i < subInsts.size(); ++i) {
      const ModelInstance& subInst = subInsts[i];
      if(localB.Overlapping(subInst.GetAABB())) {
        //Construct a model instance with the combined transforms so there aren't extra transforms when getting supports
        ModelInstance tempWorldInst = ModelInstance::Combined(*compositeRoot, subInst, subInst, compositeRoot->GetSubmodelInstHandle(i));
        mInstA = &tempWorldInst;
        mPrimitive.Set(mInstA, mInstB, mSpace, this);
        HandlePair();
      }
    }
  }

  void Narrowphase::OtherCompositeHandler() {
    SwapAB();
    CompositeOtherHandler();
  }

  void Narrowphase::CompositeCompositeHandler() {
    //We'll calculate in the space of the composite object with more submodels
    if(mInstA->GetModel().GetSubmodelInstances().size() < mInstB->GetModel().GetSubmodelInstances().size()) {
      SwapAB();
    }

    Transformer localBToLocalA = Transformer::Combined(mInstB->GetModelToWorld(), mInstA->GetWorldToModel());
    ModelInstance* rootA = mInstA;
    ModelInstance* rootB = mInstB;

    //N^2 composite to composite. Needs broadphase!
    auto& subInstsB = rootB->GetModel().GetSubmodelInstances();
    for(size_t i = 0; i < subInstsB.size(); ++i) {
      const ModelInstance& subInstB = subInstsB[i];
      AABB bInA = subInstB.GetAABB().Transform(localBToLocalA);
      ModelInstance tempWorldInstB = ModelInstance::Combined(*rootB, subInstB, subInstB, rootB->GetSubmodelInstHandle(i));

      auto& subInstsA = rootA->GetModel().GetSubmodelInstances();
      for(size_t j = 0; j < subInstsA.size(); ++j) {
        const ModelInstance& subInstA = subInstsA[j];
        if(subInstA.GetAABB().Overlapping(bInA)) {
          ModelInstance tempWorldInstA = ModelInstance::Combined(*rootA, subInstA, subInstA, rootA->GetSubmodelInstHandle(j));
          mInstA = &tempWorldInstA;
          mInstB = &tempWorldInstB;
          mPrimitive.Set(mInstA, mInstB, mSpace, this);
          HandlePair();
        }
      }
    }
  }

  void Narrowphase::EnvOtherHandler() {
    //Transform b's bounding box into a's space so it can test against all a's submodels
    AABB localB = mB->GetCollider()->GetAABB().Transform(mInstA->GetWorldToModel());
    ModelInstance* envRoot = mInstA;
    mInstA->GetModel().GetBroadphase().QueryVolume(localB, *mBroadphaseContext);
    const Vec3Vec& tris = mInstA->GetModel().GetTriangles();

    ModelInstance tempInst(mTempTri, envRoot->GetModelToWorld(), envRoot->GetWorldToModel());

    for(ResultNode& result : mBroadphaseContext->mQueryResults) {
      size_t triIndex = reinterpret_cast<size_t>(result.mUserdata);
      const Vec3& a = tris[triIndex];
      Handle instHandle = *reinterpret_cast<const Handle*>(&a.w);

      tempInst.SetHandle(instHandle);
      mTempTri.SetTriangle(a, tris[triIndex + 1], tris[triIndex + 2]);
      mInstA = &tempInst;
      mPrimitive.Set(mInstA, mInstB, mSpace, this);
      HandlePair();
    }
  }

  void Narrowphase::OtherEnvHandler() {
    SwapAB();
    EnvOtherHandler();
  }

  void Narrowphase::EnvEnvHandler() {
    //Don't care if environments collide
  }

  void Narrowphase::EnvCompositeHandler() {
    ModelInstance* envRoot = mInstA;
    ModelInstance* compRoot = mInstB;
    //Local composite to local environment
    Transformer compToEnv = Transformer::Combined(compRoot->GetModelToWorld(), envRoot->GetWorldToModel());
    ModelInstance tempTriInst(mTempTri, envRoot->GetModelToWorld(), envRoot->GetWorldToModel());
    const Vec3Vec& tris = mInstA->GetModel().GetTriangles();

    auto& subInsts = compRoot->GetModel().GetSubmodelInstances();
    //Transform each submodel aabb into env local space and query broadphase, handle pairs of results
    for(size_t i = 0; i < subInsts.size(); ++i) {
      const ModelInstance& subInst = subInsts[i];
      AABB queryBox = subInst.GetAABB().Transform(compToEnv);

      envRoot->GetModel().GetBroadphase().QueryVolume(queryBox, *mBroadphaseContext);
      if(mBroadphaseContext->mQueryResults.empty())
        continue;

      ModelInstance tempSubInst = ModelInstance::Combined(*compRoot, subInst, subInst, compRoot->GetSubmodelInstHandle(i));
      mInstB = &tempSubInst;
      for(const ResultNode& result : mBroadphaseContext->mQueryResults) {
        size_t triIndex = reinterpret_cast<size_t>(result.mUserdata);
        const Vec3& a = tris[triIndex];
        Handle instHandle = *reinterpret_cast<const Handle*>(&a.w);

        tempTriInst.SetHandle(instHandle);
        mTempTri.SetTriangle(a, tris[triIndex + 1], tris[triIndex + 2]);
        mInstA = &tempTriInst;
        mPrimitive.Set(mInstA, mInstB, mSpace, this);
        HandlePair();
      }
    }
  }

  void Narrowphase::CompositeEnvHandler() {
    SwapAB();
    EnvCompositeHandler();
  }

  void Narrowphase::HandlePair() {
    (*this.*GetHandler(mInstA->GetModelType(), mInstB->GetModelType()))();
  }

  void Narrowphase::InitHandlers() {
    //Default to gjk, then call out special cases
    for(int i = 0; i < sHandlerRowCount; ++i)
      for(int j = 0; j < sHandlerRowCount; ++j)
        GetHandler(i, j) = &Narrowphase::GJKEPAHandler;

    for(int i = 0; i < sHandlerRowCount; ++i) {
      GetHandler(ModelType::Composite, i) = &Narrowphase::CompositeOtherHandler;
      GetHandler(i, ModelType::Composite) = &Narrowphase::OtherCompositeHandler;

      GetHandler(ModelType::Environment, i) = &Narrowphase::EnvOtherHandler;
      GetHandler(i, ModelType::Environment) = &Narrowphase::OtherEnvHandler;
    }
    GetHandler(ModelType::Composite, ModelType::Composite) = &Narrowphase::CompositeCompositeHandler;
    GetHandler(ModelType::Environment, ModelType::Environment) = &Narrowphase::EnvEnvHandler;

    GetHandler(ModelType::Environment, ModelType::Composite) = &Narrowphase::EnvCompositeHandler;
    GetHandler(ModelType::Composite, ModelType::Environment) = &Narrowphase::CompositeEnvHandler;

    GetHandler(ModelType::Sphere, ModelType::Sphere) = &Narrowphase::SphereSphereHandler;
  }

  CollisionHandler& Narrowphase::GetHandler(int modelTypeA, int modelTypeB) {
    return mHandlers[modelTypeA + sHandlerRowCount*modelTypeB];
  }

  void Narrowphase::SubmitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal) {
    float penetration = (worldB - worldA).Dot(normal);
    SubmitContact(worldA, worldB, normal, penetration);
  }

  void Narrowphase::SubmitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal, float penetration) {
    ContactObject ca(mA->GetTransform().WorldToModel(worldA), worldA);
    ContactObject cb(mB->GetTransform().WorldToModel(worldB), worldB);
    ContactPoint point(ca, cb, penetration);
    SubmitContact(point, normal);
  }

  void Narrowphase::SubmitContact(const ContactPoint& contact, const Vec3& normal) {
    Manifold* manifold = mSpace->GetManifold(*mA, *mB, *mInstA, *mInstB);
    if(manifold)
      manifold->AddContact(contact, normal);
  }

#ifdef SENABLED
  SupportPoint Narrowphase::SGetSupport(const Vec3& dir) {
    SFloats sDir = ToSVec3(dir);
    SAlign Vec3 supportA;
    SAlign Vec3 supportB;
    SVec3::Store(mInstA->SGetSupport(sDir), supportA);
    SVec3::Store(mInstB->SGetSupport(SVec3::Neg(sDir)), supportB);
    return SupportPoint(supportA, supportB);
  }

  SupportPoint Narrowphase::SGetSupport(SFloats dir, SFloats& resultSupport) {
    SAlign Vec3 supportA;
    SAlign Vec3 supportB;
    SAlign Vec3 support;
    SFloats sa = mInstA->SGetSupport(dir);
    SFloats sb = mInstB->SGetSupport(SVec3::Neg(dir));
    resultSupport = SSubAll(sa, sb);
    SVec3::Store(sa, supportA);
    SVec3::Store(sb, supportB);
    SVec3::Store(resultSupport, support);
    return SupportPoint(supportA, supportB, support);
  }

  SupportPoint Narrowphase::SGetSupport(SFloats dir) {
    SFloats unused;
    return SGetSupport(dir, unused);
  }

  bool Narrowphase::SGJK(void) {
    mSimplex.Initialize();
    //Arbitrary start direction
    SupportPoint support = SGetSupport(SVec3::UnitY);

    int iterationCap = 100;
    int iteration = 0;

    while(iteration++ < iterationCap) {
      mSimplex.Add(support, false);

      SFloats newDir = mSimplex.SSolve();

      if(mSimplex.ContainsOrigin())
        return true;
      else if(mSimplex.IsDegenerate() || mSimplex.SIsDegenerate())
        return false;

      support = SGetSupport(newDir);

      if(!mSimplex.SMakesProgress(SLoadAll(&support.mSupport.x), newDir))
        return false;
    }

    Interface::Log("GJK Iteration cap reached");
    return false;
  }

  Vec3 Narrowphase::SEPA(ContactPoint& result) {
    InitEPASimplex();

    int iterationCap = 100;
    int iteration = 0;
    const SFloats epaEpsilon = SLoadSplatFloats(sepaEpsilon);
    const SFloats setW = SLoadFloats(0.0f, 0.0f, 0.0f, 1.0f);

    while(iteration++ < iterationCap) {
      SupportTri* bestTri = GetClosestTri();
      SFloats triPlane = SLoadAll(&bestTri->m_halfPlane.x);
      SFloats newSupport;
      mVerts.push_back(SGetSupport(triPlane, newSupport));
      //Put a one in the w component so dot4 works
      newSupport = SOr(newSupport, setW);

      //If progress is below threshold, we're done
      if(SILessEqualLower(SVec3::Dot4(triPlane, newSupport), epaEpsilon))
        return StoreEPAResult(result, bestTri);

      SDeleteInteriorTris(newSupport);
      SReconstructTriangles(newSupport);
    }

    Interface::Log("EPA Iteration cap reached");
    return Vec3::Zero;
  }

  void Narrowphase::SDeleteInteriorTris(SFloats newPoint) {
    for(size_t i = 0; i < mTris.size();) {
      SupportTri& curTri = mTris[i];
      //If triangle is facing new vertex. Doesn't matter which point I choose, direction is similar enough
      if(SIGreaterLower(SVec3::Dot4(SLoadAll(&curTri.m_halfPlane.x), newPoint), SVec3::Zero)) {
        //Push edges on the list so they can later be used to construct new triangles with new vertex
        curTri.AddEdges(mEdges);
        SwapRemove(mTris, i);
      }
      else
        ++i;
    }
  }

  void Narrowphase::SReconstructTriangles(SFloats newSupport) {
    //Connect deleted triangles' edges to new face
    for(size_t i = 0; i < mEdges.size();) {
      auto& curEdge = mEdges[i];
      //See if an opposite edge to this one exists in the list, and if so, ignore both
      //I should use a hashmap for this search
      bool edgeRemoved = false;
      for(size_t j = i + 1; j < mEdges.size(); ++j) {
        auto& searchEdge = mEdges[j];
        if(searchEdge.m_from == curEdge.m_to &&
          searchEdge.m_to == curEdge.m_from) {
          SwapRemove(mEdges, j);
          SwapRemove(mEdges, i);
          edgeRemoved = true;
          break;
        }
      }
      if(edgeRemoved)
        continue;

      //Construct a triangle out of this edge with the new vertex
      SFloats a = SLoadAll(&mVerts[curEdge.m_from].mSupport.x);
      SFloats b = SLoadAll(&mVerts[curEdge.m_to].mSupport.x);

      SFloats normal = SVec3::SafeNormalized(SVec3::CCWTriangleNormal(a, b, newSupport));
      SFloats wTerm = SVec3::Neg(SVec3::Dot(a, normal));
      SAlign Vec3 plane;
      SAlign Vec3 w;
      SStoreLower(&w.x, wTerm);
      SStoreAll(&plane.x, normal);
      plane.w = w.x;

      mTris.push_back(SupportTri(curEdge.m_from, curEdge.m_to, mVerts.size() - 1, plane));
      ++i;
    }

    //Clear edge list for use next frame
    mEdges.clear();
  }
#else
  SupportPoint Narrowphase::SGetSupport(const Vec3&) { return SupportPoint(); }
#endif
}