#pragma once

#include "Table.h"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct IDatabase;
struct StableElementMappings;

namespace SceneNavigator {
  using SceneID = size_t;

  //Add this to any tables that should be cleared as part of unloading any scene
  //Can also be done manually be the registered scene's uninit
  struct IsClearedWithSceneTag : TagRow {};

  //Registered through ISceneRegistry. Methods create the tasks relevant for the particular scenes
  //which will be registered as part of update. The tasks will only run if the given scene is active
  //and in the appropriate state, meaning init and unint calls will be balanced
  struct IScene {
    virtual ~IScene() = default;
    virtual void init(IAppBuilder&) {};
    virtual void update(IAppBuilder&) {};
    virtual void uninit(IAppBuilder&) {};
  };

  struct INavigator {
    virtual ~INavigator() = default;
    virtual SceneID getCurrentScene() const = 0;
    virtual void navigateTo(SceneID scene) = 0;
  };

  struct ISceneRegistry {
    virtual ~ISceneRegistry() = default;
    virtual SceneID registerScene(std::unique_ptr<IScene> scene) = 0;
  };

  std::shared_ptr<INavigator> createNavigator(RuntimeDatabaseTaskBuilder& task);
  std::shared_ptr<ISceneRegistry> createRegistry(RuntimeDatabaseTaskBuilder& task);

  //Creates the tasks for any registered scenes, this also means any scenes must be registered before calling this
  void update(IAppBuilder& builder);

  std::unique_ptr<IDatabase> createDB(StableElementMappings& mappings);
};