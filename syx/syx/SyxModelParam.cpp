#include "Precompile.h"
#include "SyxModelParam.h"
#include "SyxModel.h"

namespace Syx {
  ModelParam::ModelParam()
    : mEnvironment(false) {}

  void ModelParam::SetEnvironment(bool isEnvironment) {
    mEnvironment = isEnvironment;
  }

  void ModelParam::Reserve(size_t size) {
    mPoints.reserve(size);
    mTriangles.reserve(size/3 + 1);
  }

  void ModelParam::AddVertex(const Vec3& v) {
    mPoints.push_back(v);
  }

  void ModelParam::AddTriangle(size_t a, size_t b, size_t c) {
    mTriangles.push_back(mPoints[a]);
    mTriangles.push_back(mPoints[b]);
    mTriangles.push_back(mPoints[c]);
  }

  Model ModelParam::ToModel(void) const {
    return Model(mPoints, mTriangles, mEnvironment);
  }

  void CompositeModelParam::Reserve(size_t submodels, size_t instances) {
    mSubmodels.reserve(submodels);
    mInstances.reserve(instances);
  }

  Handle CompositeModelParam::AddSubmodel(const ModelParam& model, bool addInstance) {
    Handle result = mHandleGen.Next();
    mSubmodels[result] = model;
    if(addInstance)
      AddSubmodelInstance(SubmodelInstance(result, true, Transform()));
    return result;
  }

  void CompositeModelParam::AddSubmodelInstance(const SubmodelInstance& instance) {
    mInstances.push_back(instance);
  }
}