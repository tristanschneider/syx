#include "Precompile.h"
#include "Model.h"

Vertex::Vertex(const Syx::Vec3& pos, const Syx::Vec3& normal, const Syx::Vec2& uv)
  : mPos{pos.x, pos.y, pos.z}
  , mNormal{normal.x, normal.y, normal.z}
  , mUV{uv.x, uv.y} {
}

Vertex::Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v)
  : mPos{px, py, pz}
  , mNormal{nx, ny, nz}
  , mUV{u, v} {
}

Model::Binder::Binder(const Model& model) {
  glBindVertexArray(model.mVA);
  //This bind shouldn't be needed since vao is supposed to know all it needs, but some intel cards apparently have a bug with index buffers
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.mIB);
}

Model::Binder::~Binder() {
  glBindVertexArray(0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Model::Model()
  : mVB(0)
  , mVA(0)
  , mIB(0) {
}

Model::Model(GLuint vb, GLuint va)
  : mVB(vb)
  , mVA(va)
  , mIB(0) {
}

void Model::loadGpu() {
  if(mVA || mVB) {
    printf("Tried to upload model to gpu that already was\n");
    return;
  }
  if(mVerts.empty()) {
    printf("Tried to upload model that had no verts\n");
    return;
  }
  if(mIndices.empty()) {
    printf("Tried to upload model that had no indices\n");
    return;
  }

  //Generate and upload vertex buffer
  glGenBuffers(1, &mVB);
  glBindBuffer(GL_ARRAY_BUFFER, mVB);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)*mVerts.size(), mVerts.data(), GL_STATIC_DRAW);

  //Generate and upload index buffer
  glGenBuffers(1, &mIB);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIB);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(size_t)*mIndices.size(), mIndices.data(), GL_STATIC_DRAW);

  //Generate vertex array
  glGenVertexArrays(1, &mVA);
  //Bind this array so we can fill it in
  glBindVertexArray(mVA);

  //Define vertex attributes
  glEnableVertexAttribArray(0);
  size_t stride = sizeof(Vertex);
  size_t start = 0;
  //Position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
  start += sizeof(float)*3;
  //Normal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
  start += sizeof(float)*3;
  //UV
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
  glBindVertexArray(0);
}

void Model::unloadGpu() {
  if(mVB || mIB || mVA) {
    printf("Tried to unload model that was already unloaded\n");
    return;
  }
  glDeleteBuffers(1, &mVB);
  glDeleteBuffers(1, &mIB);
  glDeleteBuffers(1, &mVA);
  mVB = mIB = mVA = 0;
}
