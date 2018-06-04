#pragma once

//This class allows building, manipulating, and querying paths without the need for allocations
class FilePath {
public:
  //Max path on windows is 260, add one for null terminator
  static const size_t MAX_PATH = 261;
  static const FilePath EMPTY_PATH;

  FilePath(const char* path = nullptr);

  //Both ways to get the same null terminated string
  operator const char*() const;
  const char* cstr() const;
  size_t size() const;

  const char* getExtensionWithoutDot() const;
  const char* getExtensionWithDot() const;
  bool hasValidLength() const;

  //Get this path relative to the given path, meaning that if this path begins with relative that part is removed
  FilePath getRelativeTo(const FilePath& relative) const;
  //Add or replace the given extension. Dot will be placed if necessary
  FilePath addExtension(const char* extension) const;
  void getParts(FilePath& path, FilePath& file, FilePath& extension) const;
  static FilePath join(const FilePath& lhs, const FilePath& rhs);

private:
  bool _beginsWithSlash() const;
  bool _endsWithSlash() const;
  void _append(const char* str);
  size_t _remainingSize() const;
  FilePath _substr(size_t begin, size_t chars) const;
  const char* _findLastOf(const char* c) const;

  std::array<char, MAX_PATH> mPath;
  //Size of path. If this is MAX_PATH, that indicates the path is too big
  size_t mSize;
};