#pragma once

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