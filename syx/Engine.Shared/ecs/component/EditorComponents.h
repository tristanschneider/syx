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

//On objects that are currently selected
struct SelectedComponent {
};

struct PickerContextComponent {
  std::optional<size_t> mSelectedID;
};

struct ObjectInspectorContextComponent {};