#include "Precompile.h"
#include "Shader.h"

void Shader::_getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result) {
  handle;status;logLen;result;
}

void Shader::compileShader(GLuint shaderHandle, const char* source) {
  shaderHandle;source;
}

bool Shader::_link(GLuint program) {
  program;
  return {};
}

bool Shader::_validate(GLuint program) {
  program;
  return true;
}

GLuint Shader::_detachAndDestroy(GLuint program, GLuint s) {
  s;
  return program;
}

GLuint Shader::_detachAndDestroy(GLuint program, GLuint vs, GLuint ps) {
  _detachAndDestroy(program, vs);
  _detachAndDestroy(program, ps);
  return program;
}

GLuint Shader::loadShader(const char* vsSource, const char* psSource) {
  vsSource;psSource;
  return {};
}

TextureSamplerUniform Shader::_createTextureSamplerUniform(GLuint quadShader, const char* name) {
  quadShader;name;
  return {};
}

TextureSamplerUniform Shader::_createTextureSampler() {
  return {};
}
