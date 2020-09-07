#include "Precompile.h"
#include "file/FileSystem.h"
#include <filesystem>

namespace FileSystem {
  //Intended to be cross platform. If a platform ends up not supporting this part of the std the responsibility could be moved to AppPlatform
  struct STDFileSystem : public IFileSystem {
    FileResult readFile(const char* filename, std::vector<uint8_t> buffer) override {
      std::FILE* file = std::fopen(filename, "rb");
      if(!file)
        return FileResult::NotFound;

      std::fseek(file, 0, SEEK_END);
      size_t bytes = static_cast<size_t>(std::ftell(file));
      std::rewind(file);
      buffer.clear();
      buffer.resize(static_cast<size_t>(bytes));

      bool readSuccess = bytes == std::fread(&buffer[0], 1, bytes, file);
      std::fclose(file);
      return readSuccess ? FileResult::Success : FileResult::Fail;
    }

    FileResult writeFile(const char* filename, std::string_view buffer) override {
      std::FILE* file = std::fopen(filename, "wb");
      if(!file)
        return FileResult::IOError;

      bool success = true;
      if(!buffer.empty())
        success = std::fwrite(buffer.data(), sizeof(buffer[0]), buffer.size(), file) == buffer.size();

      std::fclose(file);
      return success ? FileResult::Success : FileResult::Fail;
    }

    bool isDirectory(const char* path) override {
      return std::filesystem::is_directory(path);
    }

    bool fileExists(const char* path) override {
      if(std::FILE* file = std::fopen(path, "rb")) {
        std::fclose(file);
        return true;
      }
      return false;
    }

    bool forEachInDirectoryRecursive(const char* directory, const std::function<void(std::string_view)>& func) override {
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
  };

  std::unique_ptr<IFileSystem> createStd() {
    return std::make_unique<STDFileSystem>();
  }
}