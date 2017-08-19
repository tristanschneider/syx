#include "Precompile.h"
#include "SyxPhysicsSystem.h"
#include "SyxModelParam.h"

namespace Syx {
  SyxOptions gOptions;

  float PhysicsSystem::sSimRate = 1.0f/60.0f;

  PhysicsSystem::PhysicsSystem() {
    mCubeModel = AddModel(Model(ModelType::Cube));
    mSphereModel = AddModel(Model(ModelType::Sphere));
    mCylinderModel = AddModel(Model(ModelType::Cylinder));
    mCapsuleModel = AddModel(Model(ModelType::Capsule));
    mConeModel = AddModel(Model(ModelType::Cone));

    mDefaultMaterial = AddMaterial(Material());
  }

  void PhysicsSystem::Update(float dt) {
    static float accumulated = 0.0f;
    int maxUpdates = 5;
    int updates = 0;
    DebugDrawer& drawer = DebugDrawer::Get();

    gOptions = Interface::GetOptions();

    accumulated += dt;
    while(accumulated >= sSimRate && updates++ < maxUpdates) {
      drawer.Clear();
      accumulated -= sSimRate;
      for(auto it = mSpaces.Begin(); it != mSpaces.End(); ++it)
        (*it).Update(sSimRate);
    }

    //Subtract off the rest of the rest of the time if we hit the update cap to keep time from building up
    while(accumulated >= sSimRate)
      accumulated -= sSimRate;

    //Update when game is paused to populate debug drawing
    if(dt == 0.0f) {
      drawer.Clear();
      for(auto it = mSpaces.Begin(); it != mSpaces.End(); ++it)
        (*it).Update(0.0f);
    }

    drawer.Draw();
  }

  Handle PhysicsSystem::AddModel(const ModelParam& newModel) {
    return AddModel(newModel.ToModel());
  }

  Handle PhysicsSystem::AddCompositeModel(const CompositeModelParam& newModel) {
    Model* model = mModels.Add();
    Handle handle = model->GetHandle();
    *model = Model(ModelType::Composite);
    model->mHandle = handle;

    model->InitComposite(newModel, mModels);

    //Force model to be centered around its center of mass. I should ultimately support off-center models instead
    MassInfo info = model->ComputeMasses(Vec3::Identity);
    model->Offset(-info.mCenterOfMass);

    return handle;
  }

  Handle PhysicsSystem::AddModel(const Model& newModel) {
    Model* model = mModels.Add();
    Handle handle = model->GetHandle();
    *model = newModel;
    model->mHandle = handle;

    if(model->GetType() == ModelType::Environment) {
      model->InitEnvironment();
    }
    else {
      //Force model to be centered around its center of mass. I should ultimately support off-center models instead
      MassInfo info = model->ComputeMasses(Vec3::Identity);
      model->Offset(-info.mCenterOfMass);
    }
    return handle;
  }

  Handle PhysicsSystem::AddMaterial(const Material& newMaterial) {
    Material* material = mMaterials.Add();
    Handle handle = material->mHandle;
    *material = newMaterial;
    material->mHandle = handle;
    return handle;
  }

#define AddConstraint(func)\
    Space* space = mSpaces.Get(ops.mSpace);\
    if(!space)\
      return SyxInvalidHandle;\
    return space->func(ops)

  Handle PhysicsSystem::AddDistanceConstraint(DistanceOps ops) {
    AddConstraint(AddDistanceConstraint);
  }

  Handle PhysicsSystem::AddSphericalConstraint(SphericalOps ops) {
    AddConstraint(AddSphericalConstraint);
  }

  Handle PhysicsSystem::AddWeldConstraint(WeldOps ops) {
    AddConstraint(AddWeldConstraint);
  }

  Handle PhysicsSystem::AddRevoluteConstraint(RevoluteOps ops) {
    AddConstraint(AddRevoluteConstraint);
  }

  void PhysicsSystem::RemoveConstraint(Handle space, Handle constraint) {
    Space* pSpace = mSpaces.Get(space);
    if(!pSpace)
      return;
    pSpace->RemoveConstraint(constraint);
  }

