#pragma once
#include "SyxTransform.h"
#include "SyxMaterial.h"

namespace Syx {
  class Model;

  class ModelInstance {
  public:
    friend class Model;

    //Maybe this should be on the space. I don't really think it matters though, as it's just used as a unique identifier for contact pairs
    static HandleGenerator sHandleGen;

    ModelInstance(Model* model = nullptr);
    ModelInstance(const Model& model, const Transformer& toWorld, const Transformer& toModel);

    //Returns a model instance with the concatenated transforms of parent and child, and model/material info of modelInfo
    //AABB and localtransform won't be meaningful on the returned object
    static ModelInstance combined(const ModelInstance& parent, const ModelInstance& child, const ModelInstance& modelInfo, Handle handle);

    void setModel(const Model& model);
    void setMaterial(const Material& material);
    void setLocalTransform(const Transform& transform);

    void setSubmodelInstLocalTransform(const Transform& transform);
    Handle getSubmodelInstHandle(size_t index) const;

    const Transform& getLocalTransform() const { return mLocalTransform; }
    const Model& getModel(void) const { return *mModel; }
    int getModelType(void) const;
    const Material& getMaterial(void) const { return mLocalMaterial; }
    const Transformer& getModelToWorld() const { return mModelToWorld; }
    const Transformer& getWorldToModel() const { return mWorldToModel; }

    void updateTransformers(const Transform& parentTrans);
    void updateAABB(void);
    Vec3 getSupport(const Vec3& dir);

    const AABB& getAABB(void) const { return mAABB; }

    Handle getHandle() const { return mHandle; }
    void setHandle(Handle val) { mHandle = val; }

    SFloats sGetSupport(SFloats dir);

  private:
    const Model* mModel;
    //Since materials are so small, keep a local version to avoid cache misses and only use the pointer if we need to update our local version
    const Material* mMaterialSource;
    Material mLocalMaterial;
    //Offset relative to parent physics object
    Transform mLocalTransform;
    Transformer mModelToWorld;
    Transformer mWorldToModel;
    AABB mAABB;
    Handle mHandle;
    std::vector<Handle> mSubmodelInstHandles;
  };
}