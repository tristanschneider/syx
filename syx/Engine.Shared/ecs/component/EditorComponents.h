#pragma once

#include "ecs/ECS.h"
#include "file/FilePath.h"

//Type tag for an editor entity to store entity state
struct EditorContextComponent {};

enum class EditorPlayState : uint8_t {
  Invalid,
  Stopped,
  Paused,
  Stepping,
  Playing,
};

struct EditorPlayStateComponent {
  EditorPlayState mLastState{};
  EditorPlayState mCurrentState{};
};

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

//Create an entity with this to open an asset picker
struct InspectedAssetModalComponent {
  Engine::Entity mInspectedEntity{};
  Engine::Entity mCurrentSelection{};
  Engine::Entity mConfirmedSelection{};
  //TODO: hacky that this is needed
  typeId_t<InspectedAssetModalComponent> mID{};
  std::string mModalName;
  bool mNeedsInit = true;
};

template<class AssetT>
struct InspectAssetModalTagComponent {
};

struct AssetPreviewDialogComponent {
  Engine::Entity mAsset;
};

//Indicates that there should only be one of this type of dialog. Any creation of an entity with this should first
//check if it already exists
struct ModalComponent {};