  void PhysicsSystem::UpdateModel(Handle handle, const Model& updated) {
    Model* model = mModels.Get(handle);
    if(!model)
      return;

    *model = updated;
    model->mHandle = handle;
  }

  void PhysicsSystem::UpdateMaterial(Handle handle, const Material& updated) {
    Material* material = mMaterials.Get(handle);
    if(!material)
      return;

    *material = updated;
    material->mHandle = handle;
  }

  const Model* PhysicsSystem::GetModel(Handle handle) {
    return mModels.Get(handle);
  }

  const Material* PhysicsSystem::GetMaterial(Handle handle) {
    return mMaterials.Get(handle);
  }

  void PhysicsSystem::RemoveModel(Handle handle) {
    mModels.Remove(handle);
  }

  void PhysicsSystem::RemoveMaterial(Handle handle) {
    mMaterials.Remove(handle);
  }

  Handle PhysicsSystem::AddSpace(void) {
    Space* newSpace = mSpaces.Add();
    return newSpace->GetHandle();
  }

  void PhysicsSystem::RemoveSpace(Handle handle) {
    mSpaces.Remove(handle);
  }

  void PhysicsSystem::ClearSpace(Handle handle) {
    Space* space = mSpaces.Get(handle);
    if(space)
      space->Clear();
  }

  Handle PhysicsSystem::AddPhysicsObject(bool hasRigidbody, bool hasCollider, Handle space) {
    Space* pSpace = mSpaces.Get(space);
    if(!pSpace)
      return SyxInvalidHandle;

    PhysicsObject* newObj = pSpace->CreateObject();

    //Enable so we can set defaults
    newObj->SetColliderEnabled(true);
    newObj->GetCollider()->SetMaterial(*mMaterials.Get(GetDefaultMaterial()));
    newObj->GetCollider()->SetModel(*mModels.Get(GetCube()));
    newObj->UpdateModelInst();
    newObj->SetColliderEnabled(hasCollider);
    if(hasCollider)
      newObj->GetCollider()->Initialize(*pSpace);

    pSpace->SetRigidbodyEnabled(*newObj, hasRigidbody);

    Transform& t = newObj->GetTransform();
    t.mPos = Vec3::Zero;
    t.mRot = Quat::Identity;
    t.mScale = Vec3::Identity;
    newObj->UpdateModelInst();

    if(hasRigidbody)
      newObj->GetRigidbody()->CalculateMass();

    return newObj->GetHandle();
  }

  void PhysicsSystem::RemovePhysicsObject(Handle space, Handle object) {
    Space* pSpace = mSpaces.Get(space);
    if(!pSpace)
      return;

    pSpace->DestroyObject(object);
  }

  PhysicsObject* PhysicsSystem::GetObject(Handle space, Handle object) {
    Space* pSpace = mSpaces.Get(space);
    if(!pSpace)
      return nullptr;
    return pSpace->GetObject(object);
  }

  Rigidbody* PhysicsSystem::GetRigidbody(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return nullptr;
    return obj->GetRigidbody();
  }

  Collider* PhysicsSystem::GetCollider(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return nullptr;
    return obj->GetCollider();
  }

  void PhysicsSystem::SetHasRigidbody(bool has, Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return;
    obj->SetRigidbodyEnabled(has);
  }

  void PhysicsSystem::SetHasCollider(bool has, Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return;
    obj->SetColliderEnabled(has);
  }

  bool PhysicsSystem::GetHasRigidbody(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return false;
    return obj->GetRigidbody() != nullptr;
  }

  bool PhysicsSystem::GetHasCollider(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return false;
    return obj->GetCollider() != nullptr;
  }

  void PhysicsSystem::SetObjectModel(Handle space, Handle object, Handle model) {
    Collider* collider = GetCollider(space, object);
    if(!collider)
      return;

    Model* pModel = mModels.Get(model);
    if(!pModel)
      return;

    collider->SetModel(*pModel);
    mSpaces.Get(space)->UpdateMovedObject(*collider->GetOwner());
    Rigidbody* rb = collider->GetOwner()->GetRigidbody();
    if(rb)
      rb->CalculateMass();
  }

