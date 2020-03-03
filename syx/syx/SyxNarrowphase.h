
#pragma once
#include "SyxBroadphase.h"
#include "SyxSimplex.h"
#include "SyxSupportTri.h"
#include "SyxAlignmentAllocator.h"
#include "SyxModel.h"
#include "SyxPrimitiveNarrowphase.h"

namespace Syx {
  class Space;
  class PhysicsObject;
  class Simplex;
  class DebugSimplex;
  struct ContactPoint;
  class NarrowphaseTest;

  class Narrowphase;
  typedef void(Narrowphase::*CollisionHandler)(void);

  class Narrowphase {
  public:
    friend Simplex;
    friend DebugSimplex;
    friend PrimitiveNarrowphase;
    friend NarrowphaseTest;

    Narrowphase();
    Narrowphase(const Narrowphase& other);
    ~Narrowphase();

    Narrowphase& operator=(const Narrowphase& rhs);

    void processPairQuery(const std::vector<std::pair<ResultNode, ResultNode>>& pairs, Space& space);
    void processRayQuery(const std::vector<ResultNode>& objs, const Vec3& start, const Vec3& end, Space& space);
    void processVolumeQuery(const std::vector<ResultNode>& objs, const BoundingVolume& volume, Space& space);

  private:
    //Once m_a, m_b, m_space are set, this is called to find the right pair function and call it, which takes care of everything
    void _handlePair(void);
    CollisionHandler& _getHandler(int modelTypeA, int modelTypeB);

    void _gjkEPAHandler();
    void _sphereSphereHandler();
    void _compositeOtherHandler();
    void _otherCompositeHandler();
    void _compositeCompositeHandler();
    void _envOtherHandler();
    void _otherEnvHandler();
    void _envEnvHandler();
    void _envCompositeHandler();
    void _compositeEnvHandler();

    void _initHandlers(void);

    bool _gjk(void);

    Vec3 _epa(ContactPoint& result);
    void _initEPASimplex(void);
    SupportTri* _getClosestTri(void);
    void _deleteInteriorTris(const Vec3& newPoint);
    void _reconstructTriangles(void);
    Vec3 _storeEPAResult(ContactPoint& result, SupportTri* bestTri);

    void _submitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal);
    void _submitContact(const Vec3& worldA, const Vec3& worldB, const Vec3& normal, float penetration);
    void _submitContact(const ContactPoint& contact, const Vec3& normal);

    SupportPoint _getSupport(const Vec3& dir);

    SupportPoint _sGetSupport(const Vec3& dir);
    SupportPoint _sGetSupport(SFloats dir);
    SupportPoint _sGetSupport(SFloats dir, SFloats& resultSupport);
    void _sDeleteInteriorTris(SFloats newPoint);
    void _sReconstructTriangles(SFloats newSupport);

    bool _sGJK(void);
    Vec3 _sEPA(ContactPoint& result);

    void _drawEPA(SupportTri* bestTri);

    void _swapAB();
    //Returns a cached context if available, otherwise creates a new one
    BroadphaseContext& _getBroadphaseContext(const Broadphase& broadphase);

    static float sepaEpsilon;

    Simplex mSimplex;
    PhysicsObject* mA;
    PhysicsObject* mB;
    ModelInstance* mInstA;
    ModelInstance* mInstB;
    Space* mSpace;
    //2D array of member function pointers for handling all combinations of model types
    CollisionHandler mHandlers[ModelType::Count*ModelType::Count];
    PrimitiveNarrowphase mPrimitive;

    std::vector<SupportTri, AlignmentAllocator<SupportTri>> mTris;
    std::vector<SupportEdge, AlignmentAllocator<SupportEdge>> mEdges;
    std::vector<SupportPoint, AlignmentAllocator<SupportPoint>> mVerts;
    std::unique_ptr<BroadphaseContext> mBroadphaseContext;
    Model mTempTri;

    static const int sHandlerCount = ModelType::Count*ModelType::Count;
    static const int sHandlerRowCount = ModelType::Count;
  };
}