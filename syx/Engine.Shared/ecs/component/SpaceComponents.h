#pragma once

#include "ecs/ECS.h"
#include "file/FilePath.h"
#include "Handle.h"

//A component on a space to designate it as a space
struct SpaceTagComponent {
};

//On default play space entity has this and SpaceTagComponent
struct DefaultPlaySpaceComponent {
};

//Messages
struct ClearSpaceComponent {
  Engine::Entity mSpace;
};

struct LoadSpaceComponent {
  //Space to load into
  Engine::Entity mSpace;
  //File to load space from
  FilePath mToLoad;
};

//The space that this entity is in
struct InSpaceComponent {
  Engine::Entity mSpace;
};

//On the space from the time the LoadSpaceComponent is processed until all entities are filled in
struct SpaceLoadingComponent {
};

//On the space after the file has been parsed until space loading is complete
struct ParsedSpaceContentsComponent {
  struct Section {
    std::vector<uint8_t> mBuffer;
  };
  std::vector<Engine::Entity> mNewEntities;
  //Section intended for each component type or group
  std::unordered_map<std::string, Section> mSections;
  //If entities couldn't be created with the original ids they are remapped through this
  std::unordered_map<Engine::Entity, Engine::Entity> mRemappings;
  FilePath mFile;
};

//On the space after all entities have created until their components have been filled in
struct SpaceFillingEntitiesComponent {
};

struct SaveSpaceComponent {
  Engine::Entity mSpace;
  FilePath mToSave;
};

struct SpaceSavingComponent {
};