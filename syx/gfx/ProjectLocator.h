#pragma once

class FilePath;

//Abstracts all conversions to and from full paths that are relevant to the engine
//This makes it easier to change project structure without having to change all uses of paths
class ProjectLocator {
public:
  //Space the given path is in, so Project means path is relative to project while full means its the full path
  enum class PathSpace : uint8_t {
    Project,
    Full
  };

  ProjectLocator();
  ~ProjectLocator();

  void setPathRoot(const char* path, PathSpace pathSpace);
  //Convert a path in the from space to a path in the to space,
  //so to making a project relative path full would be transform(path, PathSpace::Project, PathSpace::Full)
  FilePath transform(const char* path, PathSpace from, PathSpace to) const;

private:
  const FilePath& _getPathRoot(PathSpace space) const;
  FilePath& _getPathRoot(PathSpace space);

  std::unique_ptr<FilePath> mProjectRoot;
};