#pragma once
#include "SyxAABB.h"
#include "SyxHandles.h"
#include <vector>
#include "SyxHandleMap.h"
#include "SyxModelParam.h"
#include "SyxModelInstance.h"
#include "SyxAABBTree.h"

namespace Syx {
  class PhysicsSystem;
  struct Transform;
  struct Matrix3;

  typedef AABBTree ModelBroadphase;

  namespace ModelType {
    enum {
      Mesh,
      Cube,
      Sphere,
      Cylinder,
      Capsule,
      Cone,
      Composite,
      Environment,
      Triangle,
      Count
    };
  };

  struct MassInfo {
    float mMass;
    Vector3 mInertia;
    Vector3 mCenterOfMass;
  };

  class Model {
  public:
    friend class PhysicsSystem;
    DeclareHandleMapNode(Model);

    Model(void) {}
    Model(const Vector3Vec& points, const Vector3Vec& triangles, bool environment);
    Model(int type);

    Model(Handle handle): mType(ModelType::Mesh), mHandle(handle) {}

    // Links pointers to local objects, so this needs to be at its final location
    void InitComposite(const CompositeModelParam& param, const HandleMap<Model>& modelMap);
    void InitEnvironment();

    void Draw(const Transformer& toWorld) const;

    bool operator<(Handle handle) { return mHandle < handle; }
    bool operator==(Handle handle) { return mHandle == handle; }

    Handle GetHandle(void) { return mHandle; }
    int GetType(void) const { return mType; }
    const std::vector<ModelInstance, AlignmentAllocator<ModelInstance>>& GetSubmodelInstances() const { return mInstances; }
    const Broadphase& GetBroadphase() const { return mBroadphase; }
    const Vector3Vec& GetTriangles() const { return mTriangles; }

    const AABB& GetAABB(void) const { return mAABB; }
    AABB GetWorldAABB(const Transformer& toWorld) const;
    void SetTriangle(const Vector3& a, const Vector3& b, const Vector3& c);

    MassInfo ComputeMasses(const Vector3& scale) const;

    //Takes a point in model space and returns the model space support point.
    //Input should have been transformed to alter primary axis, as from here it will be assumed to be y
#ifdef SENABLED
    SFloats SGetSupport(SFloats dir) const;
#endif

    Vector3 GetSupport(const Vector3& dir) const;
  private:

#ifdef SENABLED
    SFloats SGetMeshSupport(SFloats dir) const;
    //Shapes -1 to 1
    SFloats SGetCubeSupport(SFloats dir) const;
    SFloats SGetSphereSupport(SFloats dir) const;
    SFloats SGetCylinderSupport(SFloats dir) const;
    SFloats SGetCapsuleSupport(SFloats dir) const;
    SFloats SGetConeSupport(SFloats dir) const;
    SFloats SGetTriSupport(SFloats dir) const;
#endif

    void ComputeMasses(const Vector3& scale, float& mass, Vector3& centerOfMass, Matrix3& inertia) const;
    void ComputeMasses(const Vector3Vec& triangles, const Vector3& scale, float& mass, Vector3& centerOfMass, Matrix3& inertia) const;
    //Offset all model points by the given vector
    void Offset(const Vector3& offset);

    Vector3 GetMeshSupport(const Vector3& dir) const;
    Vector3 GetCubeSupport(const Vector3& dir) const;
    Vector3 GetSphereSupport(const Vector3& dir) const;
    Vector3 GetCylinderSupport(const Vector3& dir) const;
    Vector3 GetCapsuleSupport(const Vector3& dir) const;
    Vector3 GetConeSupport(const Vector3& dir) const;
    Vector3 GetTriSupport(const Vector3& dir) const;

    AABB GetCompositeWorldAABB(const Transformer& toWorld) const;
    AABB GetEnvironmentWorldAABB(const Transformer& toWorld) const;
    AABB GetSphereWorldAABB(const Transformer& toWorld) const;
    AABB GetBaseWorldAABB(const Transformer& toWorld) const;

    MassInfo ComputeMeshMasses(const Vector3& scale) const;
    MassInfo ComputeCubeMasses(const Vector3& scale) const;
    MassInfo ComputeSphereMasses(const Vector3& scale) const;
    MassInfo ComputeCapsuleMasses(const Vector3& scale) const;
    MassInfo ComputeCompositeMasses(const Vector3& scale) const;
    MassInfo ComputeEnvironmentMasses(const Vector3& scale) const;

    Vector3Vec mPoints;
    Vector3Vec mTriangles;
    AABB mAABB;
    int mType;
    Handle mHandle;

    //Only for composite models
    std::vector<ModelInstance, AlignmentAllocator<ModelInstance>> mInstances;
    std::vector<Model, AlignmentAllocator<ModelInstance>> mSubmodels;
    //Composite and environment
    ModelBroadphase mBroadphase;
  };
}