#pragma once
#include "SyxAABBTree.h"
#include "SyxModelInstance.h"
#include "SyxModelParam.h"

namespace Syx {
  class Broadphase;
  class PhysicsSystem;
  struct Transform;
  struct Mat3;

  namespace ModelType {
    enum {
      Invalid,
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
    float mMass = 0.f;
    Vec3 mInertia;
    Vec3 mCenterOfMass;
  };

  class Model {
  public:
    friend class PhysicsSystem;

    static const Model NONE;

    ~Model();
    Model(int type, Vec3Vec points, Vec3Vec triangles, std::unique_ptr<Broadphase> broadphase, const AABB& aabb);
    Model(Vec3Vec points, Vec3Vec triangles, bool environment);
    Model(int type);

    //TODO: harden this so that the caller can't forget to initialize
    void initComposite(const CompositeModelParam& param);
    void initEnvironment();
    void init();

    void draw(const Transformer& toWorld) const;

    int getType(void) const { return mType; }
    const std::vector<ModelInstance, AlignmentAllocator<ModelInstance>>& getSubmodelInstances() const { return mInstances; }
    const Broadphase& getBroadphase() const { return *mBroadphase; }
    const Vec3Vec& getTriangles() const { return mTriangles; }

    const AABB& getAABB(void) const { return mAABB; }
    AABB getWorldAABB(const Transformer& toWorld) const;
    void setTriangle(const Vec3& a, const Vec3& b, const Vec3& c);

    MassInfo _computeMasses(const Vec3& scale) const;

    //Takes a point in model space and returns the model space support point.
    //Input should have been transformed to alter primary axis, as from here it will be assumed to be y
#ifdef SENABLED
    SFloats sGetSupport(SFloats dir) const;
#endif

    Vec3 getSupport(const Vec3& dir) const;
  private:

#ifdef SENABLED
    SFloats _sGetMeshSupport(SFloats dir) const;
    //Shapes -1 to 1
    SFloats _sGetCubeSupport(SFloats dir) const;
    SFloats _sGetSphereSupport(SFloats dir) const;
    SFloats _sGetCylinderSupport(SFloats dir) const;
    SFloats _sGetCapsuleSupport(SFloats dir) const;
    SFloats _sGetConeSupport(SFloats dir) const;
    SFloats _sGetTriSupport(SFloats dir) const;
#endif

    void _computeMasses(const Vec3& scale, float& mass, Vec3& centerOfMass, Mat3& inertia) const;
    void _computeMasses(const Vec3Vec& triangles, const Vec3& scale, float& mass, Vec3& centerOfMass, Mat3& inertia) const;
    //Offset all model points by the given vector
    void _offset(const Vec3& offset);

    Vec3 _getMeshSupport(const Vec3& dir) const;
    Vec3 _getCubeSupport(const Vec3& dir) const;
    Vec3 _getSphereSupport(const Vec3& dir) const;
    Vec3 _getCylinderSupport(const Vec3& dir) const;
    Vec3 _getCapsuleSupport(const Vec3& dir) const;
    Vec3 _getConeSupport(const Vec3& dir) const;
    Vec3 _getTriSupport(const Vec3& dir) const;

    AABB _getCompositeWorldAABB(const Transformer& toWorld) const;
    AABB _getEnvironmentWorldAABB(const Transformer& toWorld) const;
    AABB _getSphereWorldAABB(const Transformer& toWorld) const;
    AABB _getBaseWorldAABB(const Transformer& toWorld) const;

    MassInfo _computeMeshMasses(const Vec3& scale) const;
    MassInfo _computeCubeMasses(const Vec3& scale) const;
    MassInfo _computeSphereMasses(const Vec3& scale) const;
    MassInfo _computeCapsuleMasses(const Vec3& scale) const;
    MassInfo _computeCompositeMasses(const Vec3& scale) const;
    MassInfo _computeEnvironmentMasses(const Vec3& scale) const;

    Vec3Vec mPoints;
    Vec3Vec mTriangles;
    AABB mAABB;
    int mType = ModelType::Invalid;

    //Only for composite models
    std::vector<ModelInstance, AlignmentAllocator<ModelInstance>> mInstances;
    //Composite and environment
    std::unique_ptr<Broadphase> mBroadphase;
  };
}