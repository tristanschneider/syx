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

//Not a component, a type to use for ObjectInspectorTraits ModalInspectorImpl<AssetInspectorModal<AssetT>>;
template<class AssetT>
struct AssetInspectorModal {
  using AssetTy = AssetT;
};

// Create an entity with this to open an asset picker
struct InspectedAssetModalComponent {
  Engine::Entity mInspectedEntity{};
  Engine::Entity mCurrentSelection{};
  Engine::Entity mConfirmedSelection{};
  std::string mModalName;
  bool mNeedsInit = true;
};