  void PhysicsSystem::SetObjectMaterial(Handle space, Handle object, Handle material) {
    Collider* collider = GetCollider(space, object);
    if(!collider)
      return;

    Material* pMat = mMaterials.Get(material);
    if(!pMat)
      return;

    collider->SetMaterial(*pMat);
    Rigidbody* rb = collider->GetOwner()->GetRigidbody();
    if(rb)
      rb->CalculateMass();
  }

  void PhysicsSystem::SetVelocity(Handle space, Handle object, const Vec3& vel) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj || !obj->GetRigidbody())
      return;

    obj->GetRigidbody()->mLinVel = vel;
    mSpaces.Get(space)->WakeObject(*obj);
  }

  Vec3 PhysicsSystem::GetVelocity(Handle space, Handle object) {
    Rigidbody* rigidbody = GetRigidbody(space, object);
    if(!rigidbody)
      return Vec3::Zero;

    return rigidbody->mLinVel;
  }

  void PhysicsSystem::SetAngularVelocity(Handle space, Handle object, const Vec3& angVel) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj || !obj->GetRigidbody())
      return;

    obj->GetRigidbody()->mAngVel = angVel;
    mSpaces.Get(space)->WakeObject(*obj);
  }

  Vec3 PhysicsSystem::GetAngularVelocity(Handle space, Handle object) {
    Rigidbody* rigidbody = GetRigidbody(space, object);
    if(!rigidbody)
      return Vec3::Zero;

    return rigidbody->mAngVel;
  }

  void PhysicsSystem::SetPosition(Handle space, Handle object, const Vec3& pos) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return;

    obj->GetTransform().mPos = pos;
    Space* s = mSpaces.Get(space);
    s->UpdateMovedObject(*obj);
    s->WakeObject(*obj);
  }

  Vec3 PhysicsSystem::GetPosition(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return Vec3::Zero;

    return obj->GetTransform().mPos;
  }

  void PhysicsSystem::SetRotation(Handle space, Handle object, const Quat& rot) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return;

    obj->GetTransform().mRot = rot;
    Space* s = mSpaces.Get(space);
    s->UpdateMovedObject(*obj);
    s->WakeObject(*obj);
  }

  Quat PhysicsSystem::GetRotation(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return Quat::Zero;

    return obj->GetTransform().mRot;
  }

  void PhysicsSystem::SetScale(Handle space, Handle object, const Vec3& scale) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return;

    obj->GetTransform().mScale = scale;
    obj->UpdateModelInst();
    Rigidbody* rb = obj->GetRigidbody();
    if(rb)
      rb->CalculateMass();
    Space* s = mSpaces.Get(space);
    s->UpdateMovedObject(*obj);
    s->WakeObject(*obj);
  }

  Vec3 PhysicsSystem::GetScale(Handle space, Handle object) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj)
      return Vec3::Zero;

    return obj->GetTransform().mScale;
  }

  const std::string* PhysicsSystem::GetProfileReport(Handle space, const std::string& indent) {
    Space* s = mSpaces.Get(space);
    if(!s)
      return nullptr;

    return &s->GetProfileReport(indent);
  }

  const std::vector<ProfileResult>* PhysicsSystem::GetProfileHistory(Handle space) {
    Space* s = mSpaces.Get(space);
    if(!s)
      return nullptr;

    return &s->GetProfileHistory();
  }

  void PhysicsSystem::GetAABB(Handle space, Handle object, Vec3& min, Vec3& max) {
    PhysicsObject* obj = GetObject(space, object);
    if(!obj || !obj->GetCollider())
      return;
    const AABB& bb = obj->GetCollider()->GetAABB();
    min = bb.GetMin();
    max = bb.GetMax();
  }

  CastResult PhysicsSystem::LineCastAll(Handle space, const Vec3& start, const Vec3& end) {
    Space* s = mSpaces.Get(space);
    if(!s)
      return CastResult();
    SAlign Vec3 sStart = start;
    SAlign Vec3 sEnd = end;
    return s->LineCastAll(sStart, sEnd);
  }
}