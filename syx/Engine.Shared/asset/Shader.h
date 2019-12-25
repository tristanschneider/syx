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
  GLHandle getUniform(const std::string& name);
  GLHandle getAttrib(const std::string& name);
  GLHandle getId() const;
  void set(std::string&& sourceVS, std::string&& sourcePS);

private:
  GLHandle mId;
  std::string mSourceVS;
  std::string mSourcePS;
  std::unordered_map<std::string, GLHandle> mUniformLocations;
};