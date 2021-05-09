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
      SubmodelInstance(std::shared_ptr<Model> model, const Transform& transform)
        : mModel(std::move(model))
        , mTransform(transform) {
      }

      std::shared_ptr<Model> mModel;
      Transform mTransform;
    };

    void reserve(size_t submodels, size_t instances);
    void addSubmodelInstance(SubmodelInstance instance);

  private:
    std::vector<SubmodelInstance> mInstances;
  };
}