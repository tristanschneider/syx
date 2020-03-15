#include "Precompile.h"
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
    : mTempTri(ModelType::Triangle) {
    _initHandlers();
  }

  Narrowphase::~Narrowphase() {
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
    _initHandlers();
    return *this;
  }

  SupportPoint Narrowphase::_getSupport(const Vec3& dir) {
    if(gOptions.mSimdFlags & SyxOptions::SIMD::SupportPoints)
      return _sGetSupport(dir);

    Vec3 supportA = mInstA->getSupport(dir);
    SAlign Vec3 negDir = -dir;
    Vec3 supportB = mInstB->getSupport(negDir);
    return SupportPoint(supportA, supportB);
  }

  bool Narrowphase::_gjk(void) {
    mSimplex.initialize();
    //Arbitrary start direction
    SAlign Vec3 curDir(Vec3::UnitY);
    SupportPoint support = _getSupport(curDir);

    int iterationCap = 100;
    int iteration = 0;

    while(iteration++ < iterationCap) {
      mSimplex.add(support, false);
      curDir = mSimplex.solve();

      if(mSimplex.containsOrigin())
        return true;
      else if(mSimplex.isDegenerate())
        return false;

      support = _getSupport(curDir);

      // If we didn't pass over the origin, no collision. Only works when used for collision, not closest point or raycast
      // Dotting points looks weird, but it's actually vectors from origin to the supports, but that's subtraction by zero
      if(support.mSupport.dot(curDir) < 0.0f)
        return false;

#ifdef DEBUG_GJK
      bool drawIteration = (iteration == gOptions.mTest && gOptions.mDebugFlags & SyxOptions::Debug::DrawGJK);
      if(drawIteration) {
        DebugDrawer& d = DebugDrawer::get();
        d.setColor(1.0f, 0.0f, 0.0f);
        drawCube(support.mSupport, 0.1f);
        d.setColor(0.0f, 1.0f, 1.0f);
        drawCube(support.mPointA, 0.03f);
        drawCube(support.mPointB, 0.03f);
      }
#endif
    }

    Interface::log("GJK Iteration cap reached");
    return false;
  }

  Vec3 Narrowphase::_epa(ContactPoint& result) {
    _initEPASimplex();

    int iterationCap = 100;
    int iteration = 0;
    while(iteration++ < iterationCap) {
      SupportTri* bestTri = _getClosestTri();

      mVerts.push_back(_getSupport(bestTri->mHalfPlane));

      SupportPoint* newVert = &mVerts.back();
      float progress = bestTri->signedNormalDist(newVert->mSupport);

#ifdef DEBUG_EPA
      if(iteration == gOptions.mTest && gOptions.mDebugFlags & SyxOptions::Debug::DrawEPA) {
        DebugDrawer::get().setColor(0.0f, 1.0f, 0.0f);
        _drawEPA(bestTri);
      }
#endif

      if(progress <= sepaEpsilon)
        return _storeEPAResult(result, bestTri);

      _deleteInteriorTris(newVert->mSupport);
      _reconstructTriangles();
    }

    Interface::log("EPA Iteration cap reached");
    return Vec3::Zero;
  }

  Vec3 Narrowphase::_storeEPAResult(ContactPoint& result, SupportTri* bestTri) {
    Vec3 originOnTri;
    SupportPoint *a, *b, *c;
    Vec3 bary;
    bool first = true;

    if(!bestTri) {
      Interface::log("Problems! No resulting triangle in StoreEPAResult");
      return Vec3::Zero;
    }

    do {
      //We're given the first one, but after that we need to throw away the invalid one and get a new one
      if(!first) {
        int index = bestTri - &*mTris.begin();
        swapRemove(mTris, index);
        bestTri = _getClosestTri();
      }
      first = false;

      if(!bestTri) {
        Interface::log("Problems! No resulting triangle in StoreEPAResult");
        return Vec3::Zero;
      }

      a = &mVerts[bestTri->mVerts[0]];
      b = &mVerts[bestTri->mVerts[1]];
      c = &mVerts[bestTri->mVerts[2]];

      originOnTri = bestTri->project(Vec3::Zero);
      bary = pointToBarycentric(a->mSupport, b->mSupport, c->mSupport, originOnTri);
    }
    while(!validBarycentric(bary));

    result.mObjA.mStartingWorld = result.mObjA.mCurrentWorld = barycentricToPoint(a->mPointA, b->mPointA, c->mPointA, bary);
    result.mObjB.mStartingWorld = result.mObjB.mCurrentWorld = barycentricToPoint(a->mPointB, b->mPointB, c->mPointB, bary);

    result.mObjA.mModelPoint = mA->getTransform().worldToModel(result.mObjA.mStartingWorld);
    result.mObjB.mModelPoint = mB->getTransform().worldToModel(result.mObjB.mStartingWorld);
    result.mPenetration = (result.mObjB.mStartingWorld - result.mObjA.mStartingWorld).dot(-bestTri->mHalfPlane);
    result.mWarmContact = result.mWarmFriction[0] = result.mWarmFriction[1] = 0.0f;
    return -bestTri->mHalfPlane;
  }

  void Narrowphase::_initEPASimplex(void) {
    mSimplex.growToFourPoints(*this);

    mVerts.clear();
    mEdges.clear();
    mTris.clear();

    mVerts.push_back(mSimplex.getSupport(SupportID::A));
    mVerts.push_back(mSimplex.getSupport(SupportID::B));
    mVerts.push_back(mSimplex.getSupport(SupportID::C));
    mVerts.push_back(mSimplex.getSupport(SupportID::D));

    //bad, cbd, acd, abc
    mTris.push_back(SupportTri(SupportID::B, SupportID::A, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::C, SupportID::B, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::A, SupportID::C, SupportID::D, mVerts));
    mTris.push_back(SupportTri(SupportID::A, SupportID::B, SupportID::C, mVerts));
  }

  SupportTri* Narrowphase::_getClosestTri(void) {
    if(mTris.empty())
      return nullptr;

    //Always negating distance because it's signed and we know the origin is inside, meaning the distance is negative
    SupportTri* result = &mTris[0];
    float bestDist = std::numeric_limits<float>::max();
    for(size_t i = 0; i < mTris.size(); ++i) {
      float curDist = std::abs(mTris[i].originDist());
      if(curDist < bestDist) {
        result = &mTris[i];
        bestDist = curDist;
      }
    }
    return result;
  }

  void Narrowphase::_deleteInteriorTris(const Vec3& newPoint) {
    for(size_t i = 0; i < mTris.size();) {
      SupportTri& curTri = mTris[i];
      //If triangle is facing new vertex. Doesn't matter which point I choose, direction is similar enough
      if(curTri.signedNormalDist(newPoint) > 0.0f) {
        //Push edges on the list so they can later be used to construct new triangles with new vertex
        curTri.addEdges(mEdges);
        swapRemove(mTris, i);
      }
      else
        ++i;
    }
  }

  void Narrowphase::_reconstructTriangles(void) {
    //Connect deleted triangles' edges to new face
    for(size_t i = 0; i < mEdges.size();) {
      auto& curEdge = mEdges[i];
      //See if an opposite edge to this one exists in the list, and if so, ignore both
      //I should use a hashmap for this search
      bool edgeRemoved = false;
      for(size_t j = i + 1; j < mEdges.size(); ++j) {
        auto& searchEdge = mEdges[j];
        if(searchEdge.mFrom == curEdge.mTo &&
          searchEdge.mTo == curEdge.mFrom) {
          swapRemove(mEdges, j);
          swapRemove(mEdges, i);
          edgeRemoved = true;
          break;
        }
      }
      if(edgeRemoved)
        continue;

      //Construct a triangle out of this edge with the new vertex
      mTris.push_back(SupportTri(curEdge.mFrom, curEdge.mTo, mVerts.size() - 1, mVerts));
      ++i;
    }

    //Clear edge list for use next frame
    mEdges.clear();
  }

  void Narrowphase::_gjkEPAHandler(void) {
    bool collision;
    if(gOptions.mSimdFlags & SyxOptions::SIMD::GJK)
      collision = _sGJK();
    else
      collision = _gjk();

    if(collision) {
      if(gOptions.mDebugFlags & SyxOptions::Debug::DrawCollidingPairs)
        DebugDrawer::get().drawLine(mA->getTransform().mPos, mB->getTransform().mPos);

      ContactPoint resultPoint;
      Vec3 normal;
      if(gOptions.mSimdFlags & SyxOptions::SIMD::EPA)
        normal = _sEPA(resultPoint);
      else
        normal = _epa(resultPoint);

      if(normal != Vec3::Zero)
        _submitContact(resultPoint, normal);
      else
        Interface::log("Invalid contact normal");
    }
  }

  void Narrowphase::processPairQuery(const std::vector<std::pair<ResultNode, ResultNode>>& pairs, Space& space) {
    mSpace = &space;
    for(auto& pair : pairs) {
      mA = reinterpret_cast<PhysicsObject*>(pair.first.mUserdata);
      mB = reinterpret_cast<PhysicsObject*>(pair.second.mUserdata);
      //Eventually broadphase shouldn't even return these in the query
      if(mA->getAsleep() && mB->getAsleep())
        continue;
      //Ensure always consistent ordering of a and b
      if(mA->getHandle() < mB->getHandle())
        std::swap(mA, mB);

      //Update these values in case the primitive narrowphase is used for this
      mPrimitive.set(mInstA, mInstB, mSpace, this);
      mInstA = &mA->getCollider()->getModelInstance();
      mInstB = &mB->getCollider()->getModelInstance();

      _handlePair();
    }
  }

  void Narrowphase::_swapAB() {
    std::swap(mA, mB);
    std::swap(mInstA, mInstB);
    mPrimitive.set(mInstA, mInstB, mSpace, this);
  }

  BroadphaseContext& Narrowphase::_getBroadphaseContext(const Broadphase& broadphase) {
    // TODO: better solution for re-use
    mBroadphaseContext = broadphase.createHitContext();
    return *mBroadphaseContext;
  }

  void Narrowphase::processRayQuery(const std::vector<ResultNode>& /*objs*/, const Vec3& /*start*/, const Vec3& /*end*/, Space& /*space*/) {

  }

  void Narrowphase::processVolumeQuery(const std::vector<ResultNode>& /*objs*/, const BoundingVolume& /*volume*/, Space& /*space*/) {

  }

  void Narrowphase::_drawEPA(SupportTri* bestTri) {
    DebugDrawer& d = DebugDrawer::get();
    bestTri = bestTri;
    d.setColor(1.0f, 0.0f, 0.0f);
    for(auto& tri : mTris) {
      Vec3 a = mVerts[tri.mVerts[0]].mSupport;
      Vec3 b = mVerts[tri.mVerts[1]].mSupport;
      Vec3 c = mVerts[tri.mVerts[2]].mSupport;
      drawTriangle(a, b, c, true);
    }

    d.setColor(1.0f, 0.0f, 1.0f);
    drawSphere(Vec3::Zero, 0.03f);
    d.drawPoint(bestTri->project(Vec3::Zero), 0.01f);
  }

  void Narrowphase::_sphereSphereHandler() {
    mPrimitive.sphereSphere();
  }

  void Narrowphase::_compositeOtherHandler() {
    //Transform b's bounding box into a's space so it can test against all a's submodels
    AABB localB = mB->getCollider()->getAABB().transform(mInstA->getWorldToModel());
    ModelInstance* compositeRoot = mInstA;

    //This would be the place to have a local space broadphase for composite models, but we're doing them all now
    auto& subInsts = compositeRoot->getModel().getSubmodelInstances();
    for(size_t i = 0; i < subInsts.size(); ++i) {
      const ModelInstance& subInst = subInsts[i];
      if(localB.overlapping(subInst.getAABB())) {
        //Construct a model instance with the combined transforms so there aren't extra transforms when getting supports
        ModelInstance tempWorldInst = ModelInstance::combined(*compositeRoot, subInst, subInst, compositeRoot->getSubmodelInstHandle(i));
        mInstA = &tempWorldInst;
        mPrimitive.set(mInstA, mInstB, mSpace, this);
        _handlePair();
      }
    }
  }

  void Narrowphase::_otherCompositeHandler() {
    _swapAB();
    _compositeOtherHandler();
  }

  void Narrowphase::_compositeCompositeHandler() {
    //We'll calculate in the space of the composite object with more submodels
    if(mInstA->getModel().getSubmodelInstances().size() < mInstB->getModel().getSubmodelInstances().size()) {
      _swapAB();
    }

    Transformer localBToLocalA = Transformer::combined(mInstB->getModelToWorld(), mInstA->getWorldToModel());
    ModelInstance* rootA = mInstA;
    ModelInstance* rootB = mInstB;

    //N^2 composite to composite. Needs broadphase!
    auto& subInstsB = rootB->getModel().getSubmodelInstances();
    for(size_t i = 0; i < subInstsB.size(); ++i) {
      const ModelInstance& subInstB = subInstsB[i];
      AABB bInA = subInstB.getAABB().transform(localBToLocalA);
      ModelInstance tempWorldInstB = ModelInstance::combined(*rootB, subInstB, subInstB, rootB->getSubmodelInstHandle(i));

      auto& subInstsA = rootA->getModel().getSubmodelInstances();
      for(size_t j = 0; j < subInstsA.size(); ++j) {
        const ModelInstance& subInstA = subInstsA[j];
        if(subInstA.getAABB().overlapping(bInA)) {
          ModelInstance tempWorldInstA = ModelInstance::combined(*rootA, subInstA, subInstA, rootA->getSubmodelInstHandle(j));
          mInstA = &tempWorldInstA;
          mInstB = &tempWorldInstB;
          mPrimitive.set(mInstA, mInstB, mSpace, this);
          _handlePair();
        }
      }
    }
  }

  void Narrowphase::_envOtherHandler() {
    //Transform b's bounding box into a's space so it can test against all a's submodels
    AABB localB = mB->getCollider()->getAABB().transform(mInstA->getWorldToModel());
    ModelInstance* envRoot = mInstA;
    _getBroadphaseContext(mInstA->getModel().getBroadphase()).queryVolume(localB);
    const Vec3Vec& tris = mInstA->getModel().getTriangles();

    ModelInstance tempInst(mTempTri, envRoot->getModelToWorld(), envRoot->getWorldToModel());

    for(const ResultNode& result : mBroadphaseContext->get()) {
      size_t triIndex = reinterpret_cast<size_t>(result.mUserdata);
      const Vec3& a = tris[triIndex];
      Handle instHandle = *reinterpret_cast<const Handle*>(&a.w);

      tempInst.setHandle(instHandle);
      mTempTri.setTriangle(a, tris[triIndex + 1], tris[triIndex + 2]);
      mInstA = &tempInst;
      mPrimitive.set(mInstA, mInstB, mSpace, this);
      _handlePair();
    }
  }

  void Narrowphase::_otherEnvHandler() {
    _swapAB();
    _envOtherHandler();
  }

  void Narrowphase::_envEnvHandler() {
    //Don't care if environments collide
  }

  void Narrowphase::_envCompositeHandler() {
    ModelInstance* envRoot = mInstA;
    ModelInstance* compRoot = mInstB;
    //Local composite to local environment
    Transformer compToEnv = Transformer::combined(compRoot->getModelToWorld(), envRoot->getWorldToModel());
    ModelInstance tempTriInst(mTempTri, envRoot->getModelToWorld(), envRoot->getWorldToModel());
    const Vec3Vec& tris = mInstA->getModel().getTriangles();

    auto& subInsts = compRoot->getModel().getSubmodelInstances();
    //Transform each submodel aabb into env local space and query broadphase, handle pairs of results
    auto& context = _getBroadphaseContext(envRoot->getModel().getBroadphase());
    for(size_t i = 0; i < subInsts.size(); ++i) {
      const ModelInstance& subInst = subInsts[i];
      AABB queryBox = subInst.getAABB().transform(compToEnv);

      _getBroadphaseContext(envRoot->getModel().getBroadphase()).queryVolume(queryBox);
      if(context.get().empty()) {
        continue;
      }

      ModelInstance tempSubInst = ModelInstance::combined(*compRoot, subInst, subInst, compRoot->getSubmodelInstHandle(i));
      mInstB = &tempSubInst;
      for(const ResultNode& result : context.get()) {
        size_t triIndex = reinterpret_cast<size_t>(result.mUserdata);
        const Vec3& a = tris[triIndex];
        Handle instHandle = *reinterpret_cast<const Handle*>(&a.w);

        tempTriInst.setHandle(instHandle);
        mTempTri.setTriangle(a, tris[triIndex + 1], tris[triIndex + 2]);
        mInstA = &tempTriInst;
        mPrimitive.set(mInstA, mInstB, mSpace, this);
        _handlePair();
      }
    }
  }

  void Narrowphase::_compositeEnvHandler() {
    _swapAB();
    _envCompositeHandler();
  }

  void Narrowphase::_handlePair() {
    (*this.*_getHandler(mInstA->getModelType(), mInstB->getModelType()))();
  }

  void Narrowphase::_initHandlers() {
    //Default to gjk, then call out special cases
    for(int i = 0; i < sHandlerRowCount; ++i)
      for(int j = 0; j < sHandlerRowCount; ++j)
        _getHandler(i, j) = &Narrowphase::_gjkEPAHandler;

    for(int i = 0; i < sHandlerRowCount; ++i) {
      _getHandler(ModelType::Composite, i) = &Narrowphase::_compositeOtherHandler;
      _getHandler(i, ModelType::Composite) = &Narrowphase::_otherCompositeHandler;

      _getHandler(ModelType::Environment, i) = &Narrowphase::_envOtherHandler;
      _getHandler(i, ModelType::Environment) = &Narrowphase::_otherEnvHandler;
    }
    _getHandler(ModelType::Composite, ModelType::Composite) = &Narrowphase::_compositeCompositeHandler;
    _getHandler(ModelType::Environment, ModelType::Environment) = &Narrowphase::_envEnvHandler;

    _getHandler(ModelType::Environment, ModelType::Composite) = &Narrowphase::_envCompositeHandler;
    _getHandler(ModelType::Composite, ModelType::Environment) = &Narrowphase::_compositeEnvHandler;

    _getHandler(ModelType::Sphere, ModelType::Sphere) = &Narrowphase::_sphereSphereHandler;
  }

  CollisionHandler& Narrowphase::_getHandler(int modelTypeA, int modelTypeB) {
    return mHandlers[modelTypeA + sHandlerRowCount*modelTypeB];
  }

  void Narrowphase::_submitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal) {
    float penetration = (worldB - worldA).dot(normal);
    _submitContact(worldA, worldB, normal, penetration);
  }

  void Narrowphase::_submitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal, float penetration) {
    ContactObject ca(mA->getTransform().worldToModel(worldA), worldA);
    ContactObject cb(mB->getTransform().worldToModel(worldB), worldB);
    ContactPoint point(ca, cb, penetration);
    _submitContact(point, normal);
  }

  void Narrowphase::_submitContact(const ContactPoint& contact, const Vec3& normal) {
    Manifold* manifold = mSpace->getManifold(*mA, *mB, *mInstA, *mInstB);
    if(manifold)
      manifold->addContact(contact, normal);
  }

