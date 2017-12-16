#pragma once
#include "asset/Asset.h"

class Shader : public Asset {
public:
  using Asset::Asset;

  struct Binder {
    Binder(const Shader& shader);
    ~Binder();
  };

  void load();
  void unload();
  GLuint getUniform(const std::string& name);
  GLuint getAttrib(const std::string& name);
  GLuint getId() const;
  void set(std::string&& sourceVS, std::string&& sourcePS);

private:
  GLuint mId;
  std::string mSourceVS;
  std::string mSourcePS;
  std::unordered_map<std::string, GLuint> mUniformLocations;
};