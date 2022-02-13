#pragma once

//This class allows building, manipulating, and querying paths without the need for allocations
class FilePath {
public:
  //Max path on windows is 260, add one for null terminator
  static const size_t MAX_FILE_PATH = 261;
  static const FilePath EMPTY_PATH;

  explicit FilePath(const std::string& str);
  FilePath(const char* path = nullptr);

  //Both ways to get the same null terminated string
  operator const char*() const;
  const char* cstr() const;
  size_t size() const;

  const char* getExtensionWithoutDot() const;
  const char* getExtensionWithDot() const;
  const char* getFileNameWithExtension() const;
  FilePath getFileNameWithoutExtension() const;
  FilePath getPath();
  bool hasValidLength() const;

  //Get this path relative to the given path, meaning that if this path begins with relative that part is removed
  FilePath getRelativeTo(const FilePath& relative) const;
  //Add or replace the given extension. Dot will be placed if necessary
  FilePath addExtension(const char* extension) const;
  void getParts(FilePath& path, FilePath& file, FilePath& extension) const;
  static FilePath join(const FilePath& lhs, const FilePath& rhs);

  bool operator==(const FilePath& rhs) const;

private:
  bool _beginsWithSlash() const;
  bool _endsWithSlash() const;
  void _append(const char* str);
  size_t _remainingSize() const;
  FilePath _substr(size_t begin, size_t chars) const;
  const char* _findLastOf(const char* c) const;

  std::array<char, MAX_FILE_PATH> mPath;
  //Size of path. If this is MAX_FILE_PATH, that indicates the path is too big
  size_t mSize;
};

namespace std {
  template<>
  struct hash<FilePath> {
    typedef FilePath argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& path) const noexcept {
      return std::hash<std::string_view>()(std::string_view(path.cstr(), path.size()));
    }
  };
}
