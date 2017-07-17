#include "Shader.h"

Shader::Binder::Binder(const Shader& shader) {
  glUseProgram(shader.getId());
}

Shader::Binder::~Binder() {
  glUseProgram(0);
}

static void _getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result) {
  result = GL_FALSE;
  glGetShaderiv(handle, status, &result);
  glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
}

static void _compileShader(GLuint shaderHandle, const std::string& source) {
  //Compile Shader
  const char* cstr = source.c_str();
  glShaderSource(shaderHandle, 1, &cstr, NULL);
  glCompileShader(shaderHandle);

  //GL_COMPILE_STATUS
  GLint result, logLen;
  _getStatusWithInfo(shaderHandle, GL_COMPILE_STATUS, logLen, result);
  //Check Shader
  if(logLen > 0) {
    std::string error(logLen + 1, 0);
    glGetShaderInfoLog(shaderHandle, logLen, NULL, &error[0]);
    printf("%s\n", error.c_str());
  }
}

bool Shader::load(const std::string& vsSource, const std::string& psSource) {
  //Create the shaders
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);

  _compileShader(vs, vsSource);
  _compileShader(ps, psSource);

  //Link the program
  mId = glCreateProgram();
  glAttachShader(mId, vs);
  glAttachShader(mId, ps);
  glLinkProgram(mId);

  GLint result, logLen;
  _getStatusWithInfo(mId, GL_LINK_STATUS, logLen, result);
  if(logLen > 0) {
    std::string error(logLen + 1, 0);
    glGetProgramInfoLog(mId, logLen, NULL, &error[0]);
    printf("%s\n", error.c_str());
    return false;
  }

  //Once program is linked we can get rid of the individual shaders
  glDetachShader(mId, vs);
  glDetachShader(mId, ps);

  glDeleteShader(vs);
  glDeleteShader(ps);
  return true;
}

GLuint Shader::getUniform(const std::string& name) {
  auto it = mUniformLocations.find(name);
  if(it != mUniformLocations.end())
    return it->second;
  GLuint newId = glGetUniformLocation(mId, name.c_str());
  mUniformLocations[name] = newId;
  return newId;
}

GLuint Shader::getId() const {
  return mId;
}
