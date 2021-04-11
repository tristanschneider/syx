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
  struct IPhysicsObject;

  class PhysicsSystem {
  public:
    friend class NarrowphaseTest;

    static float sSimRate;

    PhysicsSystem();

    void update(float dt);

    //Adding resources this way leaves the possibility of adding a resource layer on top of it that would call these,
    //which is probably what should happen to reduce dependencies
    Handle addModel(const ModelParam& newModel);
    Handle addCompositeModel(const CompositeModelParam& newModel);
    Handle addMaterial(const Material& newMaterial);

    Handle addDistanceConstraint(DistanceOps ops);
    Handle addSphericalConstraint(SphericalOps ops);
    Handle addWeldConstraint(WeldOps ops);
    Handle addRevoluteConstraint(RevoluteOps ops);

    void removeConstraint(Handle space, Handle constraint);

    void updateModel(Handle handle, const Model& updated);
    void updateMaterial(Handle handle, const Material& updated);

    const Model* getModel(Handle handle);
    const Material* getMaterial(Handle handle);

    void removeModel(Handle handle);
    void removeMaterial(Handle handle);

    Handle addSpace();
    void removeSpace(Handle handle);
    void clearSpace(Handle handle);

    Handle addPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space);
    void removePhysicsObject(Handle space, Handle object);

    void setHasRigidbody(bool has, Handle space, Handle object);
    void setHasCollider(bool has, Handle space, Handle object);
    bool getHasRigidbody(Handle space, Handle object);
    bool getHasCollider(Handle space, Handle object);

    void setObjectModel(Handle space, Handle object, Handle model);
    void setObjectMaterial(Handle space, Handle object, Handle material);

    void setVelocity(Handle space, Handle object, const Vec3& vel);
    Vec3 getVelocity(Handle space, Handle object);

    void setAngularVelocity(Handle space, Handle object, const Vec3& angVel);
    Vec3 getAngularVelocity(Handle space, Handle object);

    void setPosition(Handle space, Handle object, const Vec3& pos);
    Vec3 getPosition(Handle space, Handle object);

    void setRotation(Handle space, Handle object, const Quat& rot);
    Quat getRotation(Handle space, Handle object);

    void setScale(Handle space, Handle object, const Vec3& scale);
    Vec3 getScale(Handle space, Handle object);

    void getAABB(Handle space, Handle object, Vec3& min, Vec3& max);

    const EventListener<UpdateEvent>* getUpdateEvents(Handle space);

    CastResult lineCastAll(Handle space, const Vec3& start, const Vec3& end);

    Handle getCube(void) { return mCubeModel; }
    Handle getSphere(void) { return mSphereModel; }
    Handle getCylinder(void) { return mCylinderModel; }
    Handle getCapsule(void) { return mCapsuleModel; }
    Handle getCone(void) { return mConeModel; }
    Handle getDefaultMaterial(void) { return mDefaultMaterial; }

    const std::string* getProfileReport(Handle space, const std::string& indent);
    const std::vector<ProfileResult>* getProfileHistory(Handle space);

    //TODO: all of the handle nonsense above should be removed in favor of this interface
    //TODO: This interface probably makes sense as lifetime management. Right now it is not,
    //and the expiration of this does not affect the underlying physics object
    std::shared_ptr<IPhysicsObject> getPhysicsObject(Handle space, Handle object);

  private:
    Handle _addModel(const Model& newModel);

    PhysicsObject* _getObject(Handle space, Handle object);
    Rigidbody* _getRigidbody(Handle space, Handle object);
    Collider* _getCollider(Handle space, Handle object);

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