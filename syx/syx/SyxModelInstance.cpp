#include "Precompile.h"
#include "SyxModelInstance.h"
#include "SyxModel.h"

namespace Syx {
  HandleGenerator ModelInstance::sHandleGen;

  ModelInstance::ModelInstance(std::shared_ptr<const Model> model) 
   : mHandle(sHandleGen.next()) {
    if(model) {
      setModel(model);
    }
  }

  ModelInstance::ModelInstance(std::shared_ptr<const Model> model, const Transformer& toWorld, const Transformer& toModel)
    : ModelInstance(std::move(model)) {
    mModelToWorld = toWorld;
    mWorldToModel = toModel;
  }

  void ModelInstance::updateTransformers(const Transform& parentTrans) {
    mModelToWorld = parentTrans.getModelToWorld(mLocalTransform);
    mWorldToModel = parentTrans.getWorldToModel(mLocalTransform);
  }

  void ModelInstance::updateAABB(void) {
    mAABB = mModel->getWorldAABB(mModelToWorld);
  }

  Vec3 ModelInstance::getSupport(const Vec3& dir) {
    SAlign Vec3 localDir = mWorldToModel.transformVector(dir);
    SAlign Vec3 localSupport = mModel->getSupport(localDir);
    return mModelToWorld.transformPoint(localSupport);
  }

  int ModelInstance::getModelType(void) const {
    return mModel->getType();
  }

  void ModelInstance::setModel(std::shared_ptr<const Model> model) {
    mModel = model;
    mSubmodelInstHandles.clear();
    if(mModel->getType() == ModelType::Composite) {
      for(size_t i = 0; i < mModel->getSubmodelInstances().size(); ++i)
        mSubmodelInstHandles.push_back(sHandleGen.next());
    }
  }

  const Model& ModelInstance::getModel() const {
    return mModel ? *mModel : Model::NONE;
  }

  void ModelInstance::setMaterial(const IMaterialHandle& material) {
    //A bit of a hack to downcast here but it allows avoiding allocation of another handle
    mMaterialSource = static_cast<const MaterialHandle&>(material);
    mLocalMaterial = material.get();
  }

  void ModelInstance::setLocalTransform(const Transform& transform) {
    mLocalTransform = transform;
  }

  void ModelInstance::setSubmodelInstLocalTransform(const Transform& transform) {
    //Submodel instances are in the parent model's space, so update these now
    setLocalTransform(transform);
    updateTransformers(Transform());
    updateAABB();
  }

  Handle ModelInstance::getSubmodelInstHandle(size_t index) const {
    return mSubmodelInstHandles[index];
  }

  ModelInstance ModelInstance::combined(const ModelInstance& parent, const ModelInstance& child, const ModelInstance& modelInfo, Handle handle) {
    ModelInstance result = modelInfo;
    result.mHandle = handle;
    result.mModelToWorld = Transformer::combined(child.mModelToWorld, parent.mModelToWorld);
    result.mWorldToModel = Transformer::combined(parent.mWorldToModel, child.mWorldToModel);
    return result;
  }

#ifdef SENABLED
  SFloats ModelInstance::sGetSupport(SFloats dir) {
    //Transform world vector to model space, get model space support point, and transform it back into world space
    return mModelToWorld.toSIMDPoint().transformPoint(mModel->sGetSupport(mWorldToModel.toSIMDVector().transformVector(dir)));
  }
#else
  SVec3 ModelInstance::sGetSupport(const SVec3& dir) { return dir; }
#endif
}