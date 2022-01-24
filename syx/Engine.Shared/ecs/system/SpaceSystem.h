#pragma once

#include "ecs/ECS.h"

struct SpaceSystem {
  //Clear all entities in space when a message with ClearSpaceComponent exists
  static std::shared_ptr<Engine::System> clearSpaceSystem();
  //Adds the load file request on the space for the file system to load the contents of the scene
  static std::shared_ptr<Engine::System> beginLoadSpaceSystem();
  //After the file is loaded, this parses the raw file into a list of objects to load
  static std::shared_ptr<Engine::System> parseSceneSystem();
  //Once the file is loaded this creates all the entities for the scene with the InSpaceComponent
  //From here on further systems can view the scene contents to create their components on the new entities
  static std::shared_ptr<Engine::System> createSpaceEntitiesSystem();
  //Removes the SpaceLoadingComponent and SpaceFillingEntitiesComponent for the space after it finishes loading
  //Register after all systems have done what they want with ParsedSpaceContentsComponent
  static std::shared_ptr<Engine::System> completeSpaceLoadSystem();

  //Add SpaceSavingComponent, SpaceFillingEntitiesComopnent, and an empty ParsedSpaceContents component to be filled by upcoming systems
  static std::shared_ptr<Engine::System> beginSaveSpaceSystem();
  //Fills ParsedSpaceContentsComponent with the entity ids for all entities that need to be saved
  static std::shared_ptr<Engine::System> createSerializedEntitiesSystem();
  //Between above and below other systems should register their component serializers
  //Takes the ParsedSpaceContentsComponent and submits the FileWriteComponent message
  static std::shared_ptr<Engine::System> serializeSpaceSystem();
  //Remove all the intermediate saving components
  static std::shared_ptr<Engine::System> completeSpaceSaveSystem();
};
