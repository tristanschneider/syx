#pragma once

class Shader {
public:
  struct Binder {
    Binder(const Shader& shader);
    ~Binder();
  };

  bool load(const std::string& vsSource, const std::string& psSource);
  GLuint getUniform(const std::string& name);
  GLuint getId() const;

private:
  GLuint mId;
  std::unordered_map<std::string, GLuint> mUniformLocations;
};