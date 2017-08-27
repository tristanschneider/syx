#pragma once
#include "SyxMaterial.h"
#include "SyxModel.h"
#include "SyxSpace.h"
#include "SyxEvents.h"

namespace Syx {
  struct Vec3;
  struct Quat;
  class ModelParam;
  class CompositeModelParam;
  class NarrowphaseTest;
  class DistanceOps;

  class PhysicsSystem {
  public:
    friend class NarrowphaseTest;

    static float sSimRate;

    PhysicsSystem();

    void Update(float dt);

    //Adding resources this way leaves the possibility of adding a resource layer on top of it that would call these,
    //which is probably what should happen to reduce dependencies
    Handle AddModel(const ModelParam& newModel);
    Handle AddCompositeModel(const CompositeModelParam& newModel);
    Handle AddMaterial(const Material& newMaterial);

    Handle AddDistanceConstraint(DistanceOps ops);
    Handle AddSphericalConstraint(SphericalOps ops);
    Handle AddWeldConstraint(WeldOps ops);
    Handle AddRevoluteConstraint(RevoluteOps ops);

    void RemoveConstraint(Handle space, Handle constraint);

    void UpdateModel(Handle handle, const Model& updated);
    void UpdateMaterial(Handle handle, const Material& updated);

    const Model* GetModel(Handle handle);
    const Material* GetMaterial(Handle handle);

    void RemoveModel(Handle handle);
    void RemoveMaterial(Handle handle);

    Handle AddSpace(void);
    void RemoveSpace(Handle handle);
    void ClearSpace(Handle handle);

    Handle AddPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space);
    void RemovePhysicsObject(Handle space, Handle object);

    void SetHasRigidbody(bool has, Handle space, Handle object);
    void SetHasCollider(bool has, Handle space, Handle object);
    bool GetHasRigidbody(Handle space, Handle object);
    bool GetHasCollider(Handle space, Handle object);

    void SetObjectModel(Handle space, Handle object, Handle model);
    void SetObjectMaterial(Handle space, Handle object, Handle material);

    void SetVelocity(Handle space, Handle object, const Vec3& vel);
    Vec3 GetVelocity(Handle space, Handle object);

    void SetAngularVelocity(Handle space, Handle object, const Vec3& angVel);
    Vec3 GetAngularVelocity(Handle space, Handle object);

    void SetPosition(Handle space, Handle object, const Vec3& pos);
    Vec3 GetPosition(Handle space, Handle object);

    void SetRotation(Handle space, Handle object, const Quat& rot);
    Quat GetRotation(Handle space, Handle object);

    void SetScale(Handle space, Handle object, const Vec3& scale);
    Vec3 GetScale(Handle space, Handle object);

    void GetAABB(Handle space, Handle object, Vec3& min, Vec3& max);

    const EventListener<UpdateEvent>* getUpdateEvents(Handle space);

    CastResult LineCastAll(Handle space, const Vec3& start, const Vec3& end);

    Handle GetCube(void) { return mCubeModel; }
    Handle GetSphere(void) { return mSphereModel; }
    Handle GetCylinder(void) { return mCylinderModel; }
    Handle GetCapsule(void) { return mCapsuleModel; }
    Handle GetCone(void) { return mConeModel; }
    Handle GetDefaultMaterial(void) { return mDefaultMaterial; }

    const std::string* GetProfileReport(Handle space, const std::string& indent);
    const std::vector<ProfileResult>* GetProfileHistory(Handle space);

  private:
    Handle AddModel(const Model& newModel);

    PhysicsObject* GetObject(Handle space, Handle object);
    Rigidbody* GetRigidbody(Handle space, Handle object);
    Collider* GetCollider(Handle space, Handle object);

    HandleMap<Material> mMaterials;
    HandleMap<Model> mModels;
    HandleMap<Space> mSpaces;

    Handle mCubeModel;
    Handle mSphereModel;
    Handle mCylinderModel;
    Handle mCapsuleModel;
    Handle mConeModel;
    Handle mDefaultMaterial;
  };
}