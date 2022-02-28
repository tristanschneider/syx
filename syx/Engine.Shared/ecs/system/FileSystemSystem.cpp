#include "Precompile.h"
#include "ecs/system/FileSystemSystem.h"

#include "ecs/component/FileSystemComponent.h"
#include "file/FileSystem.h"

namespace FileWriter {
  using namespace Engine;
  using Modifier = EntityModifier<FileWriteSuccessResponse, FileWriteFailureResponse>;
  using Requests = View<Read<FileWriteRequest>, Exclude<FileWriteSuccessResponse>, Exclude<FileWriteFailureResponse>>;

  void _tick(SystemContext<Requests, View<Write<FileSystemComponent>>, Modifier>& context) {
    auto& view = context.get<Requests>();
    auto modifier = context.get<Modifier>();
    auto fsc = context.get<View<Write<FileSystemComponent>>>().tryGetFirst();
    if(!fsc) {
      return;
    }
    FileSystem::IFileSystem& fs = fsc->get<FileSystemComponent>().get();

    for(auto chunk : view.chunks()) {
      const std::vector<FileWriteRequest>& requests = *chunk.tryGet<const FileWriteRequest>();
      while(!requests.empty()) {
        const FileWriteRequest& request = requests.front();
        if(fs.writeFile(request.mToWrite.cstr(), std::string_view(reinterpret_cast<const char*>(request.mBuffer.data()), request.mBuffer.size())) == FileSystem::FileResult::Success) {
          modifier.addComponent<FileWriteSuccessResponse>(chunk.indexToEntity(0));
        }
        else {
          modifier.addComponent<FileWriteFailureResponse>(chunk.indexToEntity(0));
        }
      }
    }
  }
}

//TODO: async operations
std::shared_ptr<Engine::System> FileSystemSystem::fileReader() {
  using namespace Engine;
  using Modifier = EntityModifier<FileReadSuccessResponse, FileReadFailureResponse>;
  using Requests = View<Read<FileReadRequest>, Exclude<FileReadSuccessResponse>, Exclude<FileReadFailureResponse>>;
  return ecx::makeSystem("fileReader", [](SystemContext<Requests, View<Write<FileSystemComponent>>, Modifier>& context) {
    auto& view = context.get<Requests>();
    auto modifier = context.get<Modifier>();
    auto fsc = context.get<View<Write<FileSystemComponent>>>().tryGetFirst();
    if(!fsc) {
      return;
    }
    FileSystem::IFileSystem& fs = fsc->get<FileSystemComponent>().get();

    for(auto chunk : view.chunks()) {
      const std::vector<FileReadRequest>& requests = *chunk.tryGet<const FileReadRequest>();
      while(!requests.empty()) {
        const FileReadRequest& request = requests.front();
        std::vector<uint8_t> buffer;
        if(fs.readFile(request.mToRead.cstr(), buffer) == FileSystem::FileResult::Success) {
          modifier.addComponent<FileReadSuccessResponse>(chunk.indexToEntity(0), std::move(buffer));
        }
        else {
          modifier.addComponent<FileReadFailureResponse>(chunk.indexToEntity(0));
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> FileSystemSystem::fileWriter() {
  return ecx::makeSystem("fileWriter", &FileWriter::_tick);
}

std::shared_ptr<Engine::System> FileSystemSystem::addFileSystemComponent(std::unique_ptr<FileSystem::IFileSystem> fs) {
  using namespace Engine;
  using Modifier = EntityModifier<FileSystemComponent>;
  using ContextType = SystemContext<EntityFactory, Modifier>;

  struct InitSystem : public ecx::System<ContextType, Entity> {
    void _tick(ContextType& context) const override {
      if(mFS) {
        context.get<Modifier>().addComponent<FileSystemComponent>(context.get<EntityFactory>().createEntity(), std::move(mFS));
      }
    }

    ecx::SystemInfo getInfo() const override {
      ecx::SystemInfo info = ecx::System<ContextType, Entity>::getInfo();
      info.mName = "FileSystemInit";
      return info;
    }

    mutable std::unique_ptr<FileSystem::IFileSystem> mFS;
  };

  auto result = std::make_unique<InitSystem>();
  result->mFS = std::move(fs);
  return result;
}
