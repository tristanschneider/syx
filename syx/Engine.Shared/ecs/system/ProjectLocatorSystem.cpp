#include "ecs/system/ProjectLocatorSystem.h"

#include "ecs/component/AppPlatformComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/ProjectLocatorComponent.h"
#include "ecs/component/UriActivationComponent.h"

namespace {
  using namespace Engine;
  void tickPLUriListener(SystemContext
    < View<Write<FileSystemComponent>, Write<ProjectLocatorComponent>>
    , View<Read<UriActivationComponent>>
    , EntityModifier<SetWorkingDirectoryComponent>
    > context) {

    auto& global = context.get<View<Write<FileSystemComponent>, Write<ProjectLocatorComponent>>>();
    auto& view = context.get<View<Read<UriActivationComponent>>>();
    auto modifier = context.get<EntityModifier<SetWorkingDirectoryComponent>>();

    if(global.begin() == global.end()) {
      //Global components missing, can't do anything
      return;
    }

    auto ge = *global.begin();
    auto& fileSystem = ge.get<FileSystemComponent>();
    auto& projectLocator = ge.get<ProjectLocatorComponent>();

    for(auto chunks = view.chunksBegin(); chunks != view.chunksEnd(); ++chunks) {
      const std::vector<UriActivationComponent>& components = *(*chunks).tryGet<const UriActivationComponent>();
      //AddComponent moves the entity to another chunk with a swap remove, in which case `i` doesn't need to be advanced
      for(size_t i = 0; i < components.size();) {
        auto params = UriActivationComponent::parseUri(components[i].mUri);
        const auto it = params.find("projectRoot");
        if(it != params.end() && fileSystem.get().isDirectory(it->second.c_str())) {
          printf("Project root set to %s\n", it->second.c_str());
          projectLocator.get().setPathRoot(it->second.c_str(), PathSpace::Project);

          modifier.addComponent<SetWorkingDirectoryComponent>((*chunks).indexToEntity(i), SetWorkingDirectoryComponent{ FilePath(it->second) });
        }
        else {
          //Component wasn't added, meaning entity is still in this chunk, so advance `i` manually
          ++i;
        }
      }
    }
  }
}

std::unique_ptr<Engine::System> ProjectLocatorSystem::createUriListener() {
  return ecx::makeSystem("ProjectLocatorUriSystem", &tickPLUriListener);
}