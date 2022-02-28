#pragma once

#include "ecs/ECS.h"

namespace FileSystem {
  struct IFileSystem;
};

struct FileSystemSystem {
  static std::shared_ptr<Engine::System> fileReader();
  static std::shared_ptr<Engine::System> fileWriter();
  static std::shared_ptr<Engine::System> addFileSystemComponent(std::unique_ptr<FileSystem::IFileSystem> fs);
};
