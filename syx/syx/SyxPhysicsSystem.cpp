#include "Precompile.h"
#include "SyxPhysicsSystem.h"

#include "SyxIPhysicsObject.h"
#include "SyxModelParam.h"

namespace Syx {
  SyxOptions gOptions;

  float PhysicsSystem::sSimRate = 1.0f/60.0f;

  PhysicsSystem::PhysicsSystem() {
    mCubeModel = _addModel(Model(ModelType::Cube));
    mSphereModel = _addModel(Model(ModelType::Sphere));
    mCylinderModel = _addModel(Model(ModelType::Cylinder));
    mCapsuleModel = _addModel(Model(ModelType::Capsule));
    mConeModel = _addModel(Model(ModelType::Cone));
  }

  void PhysicsSystem::update(float dt) {
    static float accumulated = 0.0f;
    int maxUpdates = 5;
    int updates = 0;
    DebugDrawer& drawer = DebugDrawer::get();

    gOptions = Interface::getOptions();

    accumulated += dt;
    while(accumulated >= sSimRate && updates++ < maxUpdates) {
      drawer.clear();
      accumulated -= sSimRate;
      for(auto space : mSpaces) {
        if(auto strongSpace = space.lock()) {
          strongSpace->update(sSimRate);
        }
      }
    }

    //Subtract off the rest of the rest of the time if we hit the update cap to keep time from building up
    while(accumulated >= sSimRate)
      accumulated -= sSimRate;

    //Update when game is paused to populate debug drawing
    if(dt == 0.0f) {
      drawer.clear();
      for(auto space : mSpaces) {
        if(auto strongSpace = space.lock()) {
          strongSpace->update(0.f);
        }
      }
    }

    drawer.draw();

    //Remove all expired spaces
    mSpaces.erase(std::partition(mSpaces.begin(), mSpaces.end(), [](auto&& s) { return !s.expired(); }), mSpaces.end());
  }

  Handle PhysicsSystem::addModel(const ModelParam& newModel) {
    return _addModel(newModel.toModel());
  }

  Handle PhysicsSystem::addCompositeModel(const CompositeModelParam& newModel) {
    Model* model = mModels.add();
    Handle handle = model->getHandle();
    *model = Model(ModelType::Composite);
    model->mHandle = handle;

    model->initComposite(newModel, mModels);

    //Force model to be centered around its center of mass. I should ultimately support off-center models instead
    MassInfo info = model->_computeMasses(Vec3::Identity);
    model->_offset(-info.mCenterOfMass);

    return handle;
  }

  Handle PhysicsSystem::_addModel(const Model& newModel) {
    Model* model = mModels.add();
    Handle handle = model->getHandle();
    *model = newModel;
    model->mHandle = handle;

    if(model->getType() == ModelType::Environment) {
      model->initEnvironment();
    }
    else {
      //Force model to be centered around its center of mass. I should ultimately support off-center models instead
      MassInfo info = model->_computeMasses(Vec3::Identity);
      model->_offset(-info.mCenterOfMass);
    }
    return handle;
  }

  std::unique_ptr<IMaterialHandle> PhysicsSystem::addMaterial(const Material& newMaterial) {
    auto result = std::make_unique<MaterialHandle>();
    mMaterials.push_back(std::make_unique<OwnedMaterial>(newMaterial, *result));
    return result;
  }

#define AddConstraint(func)\
    auto space = _getSpace(ops.mSpace);\
    if(!space)\
      return SyxInvalidHandle;\
    return space->func(ops)

  Handle PhysicsSystem::addDistanceConstraint(DistanceOps ops) {
    AddConstraint(addDistanceConstraint);
  }

  Handle PhysicsSystem::addSphericalConstraint(SphericalOps ops) {
    AddConstraint(addSphericalConstraint);
  }

  Handle PhysicsSystem::addWeldConstraint(WeldOps ops) {
    AddConstraint(addWeldConstraint);
  }

  Handle PhysicsSystem::addRevoluteConstraint(RevoluteOps ops) {
    AddConstraint(addRevoluteConstraint);
  }

