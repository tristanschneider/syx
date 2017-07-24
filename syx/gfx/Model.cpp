#include "Precompile.h"
#include "Model.h"

Model::Model()
  : mVB(0)
  , mVA(0) {
}

Model::Model(GLuint vb, GLuint va)
  : mVB(vb)
  , mVA(va) {
}
