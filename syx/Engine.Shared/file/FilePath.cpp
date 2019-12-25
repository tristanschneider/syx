#include "Precompile.h"
#include "file/FilePath.h"

namespace {
  bool _isSlash(char c) {
    return c == '/' || c == '\\';
  }
}

const FilePath FilePath::EMPTY_PATH;

FilePath::FilePath(const char* path)
  : mSize(0) {
  _append(path);
}

FilePath::operator const char*() const {
  return &mPath.front();
}

const char* FilePath::cstr() const {
  return &mPath.front();
}

size_t FilePath::size() const {
  return mSize;
}

const char* FilePath::getExtensionWithoutDot() const {
  if(const char* result = getExtensionWithDot())
    return result + 1;
  return nullptr;
}

const char* FilePath::getExtensionWithDot() const {
  return _findLastOf(".");
}

const char* FilePath::getFileNameWithExtension() const {
  return _findLastOf("/\\");
}

FilePath FilePath::getFileNameWithoutExtension() const {
  FilePath path, name, ext;
  getParts(path, name, ext);
  return name;
}

FilePath FilePath::getPath() {
  FilePath path, name, ext;
  getParts(path, name, ext);
  return path;
}

bool FilePath::hasValidLength() const {
  return mSize != mPath.size();
}

bool FilePath::_beginsWithSlash() const {
  return mSize != 0 && _isSlash(mPath[0]);
}

bool FilePath::_endsWithSlash() const {
  return mSize != 0 && _isSlash(mPath[mSize - 1]);
}

size_t FilePath::_remainingSize() const {
  return mPath.size() - mSize;
}

FilePath FilePath::_substr(size_t begin, size_t chars) const {
  assert(begin + chars < mPath.size() && "Substring out of bounds");
  FilePath result;
  std::memcpy(result.mPath.data(), &mPath[begin], chars);
  result.mSize = chars;
  //Null terminator
  result._append(nullptr);
  return result;
}

namespace {
  bool _hasMatch(char c, const char* chars) {
    while(*chars) {
      if(*chars == c)
        return true;
      ++chars;
    }
    return false;
  }
}

const char* FilePath::_findLastOf(const char* c) const {
  for(size_t i = mSize; i > 0; --i)
    if(_hasMatch(mPath[i], c))
      return &mPath[i];
  return _hasMatch(mPath[0], c) ? mPath.data() : nullptr;
}

void FilePath::_append(const char* str) {
  const size_t oldSize = mSize;
  if(str) {
    const size_t charsToCopy = std::min(_remainingSize(), std::strlen(str));
    std::memcpy(&mPath[mSize], str, charsToCopy);
    mSize += charsToCopy;
  }
  //Add null terminator. If mSize is MAX_PATH then this is !hasValidLength and will be truncated with null terminator
  mPath[std::min(mSize, mPath.size() - 1)] = 0;
  //Normalize slashes
  std::replace(mPath.begin() + oldSize, mPath.begin() + mSize, '\\', '/');
}

FilePath FilePath::getRelativeTo(const FilePath& relative) const {
  if(!relative.size()) {
    return *this;
  }
  //If relative is found in this, remove it
  int cmp = std::strncmp(relative, *this, relative.size());
  if(!cmp) {
    size_t relativeBegin = relative.size();
    if(size() > relative.size() && _isSlash(mPath[relative.size()]))
      ++relativeBegin;
    return FilePath(&mPath[relativeBegin]);
  }
  return {};
}

FilePath FilePath::addExtension(const char* extension) const {
  assert(extension && "Extension to add must not be null");
  FilePath result(*this);
  //Remove existing extension if there is one
  if(const char* existing = result.getExtensionWithDot())
    result.mSize = existing - result.cstr();
  //Add dot if it isn't in extension
  if(extension[0] != '.')
    result._append(".");
  result._append(extension);
  return result;
}

void FilePath::getParts(FilePath& path, FilePath& file, FilePath& extension) const {
  path = file = extension = FilePath();
  const char* pathEnd = _findLastOf("/\\");
  const char* ext = getExtensionWithDot();
  if(pathEnd)
    path = _substr(0, pathEnd - cstr());

  size_t fileBegin = 0;
  if(pathEnd) {
    fileBegin = pathEnd - cstr();
    //Move the beginning off of the slash
    if(fileBegin != 0)
      ++fileBegin;
  }
  size_t fileEnd = mSize;
  if(ext)
    fileEnd = ext - cstr();
  file = _substr(fileBegin, fileEnd - fileBegin);

  if(ext) {
    extension = _substr(ext - cstr(), mSize - (ext - cstr()));
  }
}

FilePath FilePath::join(const FilePath& lhs, const FilePath& rhs) {
  FilePath result(lhs);

  bool leftHasSlash = lhs._endsWithSlash();
  bool rightHasSlash = rhs._beginsWithSlash();
  //If neither have a slash we need to add one between them
  if(!leftHasSlash && !rightHasSlash)
    result._append("/");
  //If both have slashes then one needs to be removed
  else if(leftHasSlash && rightHasSlash)
    --result.mSize;
  //else only one has a slash, so nothing needs to change

  result._append(rhs);
  return result;
}
