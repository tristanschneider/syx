#pragma once

struct Vertex {
  Vertex(const Syx::Vec3& pos, const Syx::Vec3& normal, const Syx::Vec2& uv);
  Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v);

  float mPos[3];
  float mNormal[3];
  float mUV[2];
};

struct Model {
  Model();
  Model(GLuint vb, GLuint va);

  void loadGpu();
  void unloadGpu();

  std::vector<Vertex> mVerts;
  std::vector<size_t> mIndices;
  Handle mHandle;
  GLuint mVB;
  GLuint mIB;
  GLuint mVA;
};