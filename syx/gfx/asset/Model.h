#pragma once
#include "asset/Asset.h"

struct Vertex {
  Vertex(const Syx::Vec3& pos, const Syx::Vec3& normal, const Syx::Vec2& uv);
  Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v);

  float mPos[3];
  float mNormal[3];
  float mUV[2];
};

class Model : public Asset {
public:
  struct Binder {
    Binder(const Model& model);
    ~Binder();
  };

  Model(AssetInfo&& info);

  void loadGpu();
  void unloadGpu();
  void draw() const;

  std::vector<Vertex> mVerts;
  std::vector<size_t> mIndices;
  Handle mHandle;
  GLHandle mVB;
  GLHandle mIB;
  GLHandle mVA;
};