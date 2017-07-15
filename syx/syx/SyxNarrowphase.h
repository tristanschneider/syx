
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

    void ProcessPairQuery(const BroadPairs& pairs, Space& space);
    void ProcessRayQuery(const BroadResults& objs, const Vector3& start, const Vector3& end, Space& space);
    void ProcessVolumeQuery(const BroadResults& objs, const BoundingVolume& volume, Space& space);

  private:
    //Once m_a, m_b, m_space are set, this is called to find the right pair function and call it, which takes care of everything
    void HandlePair(void);
    CollisionHandler& GetHandler(int modelTypeA, int modelTypeB);

    void GJKEPAHandler();
    void SphereSphereHandler();
    void CompositeOtherHandler();
    void OtherCompositeHandler();
    void CompositeCompositeHandler();
    void EnvOtherHandler();
    void OtherEnvHandler();
    void EnvEnvHandler();
    void EnvCompositeHandler();
    void CompositeEnvHandler();

    void InitHandlers(void);

    bool GJK(void);

    Vector3 EPA(ContactPoint& result);
    void InitEPASimplex(void);
    SupportTri* GetClosestTri(void);
    void DeleteInteriorTris(const Vector3& newPoint);
    void ReconstructTriangles(void);
    Vector3 StoreEPAResult(ContactPoint& result, SupportTri* bestTri);

    void SubmitContact(const Vector3& worldA, const Vector3& worldB, const Vector3& normal);
    void SubmitContact(const Vector3& worldA, const Vector3& worldB, const Vector3& normal, float penetration);
    void SubmitContact(const ContactPoint& contact, const Vector3& normal);

    SupportPoint GetSupport(const Vector3& dir);

    SupportPoint SGetSupport(const Vector3& dir);
    SupportPoint SGetSupport(SFloats dir);
    SupportPoint SGetSupport(SFloats dir, SFloats& resultSupport);
    void SDeleteInteriorTris(SFloats newPoint);
    void SReconstructTriangles(SFloats newSupport);

    bool SGJK(void);
    Vector3 SEPA(ContactPoint& result);

    void DrawEPA(SupportTri* bestTri);

    void SwapAB();

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
    BroadphaseContext* mBroadphaseContext;
    Model mTempTri;

    static const int sHandlerCount = ModelType::Count*ModelType::Count;
    static const int sHandlerRowCount = ModelType::Count;
  };
}