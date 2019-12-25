#include "Precompile.h"
#include "ProjectLocator.h"

#include "file/FilePath.h"

ProjectLocator::ProjectLocator()
  : mProjectRoot(std::make_unique<FilePath>()) {
}

ProjectLocator::~ProjectLocator() {
}

void ProjectLocator::setPathRoot(const char* path, PathSpace pathSpace) {
  assert(pathSpace != PathSpace::Full && "Setting full path root doesn't make sense");
  if(pathSpace != PathSpace::Full)
    _getPathRoot(pathSpace) = FilePath(path);
}

FilePath ProjectLocator::transform(const char* path, PathSpace from, PathSpace to) const {
  //If it's already in the desired space there's nothing to do
  if(from == to)
    return FilePath(path);

  FilePath result(path);
  //Transform relative to full. Not needed if already full
  if(from != PathSpace::Full)
    result = FilePath::join(_getPathRoot(from), result);

  //Transform from full to desired path. Not needed if desired is full
  if(to != PathSpace::Full)
    result = result.getRelativeTo(_getPathRoot(to));

  return result;
}

const FilePath& ProjectLocator::_getPathRoot(PathSpace space) const {
  switch(space) {
    case PathSpace::Project: return *mProjectRoot;
    default: assert(false && "Unhandled path space");
    case PathSpace::Full: return FilePath::EMPTY_PATH;
  }
}

FilePath& ProjectLocator::_getPathRoot(PathSpace space) {
  return const_cast<FilePath&>(const_cast<const ProjectLocator*>(this)->_getPathRoot(space));
}
