#include "Precompile.h"
#include "Shader.h"

void Shader::_getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result) {
  result = GL_FALSE;
  glGetShaderiv(handle, status, &result);
  glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
}

void Shader::compileShader(GLuint shaderHandle, const char* source) {
  //Compile Shader
  glShaderSource(shaderHandle, 1, &source, NULL);
  glCompileShader(shaderHandle);

  GLint result, logLen;
  _getStatusWithInfo(shaderHandle, GL_COMPILE_STATUS, logLen, result);
  //Check Shader
  if(logLen > 0) {
    std::string error(logLen + 1, 0);
    glGetShaderInfoLog(shaderHandle, logLen, NULL, &error[0]);
    printf("%s\n", error.c_str());
  }
}

bool Shader::_link(GLuint program) {
  std::string str;
  glLinkProgram(program);

  GLint success{};
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if(success == GL_FALSE) {
    str.resize(1000);
    glGetProgramInfoLog(program, str.size(), nullptr, str.data());
    printf("Error linking shader [%s]\n", str.c_str());
    return false;
  }
  return true;
}

bool Shader::_validate(GLuint program) {
  std::string str;
  GLint success{};

  glValidateProgram(program);

  glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
  if(success == GL_FALSE) {
    str.resize(1000);
    glGetProgramInfoLog(program, str.size(), nullptr, str.data());
    printf("Error validating shader\n [%s]", str.c_str());
    return false;
  }
  return true;
}

GLuint Shader::_detachAndDestroy(GLuint program, GLuint s) {
  glDetachShader(program, s);
  glDeleteShader(s);
  return program;
}

GLuint Shader::_detachAndDestroy(GLuint program, GLuint vs, GLuint ps) {
  _detachAndDestroy(program, vs);
  _detachAndDestroy(program, ps);
  return program;
}

GLuint Shader::loadShader(const char* vsSource, const char* psSource) {
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
  compileShader(vs, vsSource);
  compileShader(ps, psSource);

  GLuint result{ glCreateProgram() };
  glAttachShader(result, vs);
  glAttachShader(result, ps);
  if(!_link(result)) {
    return 0;
  }

  //Once program is linked we can get rid of the individual shaders
  return _detachAndDestroy(result, vs, ps);
}

TextureSamplerUniform Shader::_createTextureSamplerUniform(GLuint quadShader, const char* name) {
  TextureSamplerUniform result;
  result.uniformID = glGetUniformLocation(quadShader, name);
  glGenBuffers(1, &result.buffer);
  glGenTextures(1, &result.texture);
  return result;
}

TextureSamplerUniform Shader::_createTextureSampler() {
  TextureSamplerUniform result;
  glGenBuffers(1, &result.buffer);
  glGenTextures(1, &result.texture);
  return result;
}
