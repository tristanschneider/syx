#pragma once
#include "SyxTransform.h"

namespace Syx {
  class Model;

  class ModelParam {
  public:
    ModelParam();

    void reserve(size_t size);
    void reserve(size_t verts, size_t indices);
    void addVertex(const Vec3& v);
    void addTriangle(size_t a, size_t b, size_t c);
    void addIndex(size_t i);
    Model toModel(void) const;
    void setEnvironment(bool isEnvironment);

  private:
    Vec3Vec mPoints;
    Vec3Vec mTriangles;
    bool mEnvironment;
  };

  class CompositeModelParam {
  public:
    friend class Model;

    struct SubmodelInstance {
      SubmodelInstance(Handle handle, bool local, const Transform& transform)
        : mHandle(handle)
        , mLocal(local)
        , mTransform(transform) {}

      //Handle can either point to a Submodel in this CompositeModelParam in which case local=true, or another model added to the physics system, in which case local=false
      Handle mHandle;
      bool mLocal;
      Transform mTransform;
    };

    void reserve(size_t submodels, size_t instances);
    //Add a model that will be part of this composite model.
    //An instance can be added with an empty transform with 'addInstance' or later with a specified tranform through AddSubmodelInstance
    //If neither of these are done, the submodel won't actually be a part of the composite model, which wouldn't make sense to do
    Handle addSubmodel(const ModelParam& model, bool addInstance);
    void addSubmodelInstance(const SubmodelInstance& instance);

  private:
    std::vector<SubmodelInstance> mInstances;
    std::unordered_map<Handle, ModelParam> mSubmodels;
    HandleGenerator mHandleGen;
  };
}