  void PhysicsSystem::removeConstraint(Handle space, Handle constraint) {
    if(auto pSpace = _getSpace(space)) {
      pSpace->removeConstraint(constraint);
    }
  }

  void PhysicsSystem::updateModel(Handle handle, const Model& updated) {
    Model* model = mModels.get(handle);
    if(!model)
      return;

    *model = updated;
    model->mHandle = handle;
  }

  const Model* PhysicsSystem::getModel(Handle handle) {
    return mModels.get(handle);
  }

  void PhysicsSystem::removeModel(Handle handle) {
    mModels.remove(handle);
  }

  std::shared_ptr<ISpace> PhysicsSystem::createSpace() {
    //Arbitrary selection of new unique id
    const Handle newID = [this] {
      Handle result = 0;
      for(const auto& space : mSpaces) {
        if(const auto s = space.lock()) {
          result = std::max(result, s->getHandle());
        }
      }
      return result + 1;
    }();

    auto result = std::make_shared<Space>(newID);
    mSpaces.push_back(result);
    return result;
  }

  Handle PhysicsSystem::addPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space, const IMaterialHandle& material) {
    auto pSpace = _getSpace(space);
    if(!pSpace)
      return SyxInvalidHandle;

    PhysicsObject* newObj = pSpace->createObject();

    //Enable so we can set defaults
    newObj->setColliderEnabled(true);
    newObj->getCollider()->setMaterial(material.get());
    newObj->getCollider()->setModel(*mModels.get(getCube()));
    newObj->updateModelInst();
    newObj->setColliderEnabled(hasCollider);
    if(hasCollider)
      newObj->getCollider()->initialize(*pSpace);

    pSpace->setRigidbodyEnabled(*newObj, hasRigidbody);

    Transform& t = newObj->getTransform();
    t.mPos = Vec3::Zero;
    t.mRot = Quat::Identity;
    t.mScale = Vec3::Identity;
    newObj->updateModelInst();

    if(hasRigidbody)
      newObj->getRigidbody()->calculateMass();

    return newObj->getHandle();
  }

  void PhysicsSystem::removePhysicsObject(Handle space, Handle object) {
    if(auto pSpace = _getSpace(space)) {
      pSpace->destroyObject(object);
    }
  }

  PhysicsObject* PhysicsSystem::_getObject(Handle space, Handle object) {
    auto pSpace = _getSpace(space);
    if(!pSpace)
      return nullptr;
    return pSpace->getObject(object);
  }

  Rigidbody* PhysicsSystem::_getRigidbody(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return nullptr;
    return obj->getRigidbody();
  }

  Collider* PhysicsSystem::_getCollider(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return nullptr;
    return obj->getCollider();
  }

  std::shared_ptr<Space> PhysicsSystem::_getSpace(Handle space) {
    auto it = std::find_if(mSpaces.begin(), mSpaces.end(), [s(space)](const auto& space) {
      if(auto strongSpace = space.lock()) {
        return strongSpace->getHandle() == s;
      }
      return false;
    });
    return it != mSpaces.end() ? it->lock() : nullptr;
  }

  void PhysicsSystem::setHasRigidbody(bool has, Handle space, Handle object) {
    if(auto s = _getSpace(space)) {
      if(PhysicsObject* obj = _getObject(space, object)) {
        s->setRigidbodyEnabled(*obj, has);
      }
    }
  }

  void PhysicsSystem::setHasCollider(bool has, Handle space, Handle object) {
    if(auto s = _getSpace(space)) {
      if(PhysicsObject* obj = _getObject(space, object)) {
        s->setColliderEnabled(*obj, has);
      }
    }
  }

  bool PhysicsSystem::getHasRigidbody(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return false;
    return obj->getRigidbody() != nullptr;
  }

  bool PhysicsSystem::getHasCollider(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return false;
    return obj->getCollider() != nullptr;
  }

  void PhysicsSystem::setObjectModel(Handle space, Handle object, Handle model) {
    Collider* collider = _getCollider(space, object);
    if(!collider)
      return;

    Model* pModel = mModels.get(model);
    if(!pModel)
      return;

    collider->setModel(*pModel);
    if(auto s = _getSpace(space)) {
      s->updateMovedObject(*collider->getOwner());
    }
    if(Rigidbody* rb = collider->getOwner()->getRigidbody()) {
      rb->calculateMass();
    }
  }

  void PhysicsSystem::setVelocity(Handle space, Handle object, const Vec3& vel) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj || !obj->getRigidbody())
      return;

    obj->getRigidbody()->mLinVel = vel;
    if(auto s = _getSpace(space)) {
      s->wakeObject(*obj);
    }
  }

  Vec3 PhysicsSystem::getVelocity(Handle space, Handle object) {
    Rigidbody* rigidbody = _getRigidbody(space, object);
    if(!rigidbody)
      return Vec3::Zero;

    return rigidbody->mLinVel;
  }

  void PhysicsSystem::setAngularVelocity(Handle space, Handle object, const Vec3& angVel) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj || !obj->getRigidbody())
      return;

    obj->getRigidbody()->mAngVel = angVel;
    if(auto s = _getSpace(space)) {
      s->wakeObject(*obj);
    }
  }

  Vec3 PhysicsSystem::getAngularVelocity(Handle space, Handle object) {
    Rigidbody* rigidbody = _getRigidbody(space, object);
    if(!rigidbody)
      return Vec3::Zero;

    return rigidbody->mAngVel;
  }

  void PhysicsSystem::setPosition(Handle space, Handle object, const Vec3& pos) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return;

    obj->getTransform().mPos = pos;
    if(auto s = _getSpace(space)) {
      s->updateMovedObject(*obj);
      s->wakeObject(*obj);
    }
  }

  Vec3 PhysicsSystem::getPosition(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return Vec3::Zero;

    return obj->getTransform().mPos;
  }

  void PhysicsSystem::setRotation(Handle space, Handle object, const Quat& rot) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return;

    obj->getTransform().mRot = rot;
    if(auto s = _getSpace(space)) {
      s->updateMovedObject(*obj);
      s->wakeObject(*obj);
    }
  }

  Quat PhysicsSystem::getRotation(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return Quat::Zero;

    return obj->getTransform().mRot;
  }

  void PhysicsSystem::setScale(Handle space, Handle object, const Vec3& scale) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return;

    obj->getTransform().mScale = scale;
    obj->updateModelInst();
    Rigidbody* rb = obj->getRigidbody();
    if(rb)
      rb->calculateMass();
    if(auto s = _getSpace(space)) {
      s->updateMovedObject(*obj);
      s->wakeObject(*obj);
    }
  }

  Vec3 PhysicsSystem::getScale(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return Vec3::Zero;

    return obj->getTransform().mScale;
  }

  const std::string* PhysicsSystem::getProfileReport(Handle space, const std::string& indent) {
    auto s = _getSpace(space);
    return s ? &s->getProfileReport(indent) : nullptr;
  }

  const std::vector<ProfileResult>* PhysicsSystem::getProfileHistory(Handle space) {
    auto s = _getSpace(space);
    if(!s)
      return nullptr;

    return &s->getProfileHistory();
  }

  void PhysicsSystem::getAABB(Handle space, Handle object, Vec3& min, Vec3& max) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj || !obj->getCollider())
      return;
    const AABB& bb = obj->getCollider()->getAABB();
    min = bb.getMin();
    max = bb.getMax();
  }

  const EventListener<UpdateEvent>* PhysicsSystem::getUpdateEvents(Handle space) {
    if(auto s = _getSpace(space))
      return &s->getUpdateEvents();
    return nullptr;
  }

  CastResult PhysicsSystem::lineCastAll(Handle space, const Vec3& start, const Vec3& end) {
    auto s = _getSpace(space);
    if(!s)
      return CastResult();
    SAlign Vec3 sStart = start;
    SAlign Vec3 sEnd = end;
    return s->lineCastAll(sStart, sEnd);
  }
}