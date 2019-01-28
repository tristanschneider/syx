#pragma once
#include "system/System.h"

class AssetPreview;
class DragDropAssetLoader;
class FilePath;
class LuaGameSystemObserver;
class ObjectInspector;
enum class PlayState : uint8_t;
class SceneBrowser;
class Toolbox;

class Editor : public System {
public:
  RegisterSystemH(Editor);
  using System::System;

  void init() override;
  void uninit() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;

private:
  void _updateInput(float dt);
  void _editorUpdate();
  void _updateState(PlayState state);
  Handle _getEditorSpace() const;
  Handle _getPlaySpace() const;

  std::unique_ptr<LuaGameSystemObserver> mGameObserver;
  std::unique_ptr<SceneBrowser> mSceneBrowser;
  std::unique_ptr<ObjectInspector> mObjectInspector;
  std::unique_ptr<AssetPreview> mAssetPreview;
  std::unique_ptr<Toolbox> mToolbox;
  PlayState mCurrentState;
  std::unique_ptr<FilePath> mSavedScene;
  std::unique_ptr<DragDropAssetLoader> mDragDropAssetLoader;
};