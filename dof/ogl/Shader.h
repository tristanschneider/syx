#pragma once

#include "GL/glew.h"

struct TextureSamplerUniform {
  GLuint uniformID, texture, buffer;
};

struct Shader {
  static void compileShader(GLuint shaderHandle, const char* source);
  static GLuint loadShader(const char* vsSource, const char* psSource);

  static void _getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result);
  static bool _link(GLuint program);
  //Validates program and all bound inputs, so only reasonable to do before drawing, not after linking when nothing is bound.
  static bool _validate(GLuint program);
  static GLuint _detachAndDestroy(GLuint program, GLuint s);
  static GLuint _detachAndDestroy(GLuint program, GLuint vs, GLuint ps);
  static TextureSamplerUniform _createTextureSamplerUniform(GLuint quadShader, const char* name);
  static TextureSamplerUniform _createTextureSampler();
};