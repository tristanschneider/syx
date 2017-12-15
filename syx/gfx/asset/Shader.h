#pragma once
#include "asset/Asset.h"

class Shader : public Asset {
public:
  using Asset::Asset;

  struct Binder {
    Binder(const Shader& shader);
    ~Binder();
  };

  bool load(const std::string& vsSource, const std::string& psSource);
  void unload();
  GLuint getUniform(const std::string& name);
  GLuint getAttrib(const std::string& name);
  GLuint getId() const;

private:
  GLuint mId;
  std::unordered_map<std::string, GLuint> mUniformLocations;
};