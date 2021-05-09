#pragma once
#include "SyxMaterial.h"
#include "SyxModel.h"
#include "SyxSpace.h"
#include "SyxEvents.h"

namespace Syx {
  class CompositeModelParam;
  class DistanceOps;
  struct IMaterialRepository;
  struct IPhysicsObject;
  struct ISpace;
  class ModelParam;
  class NarrowphaseTest;
  struct Quat;
  struct Vec3;

  class PhysicsSystem {
  public:
    friend class NarrowphaseTest;

    static float sSimRate;

    PhysicsSystem(std::unique_ptr<IMaterialRepository> materials);
    PhysicsSystem();
    ~PhysicsSystem();

    void update(float dt);

    IMaterialRepository& getMaterialRepository();

    Handle addDistanceConstraint(DistanceOps ops);
    Handle addSphericalConstraint(SphericalOps ops);
    Handle addWeldConstraint(WeldOps ops);
    Handle addRevoluteConstraint(RevoluteOps ops);

    void removeConstraint(Handle space, Handle constraint);

    std::shared_ptr<ISpace> createSpace();

    Handle addPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space, const IMaterialHandle& material, std::shared_ptr<const Model> model);
    void removePhysicsObject(Handle space, Handle object);

    void setHasRigidbody(bool has, Handle space, Handle object);
    void setHasCollider(bool has, Handle space, Handle object);
    bool getHasRigidbody(Handle space, Handle object);
    bool getHasCollider(Handle space, Handle object);

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

    const std::string* getProfileReport(Handle space, const std::string& indent);
    const std::vector<ProfileResult>* getProfileHistory(Handle space);

  private:
    PhysicsObject* _getObject(Handle space, Handle object);
    Rigidbody* _getRigidbody(Handle space, Handle object);
    Collider* _getCollider(Handle space, Handle object);
    std::shared_ptr<Space> _getSpace(Handle space);

    std::unique_ptr<IMaterialRepository> mMaterials;
    //Lifetimes are managed by external strong references to ISpace
    std::vector<std::weak_ptr<Space>> mSpaces;
  };
}