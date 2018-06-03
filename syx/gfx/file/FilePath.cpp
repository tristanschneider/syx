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
  //Look from the end to find the dot. Dot at first character is not valid
  for(size_t i = mSize; i > 0; --i)
    if(mPath[i] == '.')
      return &mPath[i];
  return nullptr;
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

void FilePath::_append(const char* str) {
  if(str) {
    size_t charsToCopy = std::min(_remainingSize(), std::strlen(str));
    std::memcpy(&mPath[mSize], str, charsToCopy);
    mSize += charsToCopy;
    //Add null terminator. If mSize is MAX_PATH then this is !hasValidLength and will be truncated with null terminator
    mPath[std::min(mSize, mPath.size() - 1)] = 0;
  }
}

FilePath FilePath::getRelativeTo(const FilePath& relative) const {
  FilePath result(*this);
  //If relative is found in this, remove it
  if(!std::strncmp(relative, result, result.mPath.size())) {
    //Reset the size and append 
    result.mSize = 0;
    result._append(&mPath[relative.mSize]);
  }
  return result;
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
