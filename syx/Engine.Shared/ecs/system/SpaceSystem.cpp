#include "Precompile.h"
#include "ecs/system/SpaceSystem.h"

#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/system/RemoveFromAllEntitiesSystem.h"

std::shared_ptr<Engine::System> SpaceSystem::clearSpaceSystem() {
  using namespace Engine;
  using SpaceView = View<Include<SpaceTagComponent>>;
  using EntityView = View<Read<InSpaceComponent>>;
  using MessageView = View<Read<ClearSpaceComponent>>;
  return ecx::makeSystem("ClearSpaceSystem", [](SystemContext<EntityFactory, SpaceView, EntityView, MessageView>& context) {
    EntityFactory factory = context.get<EntityFactory>();
    auto& spaces = context.get<SpaceView>();

    //Gather all spaces to clear from all ClearSpaceComponents
    std::vector<Entity> spacesToClear;
    for(auto messageChunks : context.get<MessageView>().chunks()) {
      for(const ClearSpaceComponent& msg : *messageChunks.tryGet<const ClearSpaceComponent>()) {
        if(auto spaceToClear = spaces.find(msg.mSpace); spaceToClear != spaces.end()) {
          spacesToClear.push_back((*spaceToClear).entity());
        }
      }
    }

    //Remove all entities that are in cleared spaces
    if(!spacesToClear.empty()) {
      //TODO: optimization with compile time space tags
      for(auto entityChunks : context.get<EntityView>().chunks()) {
        const auto entities = entityChunks.tryGet<const InSpaceComponent>();
        for(size_t i = 0; i < entities->size();) {
          if(std::find(spacesToClear.begin(), spacesToClear.end(), entities->at(i).mSpace) != spacesToClear.end()) {
            //TODO: this operation is more expensive than it needs to be, should use the chunk to avoid lookups
            factory.destroyEntity(entityChunks.indexToEntity(i));
          }
          else {
            ++i;
          }
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::beginLoadSpaceSystem() {
  using namespace Engine;
  using MessageView = View<Read<LoadSpaceComponent>>;
  using Modifier = EntityModifier<SpaceLoadingComponent, FileReadRequest>;
  return ecx::makeSystem("beginLoadSpace", [](SystemContext<MessageView, Modifier>& context) {
    auto modifier = context.get<Modifier>();
    for(auto messageChunk : context.get<MessageView>().chunks()) {
      for(const auto& message : *messageChunk.tryGet<const LoadSpaceComponent>()) {
        modifier.addDeducedComponent(message.mSpace, FileReadRequest{ message.mToLoad });
        modifier.addComponent<SpaceLoadingComponent>(message.mSpace);
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::parseSceneSystem() {
  //TODO:
  return nullptr;
}

std::shared_ptr<Engine::System> SpaceSystem::createSpaceEntitiesSystem() {
  using namespace Engine;
  using Modifier = EntityModifier<InSpaceComponent, SpaceFillingEntitiesComponent>;
  using SpaceView = View<Write<ParsedSpaceContentsComponent>>;
  return ecx::makeSystem("createSpaceEntities", [](SystemContext<EntityFactory, Modifier, SpaceView>& context) {
    auto modifier = context.get<Modifier>();

    for(auto spaceChunk : context.get<SpaceView>().chunks()) {
      //Create all entities
      for(auto& parsedSpaceContent : *spaceChunk.tryGet<ParsedSpaceContentsComponent>()) {
        //TODO: fill mNewEntities here with some parsed data
        (void)parsedSpaceContent;
      }

      //Tag the space as loading entities
      while(spaceChunk.size()) {
        modifier.addComponent<SpaceFillingEntitiesComponent>(spaceChunk.indexToEntity(0));
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::completeSpaceLoadSystem() {
  using namespace Engine;
  //Remove intermediate loading components from space at the end of the tick that it had SpaceFillingEntitiesComponent
  //In the future there may need to be a mechanism to delay the destruction for multiple frames to allow deferred loading
  return removeFomAllEntitiesInView<View<Include<SpaceLoadingComponent>, Include<SpaceFillingEntitiesComponent>>
    , SpaceLoadingComponent
    , ParsedSpaceContentsComponent
    , SpaceFillingEntitiesComponent
  >();
}

std::shared_ptr<Engine::System> SpaceSystem::beginSaveSpaceSystem() {
  //TODO
  return nullptr;
}

std::shared_ptr<Engine::System> SpaceSystem::createSerializedEntitiesSystem() {
  //TODO
  return nullptr;
}

std::shared_ptr<Engine::System> SpaceSystem::serializeSpaceSystem() {
  //TODO
  return nullptr;
}

std::shared_ptr<Engine::System> SpaceSystem::completeSpaceSaveSystem() {
  using namespace Engine;
  return removeFomAllEntitiesInView<View<Include<SpaceSavingComponent>, Include<SpaceFillingEntitiesComponent>>
    , SpaceSavingComponent
    , ParsedSpaceContentsComponent
    , SpaceFillingEntitiesComponent
  >();
}
