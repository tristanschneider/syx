#pragma once

#include "file/FilePath.h"
#include "file/FileSystem.h"

class FileSystemComponent {
public:
  FileSystemComponent() = default;
  FileSystemComponent(std::unique_ptr<FileSystem::IFileSystem> file)
    : mFile(std::move(file)) {
  }

  FileSystemComponent(FileSystemComponent&&) = default;
  FileSystemComponent& operator=(FileSystemComponent&&) = default;

  FileSystem::IFileSystem& get() {
    return *mFile;
  }

  const FileSystem::IFileSystem& get() const {
    return *mFile;
  }

private:
  std::unique_ptr<FileSystem::IFileSystem> mFile;
};

//Put on an entity to request loading of a file
struct FileReadRequest {
  FilePath mToRead;
};

struct FileReadSuccessResponse {
  std::vector<uint8_t> mBuffer;
};

struct FileReadFailureResponse {
};

struct FileWriteRequest {
  FilePath mToWrite;
  std::vector<uint8_t> mBuffer;
};

struct FileWriteSuccessResponse {
};

struct FileWriteFailureResponse {
};