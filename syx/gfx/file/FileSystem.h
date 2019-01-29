#pragma once

namespace FileSystem {
  enum class FileResult {
    NotFound,
    IOError,
    Fail,
    Success
  };

  template<typename Buffer>
  FileResult readFile(const char* filename, Buffer& buffer) {
    std::FILE* file = std::fopen(filename, "rb");
    if(!file)
      return FileResult::NotFound;

    std::fseek(file, 0, SEEK_END);
    long bytes = std::ftell(file);
    std::rewind(file);
    buffer.clear();
    buffer.resize(static_cast<size_t>(bytes));

    bool readSuccess = bytes == std::fread(&buffer[0], 1, bytes, file);
    std::fclose(file);
    return readSuccess ? FileResult::Success : FileResult::Fail;
  }

  template<typename Buffer>
  FileResult writeFile(const char* filename, const Buffer& buffer) {
    std::FILE* file = std::fopen(filename, "wb");
    if(!file)
      return FileResult::IOError;

    bool success = true;
    if(!buffer.empty())
      success = std::fwrite(buffer.data(), sizeof(buffer[0]), buffer.size(), file) == buffer.size();

    std::fclose(file);
    return success ? FileResult::Success : FileResult::Fail;
  }

  template<class Func>
  bool forEachInDirectoryRecursive(const char* directory, const Func& func) {
    assert(isDirectory(directory) && "Should only be called with directories");
    bool any = false;
    for(auto it : std::filesystem::recursive_directory_iterator(directory)) {
      if(!it.is_directory()) {
        func(it.path().u8string());
        any = true;
      }
    }
    return any;
  }

  inline bool isDirectory(const char* path) {
    return std::filesystem::is_directory(path);
  }

  bool fileExists(const char* filename);
};