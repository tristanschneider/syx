#include "Precompile.h"
#include "scenes/ImportedScene.h"

#include "SceneNavigator.h"

#include "IAppModule.h"
#include "AppBuilder.h"
#include "scenes/LoadingScene.h"
#include "scenes/SceneList.h"
#include "loader/AssetHandle.h"
#include "loader/AssetLoader.h"
#include "loader/SceneAsset.h"
#include "GameDatabase.h"
#include "RowTags.h"
#include "Narrowphase.h"
#include "ConstraintSolver.h"
#include "Events.h"
#include "Simulation.h"
#include "GraphicsTables.h"
#include "FragmentSpawner.h"
#include "generics/IndexRange.h"
#include "generics/Functional.h"
#include "TableName.h"
#include "RespawnArea.h"
#include "loader/ReflectionModule.h"
#include <transform/TransformRows.h>

namespace Scenes {
  struct ImportedSceneGlobals {
    Loader::AssetHandle toLoad;
  };
  struct ImportedSceneGlobalsRow : SharedRow<ImportedSceneGlobals> {};
  using ImportedSceneDB = Database<
    Table<ImportedSceneGlobalsRow>
  >;

  ImportedSceneGlobals* getImportedSceneGlobals(RuntimeDatabaseTaskBuilder& task) {
    auto result = task.query<ImportedSceneGlobalsRow>().tryGetSingletonElement();
    assert(result);
    return result;
  }

  struct SceneView {
    SceneView(RuntimeDatabaseTaskBuilder& task)
      : resolver{ task.getResolver(scene, stable) }
      , res{ task.getIDResolver()->getRefResolver() } {
    }

    const Loader::SceneAsset* tryGet(const Loader::AssetHandle& handle) {
      auto id = res.tryUnpack(handle.asset);
      return id ? resolver->tryGetOrSwapRowElement(scene, *id) : nullptr;
    }

    CachedRow<const Loader::SceneAssetRow> scene;
    CachedRow<const StableIDRow> stable;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver res;
  };

  void instantiateScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    ImportedSceneGlobals* globals = getImportedSceneGlobals(task);
    SceneView sceneView{ task };
    RuntimeDatabase* db = &task.getDatabase();
    GameDatabase::Tables tables{ task };
    std::shared_ptr<Reflection::IReflectionReader> reader = Reflection::createReader(task);

    task.setCallback([reader, globals, sceneView, db, tables](AppTaskArgs&) mutable {
      printf("instantiate scene\n");
      if(const Loader::SceneAsset* scene = sceneView.tryGet(globals->toLoad); scene && scene->db && reader) {
        reader->loadFromDBIntoGame(*scene->db);
      }

      //Load is finished, release asset handle
      globals->toLoad = {};
    });

    builder.submitTask(std::move(task.setName("instantiate scene")));
  }

  struct ImportedScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      instantiateScene(builder);
    }

    //Scene manages itself based on what was loaded
    void update(IAppBuilder&) final {}

    //Contents of scene are unloaded by the IsClearedWithSceneTag
    void uninit(IAppBuilder&) final {}
  };

  struct ImportedSceneNavigator : IImportedSceneNavigator {
    ImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task)
      : globals{ task.query<ImportedSceneGlobalsRow>().tryGetSingletonElement<0>() }
      , navigator{ Scenes::createLoadingNavigator(task) }
      , loader{ Loader::createAssetLoader(task) }
      , scenes{ SceneList::get(task) }
    {
    }

    Loader::AssetHandle importScene(Loader::AssetLocation&& location) final {
      const Loader::AssetHandle handle = loader->requestLoad(std::move(location));
      instantiateImportedScene(handle);
      return handle;
    }

    void instantiateImportedScene(const Loader::AssetHandle& scene) final {
      //Store the desired scene for ImportedScene to load
      globals->toLoad = scene;
      //Navigate to the loading screen and await the loading of the scene asset
      //Upon completion, navigate to the imported scene which will load from globals
      navigator->awaitLoadRequest(Scenes::LoadRequest{
        .toAwait = { scene },
        .onSuccess = scenes->imported,
        //TODO: error handling
        .onFailure = scenes->imported,
      });
    }

    ImportedSceneGlobals* globals{};
    std::shared_ptr<Scenes::ILoadingNavigator> navigator;
    std::shared_ptr<Loader::IAssetLoader> loader;
    const SceneList::Scenes* scenes{};
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<ImportedSceneNavigator>(task);
  }

  std::unique_ptr<SceneNavigator::IScene> createImportedScene() {
    return std::make_unique<ImportedScene>();
  }

  void createImportedSceneDB(RuntimeDatabaseArgs& args) {
    return DBReflect::addDatabase<ImportedSceneDB>(args);
  }
}

namespace BasicLoaders {
  struct TransformLoader {
    using src_row = Loader::TransformRow;
    static constexpr std::string_view NAME = src_row::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      using namespace gnx::func;
      using namespace Reflection;
      const Loader::TransformRow& s = static_cast<const Loader::TransformRow&>(src);
      using Scale = GetMember<&Loader::Transform::scale>;
      using GetThickness = FMap<Scale, GetZ>;
      //First try full thickness row. If not present, try shared instead
      if(!tryLoadRow<Narrowphase::ThicknessRow>(s, dst, range, GetThickness{})) {
        //If using shared thickness, arbitrary load the Z scale from the first element
        tryLoadRow<Narrowphase::SharedThicknessRow>(s, dst, gnx::makeIndexRangeBeginCount<size_t>(0, 1), GetThickness{});
      }
    }
  };

  struct MatMeshLoader {
    using src_row = Loader::MatMeshRefRow;
    static constexpr std::string_view NAME = src_row::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      using namespace gnx::func;
      using namespace Reflection;

      const Loader::MatMeshRefRow& s = static_cast<const Loader::MatMeshRefRow&>(src);
      //Currently there are only shared meshes and textures, so pick one asset and use that for the table
      const auto one = gnx::makeIndexRangeBeginCount<size_t>(0, 1);
      tryLoadRow<SharedMeshRow>(s, dst, one, GetMember<&Loader::MatMeshRef::mesh>{});
      tryLoadRow<MeshRow>(s, dst, range, GetMember<&Loader::MatMeshRef::mesh>{});
      tryLoadRow<SharedTextureRow>(s, dst, one, GetMember<&Loader::MatMeshRef::material>{});
    }
  };

  struct Module : IAppModule {
    void init(IAppBuilder& builder) final {
      using namespace Reflection;
      Reflection::registerLoaders(builder,
        createRowLoader(TransformLoader{}),
        createRowLoader(MatMeshLoader{}),
        createRowLoader(DirectRowLoader<Loader::IntRow, FragmentSpawner::FragmentSpawnerCountRow>{}),
        //TODO: this is a bit weird since it'll get set from transform and this
        createRowLoader(DirectRowLoader<Loader::SharedFloatRow, Narrowphase::SharedThicknessRow>{})
      );
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<Module>();
  }
}
