#pragma once

#include "ecs/ECS.h"
#include "file/FilePath.h"

//Type tag for an editor entity to store entity state
struct EditorContextComponent {};

struct EditorSceneReferenceComponent {
  Engine::Entity mScene;
};

struct EditorSavedSceneComponent {
  FilePath mFilename;
};