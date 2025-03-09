#include "Precompile.h"
#include "scenes/ImportedScene.h"

#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "scenes/LoadingScene.h"
#include "scenes/SceneList.h"
#include "SceneNavigator.h"
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

namespace Scenes {
  constexpr bool DEBUG_LOAD = true;

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

  struct RowLoader {
    void(*loadMulti)(const IRow& src, RuntimeTable& dst, gnx::IndexRange range){};
  };

  template<IsRow Src, Loader::IsLoadableRow Dst>
  struct DirectRowLoader {
    using src_row = Src;
    static constexpr std::string_view NAME = Dst::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      const Src& s = static_cast<const Src&>(src);
      if(Dst* dstRow = dst.tryGet<Dst>()) {
        for(size_t i : range) {
          dstRow->at(i) = static_cast<typename Dst::ElementT>(s.at(i));
        }
      }
    }
  };

  template<class R, class S, class FN>
  bool tryLoadRow(const S& s, RuntimeTable& dst, gnx::IndexRange range, FN fn) {
    if(auto row = dst.tryGet<R>()) {
      //Source is transferring all elements so is 0 to max
      //Destination is shifted by any previously existing elements
      size_t srcI{};
      for(size_t i : range) {
        row->at(i) = typename R::ElementT{ fn(s.at(srcI++)) };
      }
      return true;
    }
    return false;
  }

  struct TransformLoader {
    using src_row = Loader::TransformRow;
    static constexpr std::string_view NAME = src_row::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      using namespace gnx::func;
      const Loader::TransformRow& s = static_cast<const Loader::TransformRow&>(src);
      using Pos = GetMember<&Loader::Transform::pos>;
      tryLoadRow<Tags::PosXRow>(s, dst, range, FMap<Pos, GetX>{});
      tryLoadRow<Tags::PosYRow>(s, dst, range, FMap<Pos, GetY>{});
      tryLoadRow<Tags::PosZRow>(s, dst, range, FMap<Pos, GetZ>{});

      using Rot = GetMember<&Loader::Transform::rot>;
      tryLoadRow<Tags::RotXRow>(s, dst, range, FMap<Rot, Cos>{});
      tryLoadRow<Tags::RotYRow>(s, dst, range, FMap<Rot, Sin>{});

      using Scale = GetMember<&Loader::Transform::scale>;
      tryLoadRow<Tags::ScaleXRow>(s, dst, range, FMap<Scale, GetX>{});
      tryLoadRow<Tags::ScaleYRow>(s, dst, range, FMap<Scale, GetY>{});

      using GetThickness = FMap<Scale, GetZ>;
      //First try full thickness row. If not present, try shared instead
      if(!tryLoadRow<Narrowphase::ThicknessRow>(s, dst, range, GetThickness{})) {
        //If using shared thickness, arbitrary load the Z scale from the first element
        tryLoadRow<Narrowphase::SharedThicknessRow>(s, dst, gnx::makeIndexRangeBeginCount<size_t>(0, 1), GetThickness{});
      }
    }
  };

  struct VelocityLoader {
    using src_row = Loader::Vec4Row;
    static constexpr std::string_view NAME = "Velocity";

    static constexpr DBTypeID HASH = Loader::getDynamicRowKey<Loader::Vec4Row>("Velocity");

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      using namespace gnx::func;

      const Loader::Vec4Row& s = static_cast<const Loader::Vec4Row&>(src);
      tryLoadRow<Tags::LinVelXRow>(s, dst, range, GetX{});
      tryLoadRow<Tags::LinVelYRow>(s, dst, range, GetY{});
      tryLoadRow<Tags::LinVelZRow>(s, dst, range, GetZ{});

      tryLoadRow<Tags::AngVelRow>(s, dst, range, GetW{});
    }
  };

  struct MatMeshLoader {
    using src_row = Loader::MatMeshRefRow;
    static constexpr std::string_view NAME = src_row::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange) {
      using namespace gnx::func;

      const Loader::MatMeshRefRow& s = static_cast<const Loader::MatMeshRefRow&>(src);
      //Currently there are only shared meshes and textures, so pick one asset and use that for the table
      const auto one = gnx::makeIndexRangeBeginCount<size_t>(0, 1);
      tryLoadRow<SharedMeshRow>(s, dst, one, GetMember<&Loader::MatMeshRef::mesh>{});
      tryLoadRow<SharedTextureRow>(s, dst, one, GetMember<&Loader::MatMeshRef::material>{});
    }
  };

  template<class T>
  concept HasHash = requires() {
    typename T::src_row;
    { T::NAME } -> std::convertible_to<std::string_view>;
  };
  template<class T>
  concept HasMultiLoad = requires(const IRow& s, RuntimeTable& dst, gnx::IndexRange range) {
    T::load(s, dst, range);
  };
  template<class T> concept MultiLoader = HasHash<T> && HasMultiLoad<T>;

  template<MultiLoader L>
  std::pair<const DBTypeID, RowLoader> createRowLoader(L, std::string_view name) {
    return {
      Loader::getDynamicRowKey<typename L::src_row>(name),
      RowLoader{
        .loadMulti = &L::load
      }
    };
  }

  template<MultiLoader L>
  std::pair<const DBTypeID, RowLoader> createRowLoader(L l) {
    return createRowLoader(l, L::NAME);
  }

  const std::unordered_map<DBTypeID, RowLoader> LOADERS = {
    createRowLoader(TransformLoader{}),
    createRowLoader(VelocityLoader{}),
    createRowLoader(VelocityLoader{}, "Velocity3D"),
    createRowLoader(DirectRowLoader<Loader::BitfieldRow, ConstraintSolver::ConstraintMaskRow>{}),
    createRowLoader(MatMeshLoader{}),
    createRowLoader(DirectRowLoader<Loader::IntRow, FragmentSpawner::FragmentSpawnerCountRow>{}),
    //TODO: this is a bit weird since it'll get set from transform and this
    createRowLoader(DirectRowLoader<Loader::SharedFloatRow, Narrowphase::SharedThicknessRow>{})
  };

  void copySceneTable(RuntimeTable& src, RuntimeTable& dst) {
    if(!src.size()) {
      return;
    }

    //Default construct all the elements
    const size_t begin = dst.addElements(src.size());

    //Read elements row by row
    const auto range = gnx::makeIndexRangeBeginCount(begin, src.size());
    for(auto [type, row] : src) {
      if(auto loader = LOADERS.find(type); loader != LOADERS.end()) {
        if(loader->second.loadMulti) {
          loader->second.loadMulti(*row, dst, range);
        }
      }
    }

    //Flag creation of all the newly added elements
    if(Events::EventsRow* events = dst.tryGet<Events::EventsRow>()) {
      for(size_t i = begin; i < dst.size(); ++i) {
        events->getOrAdd(i).setCreate();
      }
    }
  }

  void copySceneDatabase(RuntimeDatabase& scene, RuntimeDatabase& game) {
    //TODO: precompute and store
    std::unordered_map<size_t, RuntimeTable*> hashToTable;
    auto names = game.query<Tags::TableNameRow>();
    for(size_t i = 0; i < names.size(); ++i) {
      hashToTable[gnx::Hash::constHash(names.get<0>(i).at().name)] = game.tryGet(names[i]);
    }

    for(size_t i = 0; i < scene.size(); ++i) {
      RuntimeTable& src = scene[i];
      if constexpr(DEBUG_LOAD) {
        if (const TableNameRow* name = Loader::tryGetDynamicRow<TableNameRow>(src)) {
          printf("%s: %d", name->at().name.c_str(), static_cast<int>(src.size()));
        }
      }
      if(auto found = hashToTable.find(src.getType().value); found != hashToTable.end() && found->second) {
        copySceneTable(src, *found->second);
      }
    }
  }

  void instantiateScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    ImportedSceneGlobals* globals = getImportedSceneGlobals(task);
    SceneView sceneView{ task };
    RuntimeDatabase* db = &task.getDatabase();
    GameDatabase::Tables tables{ task };

    task.setCallback([globals, sceneView, db, tables](AppTaskArgs&) mutable {
      printf("instantiate scene\n");
      if(const Loader::SceneAsset* scene = sceneView.tryGet(globals->toLoad); scene && scene->db) {
        copySceneDatabase(*scene->db, *db);
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
