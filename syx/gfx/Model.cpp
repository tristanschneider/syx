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

Model::Model()
  : mVB(0)
  , mVA(0) {
}

Model::Model(GLuint vb, GLuint va)
  : mVB(vb)
  , mVA(va) {
}
