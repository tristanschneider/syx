#include "Precompile.h"
#include "file/FilePath.h"
#include "test/TestFileSystem.h"

namespace FileSystem {
  FileResult TestFileSystem::readFile(const char* filename, std::vector<uint8_t> buffer) {
    if(isDirectory(filename)) {
      return FileResult::NotFound;
    }
    if(auto it = mFiles.find(std::string(filename)); it != mFiles.end()) {
      const std::string& file = it->second;
      //+1 for null terminator
      buffer.resize(file.size() + 1);
      std::memcpy(buffer.data(), file.data(), file.size() + 1);
      return FileResult::Success;
    }
    return FileResult::NotFound;
  }

  FileResult TestFileSystem::writeFile(const char* filename, std::string_view buffer) {
    mFiles[std::string(filename)] = std::string(buffer);
    return FileResult::Success;
  }

  bool TestFileSystem::isDirectory(const char* path) {
    const FilePath root(path);
    //If it has an extension it's not a directory, otherwise see if anything is relative to it
    return !root.getExtensionWithoutDot() && std::any_of(mFiles.begin(), mFiles.end(), [path, &root](const auto& pair) {
      return FilePath(pair.first).getRelativeTo(root).size();
    });
  }

  bool TestFileSystem::fileExists(const char* path) {
    auto exists = mFiles.find(std::string(path));
    //Doesn't distinguish between directories and files like it should. Hopefully that's fine for tests
    return exists != mFiles.end();
  }

  bool TestFileSystem::forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) {
    //TODO: does this need to be safe against iterator invalidation during callback?
    const FilePath base(directory);
    bool any = false;
    for(const auto& pair : mFiles) {
      if(FilePath(pair.first).getRelativeTo(base).size()) {
        func(pair.first);
        any = true;
      }
    }
    return any;
  }

  void TestFileSystem::removeFile(const std::string& filename) {
    if(auto it = mFiles.find(filename); it != mFiles.end()) {
      mFiles.erase(it);
    }
  }

  void TestFileSystem::addDirectory(std::string path) {
    mFiles[std::move(path)];
  }
}