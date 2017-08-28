#include "Precompile.h"
#include "SyxModelParam.h"
#include "SyxModel.h"

namespace Syx {
  ModelParam::ModelParam()
    : mEnvironment(false) {}

  void ModelParam::setEnvironment(bool isEnvironment) {
    mEnvironment = isEnvironment;
  }

  void ModelParam::reserve(size_t size) {
    mPoints.reserve(size);
    mTriangles.reserve(size/3 + 1);
  }

  void ModelParam::reserve(size_t verts, size_t indices) {
    mPoints.reserve(verts);
    mTriangles.reserve(indices);
  }

  void ModelParam::addVertex(const Vec3& v) {
    mPoints.push_back(v);
  }

  void ModelParam::addTriangle(size_t a, size_t b, size_t c) {
    mTriangles.push_back(mPoints[a]);
    mTriangles.push_back(mPoints[b]);
    mTriangles.push_back(mPoints[c]);
  }

  void ModelParam::addIndex(size_t i) {
    mTriangles.push_back(mPoints[i]);
  }

  Model ModelParam::toModel(void) const {
    return Model(mPoints, mTriangles, mEnvironment);
  }

  void CompositeModelParam::reserve(size_t submodels, size_t instances) {
    mSubmodels.reserve(submodels);
    mInstances.reserve(instances);
  }

  Handle CompositeModelParam::addSubmodel(const ModelParam& model, bool addInstance) {
    Handle result = mHandleGen.next();
    mSubmodels[result] = model;
    if(addInstance)
      addSubmodelInstance(SubmodelInstance(result, true, Transform()));
    return result;
  }

  void CompositeModelParam::addSubmodelInstance(const SubmodelInstance& instance) {
    mInstances.push_back(instance);
  }
}