#pragma once

namespace FileSystem {
  enum class FileResult {
    NotFound,
    IOError,
    Fail,
    Success
  };

  struct IFileSystem {
    virtual ~IFileSystem() = default;
    virtual FileResult readFile(const char* filename, std::vector<uint8_t> buffer) = 0;
    virtual FileResult writeFile(const char* filename, std::string_view buffer) = 0;
    virtual bool isDirectory(const char* path) = 0;
    virtual bool fileExists(const char* path) = 0;
    virtual bool forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) = 0;
  };

  //TODO: probably better to have more explicit ownership of this
  IFileSystem& get();
  //Caller is responsible of thread safety between get and set. Presumably the file system only needs to be set once on startup or during a test
  void _set(std::unique_ptr<IFileSystem> fs);

  std::unique_ptr<IFileSystem> createStd();
};