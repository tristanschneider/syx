#include "Precompile.h"
#include "graphics/FullScreenQuad.h"

#include <gl/glew.h>

FullScreenQuad::FullScreenQuad() {
  const float verts[] = { 1, -1, 0,
    -1, 1, 0,
    -1, -1, 0,

    1, -1, 0,
    1, 1, 0,
    -1, 1, 0
  };
  //Generate and upload vertex buffer
  glGenBuffers(1, &mVB);
  glBindBuffer(GL_ARRAY_BUFFER, mVB);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6*3, verts, GL_STATIC_DRAW);
}

FullScreenQuad::~FullScreenQuad() {
  glDeleteBuffers(1, &mVB);
}

void FullScreenQuad::draw() const {
  bool prevDepth = glIsEnabled(GL_DEPTH_TEST);;
  glDisable(GL_DEPTH_TEST);
  glBindBuffer(GL_ARRAY_BUFFER, mVB);
  //Could tie these to a vao, but that would also require and index buffer all of which seems like overkill
  glEnableVertexAttribArray(0);
  //First attribute, 3 float elements that shouldn't be normalized, tightly packed, no offset to start
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDisableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  if(prevDepth) {
    glEnable(GL_DEPTH_TEST);
  }
}
