#include "Precompile.h"
#include "SyxPhysicsSystem.h"
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

    mDefaultMaterial = addMaterial(Material());
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
      for(auto it = mSpaces.begin(); it != mSpaces.end(); ++it)
        (*it).update(sSimRate);
    }

    //Subtract off the rest of the rest of the time if we hit the update cap to keep time from building up
    while(accumulated >= sSimRate)
      accumulated -= sSimRate;

    //Update when game is paused to populate debug drawing
    if(dt == 0.0f) {
      drawer.clear();
      for(auto it = mSpaces.begin(); it != mSpaces.end(); ++it)
        (*it).update(0.0f);
    }

    drawer.draw();
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

  Handle PhysicsSystem::addMaterial(const Material& newMaterial) {
    Material* material = mMaterials.add();
    Handle handle = material->mHandle;
    *material = newMaterial;
    material->mHandle = handle;
    return handle;
  }

#define AddConstraint(func)\
    Space* space = mSpaces.get(ops.mSpace);\
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
    Space* pSpace = mSpaces.get(space);
    if(!pSpace)
      return;
    pSpace->removeConstraint(constraint);
  }

  void PhysicsSystem::updateModel(Handle handle, const Model& updated) {
    Model* model = mModels.get(handle);
    if(!model)
      return;

    *model = updated;
    model->mHandle = handle;
  }

  void PhysicsSystem::updateMaterial(Handle handle, const Material& updated) {
    Material* material = mMaterials.get(handle);
    if(!material)
      return;

    *material = updated;
    material->mHandle = handle;
  }

  const Model* PhysicsSystem::getModel(Handle handle) {
    return mModels.get(handle);
  }

  const Material* PhysicsSystem::getMaterial(Handle handle) {
    return mMaterials.get(handle);
  }

  void PhysicsSystem::removeModel(Handle handle) {
    mModels.remove(handle);
  }

  void PhysicsSystem::removeMaterial(Handle handle) {
    mMaterials.remove(handle);
  }

  Handle PhysicsSystem::addSpace(void) {
    Space* newSpace = mSpaces.add();
    return newSpace->getHandle();
  }

  void PhysicsSystem::removeSpace(Handle handle) {
    mSpaces.remove(handle);
  }

  void PhysicsSystem::clearSpace(Handle handle) {
    Space* space = mSpaces.get(handle);
    if(space)
      space->clear();
  }

  Handle PhysicsSystem::addPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space) {
    Space* pSpace = mSpaces.get(space);
    if(!pSpace)
      return SyxInvalidHandle;

    PhysicsObject* newObj = pSpace->createObject();

    //Enable so we can set defaults
    newObj->setColliderEnabled(true);
    newObj->getCollider()->setMaterial(*mMaterials.get(getDefaultMaterial()));
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
    Space* pSpace = mSpaces.get(space);
    if(!pSpace)
      return;

    pSpace->destroyObject(object);
  }

  PhysicsObject* PhysicsSystem::_getObject(Handle space, Handle object) {
    Space* pSpace = mSpaces.get(space);
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

  void PhysicsSystem::setHasRigidbody(bool has, Handle space, Handle object) {
    if(Space* s = mSpaces.get(space)) {
      if(PhysicsObject* obj = _getObject(space, object)) {
        s->setRigidbodyEnabled(*obj, has);
      }
    }
  }

  void PhysicsSystem::setHasCollider(bool has, Handle space, Handle object) {
    if(Space* s = mSpaces.get(space)) {
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
    mSpaces.get(space)->updateMovedObject(*collider->getOwner());
    Rigidbody* rb = collider->getOwner()->getRigidbody();
    if(rb)
      rb->calculateMass();
  }

  void PhysicsSystem::setObjectMaterial(Handle space, Handle object, Handle material) {
    Collider* collider = _getCollider(space, object);
    if(!collider)
      return;

    Material* pMat = mMaterials.get(material);
    if(!pMat)
      return;

    collider->setMaterial(*pMat);
    Rigidbody* rb = collider->getOwner()->getRigidbody();
    if(rb)
      rb->calculateMass();
  }

  void PhysicsSystem::setVelocity(Handle space, Handle object, const Vec3& vel) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj || !obj->getRigidbody())
      return;

    obj->getRigidbody()->mLinVel = vel;
    mSpaces.get(space)->wakeObject(*obj);
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
    mSpaces.get(space)->wakeObject(*obj);
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
    Space* s = mSpaces.get(space);
    s->updateMovedObject(*obj);
    s->wakeObject(*obj);
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
    Space* s = mSpaces.get(space);
    s->updateMovedObject(*obj);
    s->wakeObject(*obj);
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
    Space* s = mSpaces.get(space);
    s->updateMovedObject(*obj);
    s->wakeObject(*obj);
  }

  Vec3 PhysicsSystem::getScale(Handle space, Handle object) {
    PhysicsObject* obj = _getObject(space, object);
    if(!obj)
      return Vec3::Zero;

    return obj->getTransform().mScale;
  }

  const std::string* PhysicsSystem::getProfileReport(Handle space, const std::string& indent) {
    Space* s = mSpaces.get(space);
    if(!s)
      return nullptr;

    return &s->getProfileReport(indent);
  }

  const std::vector<ProfileResult>* PhysicsSystem::getProfileHistory(Handle space) {
    Space* s = mSpaces.get(space);
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
    if(Space* s = mSpaces.get(space))
      return &s->getUpdateEvents();
    return nullptr;
  }

  CastResult PhysicsSystem::lineCastAll(Handle space, const Vec3& start, const Vec3& end) {
    Space* s = mSpaces.get(space);
    if(!s)
      return CastResult();
    SAlign Vec3 sStart = start;
    SAlign Vec3 sEnd = end;
    return s->lineCastAll(sStart, sEnd);
  }
}