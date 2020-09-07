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
    virtual FileResult readFile(const char* filename, std::vector<uint8_t>& buffer) = 0;
    virtual FileResult writeFile(const char* filename, std::string_view buffer) = 0;
    virtual bool isDirectory(const char* path) = 0;
    virtual bool fileExists(const char* path) = 0;
    virtual bool forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) = 0;
  };

  std::unique_ptr<IFileSystem> createStd();
};