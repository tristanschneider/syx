#pragma once
#include "file/FileSystem.h"

namespace FileSystem {
  struct TestFileSystem : public IFileSystem {
    virtual ~TestFileSystem() = default;
    virtual FileResult readFile(const char* filename, std::vector<uint8_t> buffer) = 0;
    virtual FileResult writeFile(const char* filename, std::string_view buffer) = 0;
    virtual bool isDirectory(const char* path) = 0;
    virtual bool fileExists(const char* path) = 0;
    virtual bool forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) = 0;

    void removeFile(const std::string& filename);
    void addDirectory(std::string path);

    std::unordered_map<std::string, std::string> mFiles;
  };
}