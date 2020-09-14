#pragma once
#include "file/FileSystem.h"

namespace FileSystem {
  struct TestFileSystem : public IFileSystem {
    virtual ~TestFileSystem() = default;

    virtual FileResult readFile(const char* filename, std::vector<uint8_t>& buffer) override;
    virtual FileResult writeFile(const char* filename, std::string_view buffer) override;
    virtual bool isDirectory(const char* path) override;
    virtual bool fileExists(const char* path) override;
    virtual bool forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) override;

    void removeFile(const std::string& filename);
    void addDirectory(std::string path);

    std::unordered_map<std::string, std::string> mFiles;
    //Needs to be recursive because of foreach callback
    std::recursive_mutex mMutex;
    using Lock = std::lock_guard<decltype(mMutex)>;
  };
}