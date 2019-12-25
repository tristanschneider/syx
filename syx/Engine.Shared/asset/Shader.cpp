#include "Precompile.h"
#include "Shader.h"

#include <gl/glew.h>


Shader::Binder::Binder(const Shader& shader) {
  glUseProgram(shader.getId());
}

Shader::Binder::~Binder() {
  glUseProgram(0);
}

namespace {
  void _getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result) {
    result = GL_FALSE;
    glGetShaderiv(handle, status, &result);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
  }

  void _compileShader(GLuint shaderHandle, const std::string& source) {
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
}

void Shader::load() {
  //Create the shaders
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);

  _compileShader(vs, mSourceVS);
  _compileShader(ps, mSourcePS);

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
    mState = AssetState::Failed;
    return;
  }

  //Once program is linked we can get rid of the individual shaders
  glDetachShader(mId, vs);
  glDetachShader(mId, ps);

  glDeleteShader(vs);
  glDeleteShader(ps);
  mState = AssetState::PostProcessed;
}

void Shader::unload() {
  glDeleteProgram(mId);
  mId = 0;
}

GLHandle Shader::getUniform(const std::string& name) {
  auto it = mUniformLocations.find(name);
  if(it != mUniformLocations.end())
    return it->second;
  GLuint newId = glGetUniformLocation(mId, name.c_str());
  mUniformLocations[name] = newId;
  return newId;
}

GLHandle Shader::getAttrib(const std::string& name) {
  auto it = mUniformLocations.find(name);
  if(it != mUniformLocations.end())
    return it->second;
  GLuint newId = glGetAttribLocation(mId, name.c_str());
  mUniformLocations[name] = newId;
  return newId;
}

GLHandle Shader::getId() const {
  return mId;
}

void Shader::set(std::string&& sourceVS, std::string&& sourcePS) {
  mSourceVS = std::move(sourceVS);
  mSourcePS = std::move(sourcePS);
}
