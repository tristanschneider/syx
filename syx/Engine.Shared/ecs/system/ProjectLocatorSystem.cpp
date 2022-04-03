#include "ecs/system/ProjectLocatorSystem.h"

#include "ecs/component/AppPlatformComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/ProjectLocatorComponent.h"
#include "ecs/component/UriActivationComponent.h"

namespace {
  using namespace Engine;

  void tickPLUriListener(SystemContext
    < View<Write<FileSystemComponent>>
    , View<Write<ProjectLocatorComponent>>
    , View<Read<UriActivationComponent>>
    , EntityModifier<SetWorkingDirectoryComponent>
    > context) {

    auto fs = context.get<View<Write<FileSystemComponent>>>().tryGetFirst();
    auto pl = context.get<View<Write<ProjectLocatorComponent>>>().tryGetFirst();
    auto& view = context.get<View<Read<UriActivationComponent>>>();
    auto modifier = context.get<EntityModifier<SetWorkingDirectoryComponent>>();

    if(!fs || !pl) {
      //Global components missing, can't do anything
      return;
    }

    for(auto chunks = view.chunksBegin(); chunks != view.chunksEnd(); ++chunks) {
      const std::vector<UriActivationComponent>& components = *(*chunks).tryGet<const UriActivationComponent>();
      //AddComponent moves the entity to another chunk with a swap remove, in which case `i` doesn't need to be advanced
      for(size_t i = 0; i < components.size();) {
        std::optional<SetWorkingDirectoryComponent> setDir = ProjectLocatorSystem::tryParseSetWorkingDirectory(components[i]);
        if(setDir && fs->get<FileSystemComponent>().get().isDirectory(setDir->mDirectory)) {
          printf("Project root set to %s\n", setDir->mDirectory.cstr());
          pl->get<ProjectLocatorComponent>().get().setPathRoot(setDir->mDirectory.cstr(), PathSpace::Project);
          modifier.addComponent<SetWorkingDirectoryComponent>((*chunks).indexToEntity(i), std::move(*setDir));
        }
        else {
          //Component wasn't added, meaning entity is still in this chunk, so advance `i` manually
          ++i;
        }
      }
    }
  }
}

namespace initPL {
  using namespace Engine;
  void tick(SystemContext<EntityFactory, EntityModifier<ProjectLocatorComponent>>& context) {
    //Add an entity with the project locator
    auto factory = context.get<EntityFactory>();
    auto modifier = context.get<EntityModifier<ProjectLocatorComponent>>();
    //Arbitrary entity to hold the project locator
    auto entity = factory.createEntity();
    modifier.addDeducedComponent(entity, ProjectLocatorComponent{ std::make_unique<ProjectLocator>() });
  }
}

std::optional<SetWorkingDirectoryComponent> ProjectLocatorSystem::tryParseSetWorkingDirectory(const UriActivationComponent& uri) {
  auto params = UriActivationComponent::parseUri(uri.mUri);
  const auto it = params.find("projectRoot");
  if(it != params.end()) {
    return SetWorkingDirectoryComponent{ FilePath(it->second) };
  }
  return {};
}

std::unique_ptr<Engine::System> ProjectLocatorSystem::init() {
  return ecx::makeSystem("InitProjectLocator", &initPL::tick);
}


std::unique_ptr<Engine::System> ProjectLocatorSystem::createUriListener() {
  return ecx::makeSystem("ProjectLocatorUriSystem", &tickPLUriListener);
}