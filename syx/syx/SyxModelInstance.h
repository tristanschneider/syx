#pragma once
#include "SyxTransform.h"
#include "SyxAABB.h"
#include "SyxMaterial.h"
#include "SyxHandles.h"

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
    static ModelInstance Combined(const ModelInstance& parent, const ModelInstance& child, const ModelInstance& modelInfo, Handle handle);

    void SetModel(const Model& model);
    void SetMaterial(const Material& material);
    void SetLocalTransform(const Transform& transform);

    void SetSubmodelInstLocalTransform(const Transform& transform);
    Handle GetSubmodelInstHandle(size_t index) const;

    const Transform& GetLocalTransform() const { return mLocalTransform; }
    const Model& GetModel(void) const { return *mModel; }
    int GetModelType(void) const;
    const Material& GetMaterial(void) const { return mLocalMaterial; }
    const Transformer& GetModelToWorld() const { return mModelToWorld; }
    const Transformer& GetWorldToModel() const { return mWorldToModel; }

    void UpdateTransformers(const Transform& parentTrans);
    void UpdateAABB(void);
    Vec3 GetSupport(const Vec3& dir);

    const AABB& GetAABB(void) const { return mAABB; }

    Handle GetHandle() const { return mHandle; }
    void SetHandle(Handle val) { mHandle = val; }

    SFloats SGetSupport(SFloats dir);

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