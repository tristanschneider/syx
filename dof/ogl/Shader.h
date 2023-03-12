#pragma once

#include "GL/glew.h"

struct Shader {
  static void compileShader(GLuint shaderHandle, const char* source);
  static GLuint loadShader(const char* vsSource, const char* psSource);

  static void _getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result);
  static bool _linkAndValidate(GLuint program);
  static GLuint _detachAndDestroy(GLuint program, GLuint vs, GLuint ps);
};