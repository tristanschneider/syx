#pragma once

struct Model {
  Model();
  Model(GLuint vb, GLuint va);

  Handle mHandle;
  GLuint mVB;
  GLuint mVA;
};