#ifdef SENABLED
  SupportPoint Narrowphase::_sGetSupport(const Vec3& dir) {
    SFloats sDir = toSVec3(dir);
    SAlign Vec3 supportA;
    SAlign Vec3 supportB;
    SVec3::store(mInstA->sGetSupport(sDir), supportA);
    SVec3::store(mInstB->sGetSupport(SVec3::neg(sDir)), supportB);
    return SupportPoint(supportA, supportB);
  }

  SupportPoint Narrowphase::_sGetSupport(SFloats dir, SFloats& resultSupport) {
    SAlign Vec3 supportA;
    SAlign Vec3 supportB;
    SAlign Vec3 support;
    SFloats sa = mInstA->sGetSupport(dir);
    SFloats sb = mInstB->sGetSupport(SVec3::neg(dir));
    resultSupport = SSubAll(sa, sb);
    SVec3::store(sa, supportA);
    SVec3::store(sb, supportB);
    SVec3::store(resultSupport, support);
    return SupportPoint(supportA, supportB, support);
  }

  SupportPoint Narrowphase::_sGetSupport(SFloats dir) {
    SFloats unused;
    return _sGetSupport(dir, unused);
  }

  bool Narrowphase::_sGJK(void) {
    mSimplex.initialize();
    //Arbitrary start direction
    SupportPoint support = _sGetSupport(SVec3::UnitY);

    int iterationCap = 100;
    int iteration = 0;

    while(iteration++ < iterationCap) {
      mSimplex.add(support, false);

      SFloats newDir = mSimplex.sSolve();

      if(mSimplex.containsOrigin())
        return true;
      else if(mSimplex.isDegenerate() || mSimplex.sIsDegenerate())
        return false;

      support = _sGetSupport(newDir);

      if(!mSimplex.sMakesProgress(SLoadAll(&support.mSupport.x), newDir))
        return false;
    }

    Interface::log("GJK Iteration cap reached");
    return false;
  }

  Vec3 Narrowphase::_sEPA(ContactPoint& result) {
    _initEPASimplex();

    int iterationCap = 100;
    int iteration = 0;
    const SFloats epaEpsilon = sLoadSplatFloats(sepaEpsilon);
    const SFloats setW = sLoadFloats(0.0f, 0.0f, 0.0f, 1.0f);

    while(iteration++ < iterationCap) {
      SupportTri* bestTri = _getClosestTri();
      SFloats triPlane = SLoadAll(&bestTri->mHalfPlane.x);
      SFloats newSupport;
      mVerts.push_back(_sGetSupport(triPlane, newSupport));
      //Put a one in the w component so dot4 works
      newSupport = SOr(newSupport, setW);

      //If progress is below threshold, we're done
      if(SILessEqualLower(SVec3::dot4(triPlane, newSupport), epaEpsilon))
        return _storeEPAResult(result, bestTri);

      _sDeleteInteriorTris(newSupport);
      _sReconstructTriangles(newSupport);
    }

    Interface::log("EPA Iteration cap reached");
    return Vec3::Zero;
  }

  void Narrowphase::_sDeleteInteriorTris(SFloats newPoint) {
    for(size_t i = 0; i < mTris.size();) {
      SupportTri& curTri = mTris[i];
      //If triangle is facing new vertex. Doesn't matter which point I choose, direction is similar enough
      if(SIGreaterLower(SVec3::dot4(SLoadAll(&curTri.mHalfPlane.x), newPoint), SVec3::Zero)) {
        //Push edges on the list so they can later be used to construct new triangles with new vertex
        curTri.addEdges(mEdges);
        swapRemove(mTris, i);
      }
      else
        ++i;
    }
  }

  void Narrowphase::_sReconstructTriangles(SFloats newSupport) {
    //Connect deleted triangles' edges to new face
    for(size_t i = 0; i < mEdges.size();) {
      auto& curEdge = mEdges[i];
      //See if an opposite edge to this one exists in the list, and if so, ignore both
      //I should use a hashmap for this search
      bool edgeRemoved = false;
      for(size_t j = i + 1; j < mEdges.size(); ++j) {
        auto& searchEdge = mEdges[j];
        if(searchEdge.mFrom == curEdge.mTo &&
          searchEdge.mTo == curEdge.mFrom) {
          swapRemove(mEdges, j);
          swapRemove(mEdges, i);
          edgeRemoved = true;
          break;
        }
      }
      if(edgeRemoved)
        continue;

      //Construct a triangle out of this edge with the new vertex
      SFloats a = SLoadAll(&mVerts[curEdge.mFrom].mSupport.x);
      SFloats b = SLoadAll(&mVerts[curEdge.mTo].mSupport.x);

      SFloats normal = SVec3::safeNormalized(SVec3::ccwTriangleNormal(a, b, newSupport));
      SFloats wTerm = SVec3::neg(SVec3::dot(a, normal));
      SAlign Vec3 plane;
      SAlign Vec3 w;
      SStoreLower(&w.x, wTerm);
      SStoreAll(&plane.x, normal);
      plane.w = w.x;

      mTris.push_back(SupportTri(curEdge.mFrom, curEdge.mTo, mVerts.size() - 1, plane));
      ++i;
    }

    //Clear edge list for use next frame
    mEdges.clear();
  }
#else
  SupportPoint Narrowphase::_sGetSupport(const Vec3&) { return SupportPoint(); }
#endif
}