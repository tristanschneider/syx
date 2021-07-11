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

  void CompositeModelParam::reserve(size_t, size_t instances) {
    mInstances.reserve(instances);
  }

  void CompositeModelParam::addSubmodelInstance(SubmodelInstance instance) {
    mInstances.emplace_back(std::move(instance));
  }
}