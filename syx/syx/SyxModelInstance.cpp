#include "Precompile.h"
#include "SyxModelInstance.h"
#include "SyxModel.h"

namespace Syx {
  HandleGenerator ModelInstance::sHandleGen;

  ModelInstance::ModelInstance(Model* model)
      : mModel(model)
      , mMaterialSource(nullptr)
      , mHandle(sHandleGen.Next()) {
    if(model) {
      SetModel(*model);
    }
  }

  ModelInstance::ModelInstance(const Model& model, const Transformer& toWorld, const Transformer& toModel) 
    : mModel(&model)
    , mModelToWorld(toWorld)
    , mWorldToModel(toModel)
    , mMaterialSource(nullptr) {
    //Expecting user to re-use this for multiple instances by setting handle
  }


  void ModelInstance::UpdateTransformers(const Transform& parentTrans) {
    mModelToWorld = parentTrans.GetModelToWorld(mLocalTransform);
    mWorldToModel = parentTrans.GetWorldToModel(mLocalTransform);
  }

  void ModelInstance::UpdateAABB(void) {
    mAABB = mModel->GetWorldAABB(mModelToWorld);
  }

  Vec3 ModelInstance::GetSupport(const Vec3& dir) {
    SAlign Vec3 localDir = mWorldToModel.TransformVector(dir);
    SAlign Vec3 localSupport = mModel->GetSupport(localDir);
    return mModelToWorld.TransformPoint(localSupport);
  }

  int ModelInstance::GetModelType(void) const {
    return mModel->GetType();
  }

  void ModelInstance::SetModel(const Model& model) {
    mModel = &model;
    mSubmodelInstHandles.clear();
    if(mModel->GetType() == ModelType::Composite) {
      for(size_t i = 0; i < mModel->GetSubmodelInstances().size(); ++i)
        mSubmodelInstHandles.push_back(sHandleGen.Next());
    }
  }

  void ModelInstance::SetMaterial(const Material& material) {
    mMaterialSource = &material;
    mLocalMaterial = material;
  }

  void ModelInstance::SetLocalTransform(const Transform& transform) {
    mLocalTransform = transform;
  }

  void ModelInstance::SetSubmodelInstLocalTransform(const Transform& transform) {
    //Submodel instances are in the parent model's space, so update these now
    SetLocalTransform(transform);
    UpdateTransformers(Transform());
    UpdateAABB();
  }

  Handle ModelInstance::GetSubmodelInstHandle(size_t index) const {
    return mSubmodelInstHandles[index];
  }

  ModelInstance ModelInstance::Combined(const ModelInstance& parent, const ModelInstance& child, const ModelInstance& modelInfo, Handle handle) {
    ModelInstance result = modelInfo;
    result.mHandle = handle;
    result.mModelToWorld = Transformer::Combined(child.mModelToWorld, parent.mModelToWorld);
    result.mWorldToModel = Transformer::Combined(parent.mWorldToModel, child.mWorldToModel);
    return result;
  }

#ifdef SENABLED
  SFloats ModelInstance::SGetSupport(SFloats dir) {
    //Transform world vector to model space, get model space support point, and transform it back into world space
    return mModelToWorld.ToSIMDPoint().TransformPoint(mModel->SGetSupport(mWorldToModel.ToSIMDVector().TransformVector(dir)));
  }
#else
  SVec3 ModelInstance::SGetSupport(const SVec3& dir) { return dir; }
#endif
}