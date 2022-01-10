#pragma once

#include "ecs/ECS.h"
#include "file/FilePath.h"
#include "Handle.h"

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