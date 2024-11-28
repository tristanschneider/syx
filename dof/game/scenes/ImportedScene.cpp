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
#include "DBEvents.h"

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

  struct TransformRows {
    TransformRows(RuntimeTable& table)
      : posX{ table.tryGet<Tags::PosXRow>() }
      , posY{ table.tryGet<Tags::PosYRow>() }
      , posZ{ table.tryGet<Tags::PosZRow>() }
      , rotX{ table.tryGet<Tags::RotXRow>() }
      , rotY{ table.tryGet<Tags::RotYRow>() }
      , scaleX{ table.tryGet<Tags::ScaleXRow>() }
      , scaleY{ table.tryGet<Tags::ScaleYRow>() }
    {
    }

    void read(size_t i, const Loader::Transform3D& t) {
      if(posX && posY && posZ) {
        posX->at(i) = t.pos.x;
        posY->at(i) = t.pos.y;
        posZ->at(i) = t.pos.z;
      }
      if(rotX && rotY) {
        rotX->at(i) = std::cos(t.rot);
        rotY->at(i) = std::sin(t.rot);
      }
    }

    void read(size_t i, const Loader::Scale2D& s) {
      if(scaleX && scaleY) {
        //Currently blender's mesh is -1,1 while mine is -0.5,0.5
        //For now, compensate by doubling the scale. Will likely address this when importing meshes
        constexpr float SCALAR = 2.f;
        scaleX->at(i) = s.scale.x * SCALAR;
        scaleY->at(i) = s.scale.y * SCALAR;
      }
    }

    Tags::PosXRow* posX{};
    Tags::PosYRow* posY{};
    Tags::PosZRow* posZ{};
    Tags::RotXRow* rotX{};
    Tags::RotYRow* rotY{};
    Tags::ScaleXRow* scaleX{};
    Tags::ScaleYRow* scaleY{};
  };

  struct PhysicsRows {
    PhysicsRows(RuntimeTable& table)
      : velX{ table.tryGet<Tags::LinVelXRow>() }
      , velY{ table.tryGet<Tags::LinVelYRow>() }
      , velZ{ table.tryGet<Tags::LinVelZRow>() }
      , velA{ table.tryGet<Tags::AngVelRow>() }
      , collisionMask{ table.tryGet<Narrowphase::CollisionMaskRow>() }
      , constraintMask{ table.tryGet<ConstraintSolver::ConstraintMaskRow>() }
    {
    }

    void read(size_t i, const Loader::Velocity3D& v) {
      if(velX && velY && velZ) {
        velX->at(i) = v.linear.x;
        velY->at(i) = v.linear.y;
        velZ->at(i) = v.linear.z;
        velA->at(i) = v.angular;
      }
    }

    void read(size_t i, const Loader::CollisionMask& m) {
      if(collisionMask && m.isSet()) {
        collisionMask->at(i) = m.mask;
      }
    }

    void read(size_t i, const Loader::ConstraintMask& m) {
      if(constraintMask && m.isSet()) {
        constraintMask->at(i) = m.mask;
      }
    }

    void read(const Loader::Thickness& t) {
      if(sharedThickness && t.isSet()) {
        sharedThickness->at() = t.thickness;
      }
    }

    Tags::LinVelXRow* velX{};
    Tags::LinVelYRow* velY{};
    Tags::LinVelZRow* velZ{};
    Tags::AngVelRow* velA{};
    Narrowphase::CollisionMaskRow* collisionMask{};
    Narrowphase::SharedThicknessRow* sharedThickness{};
    ConstraintSolver::ConstraintMaskRow* constraintMask{};
  };

  struct EventPublisher {
    EventPublisher(AppTaskArgs& a)
      : args{ a }
    {
    }

    void broadcastNewElements(const RuntimeTable& table) {
      if (const StableIDRow* ids = table.tryGet<const StableIDRow>()) {
        for (const ElementRef& e : ids->mElements) {
          Events::onNewElement(e, args);
        }
      }
    }

    AppTaskArgs& args;
  };

  //TODO:
  struct GraphicsRows {
    GraphicsRows(RuntimeTable&) {}

    void read(size_t, const Loader::QuadUV&) {
    }

    void read(const Loader::MeshIndex&) {
    }
  };

  void createPlayers(const Loader::PlayerTable& players,
    RuntimeDatabase& db,
    const GameDatabase::Tables& tables,
    EventPublisher& publisher
  ) {
    RuntimeTable* playerTable = db.tryGet(tables.player);
    assert(playerTable);

    playerTable->stableModifier.resize(players.players.size(), nullptr);

    TransformRows transform{ *playerTable };
    PhysicsRows physics{ *playerTable };
    GraphicsRows graphics{ *playerTable };
    for(size_t i = 0; i < players.players.size(); ++i) {
      const Loader::Player& p = players.players[i];
      transform.read(i, p.transform);
      physics.read(i, p.velocity);
      physics.read(i, p.constraintMask);
      physics.read(i, p.collisionMask);
      graphics.read(i, p.uv);
    }
    graphics.read(players.meshIndex);
    physics.read(players.thickness);

    publisher.broadcastNewElements(*playerTable);
  }

  void createTerrain(const Loader::TerrainTable& terrain,
    RuntimeDatabase& db,
    const GameDatabase::Tables& tables,
    EventPublisher& publisher
  ) {
    RuntimeTable* table = db.tryGet(tables.terrain);
    assert(table);

    table->stableModifier.resize(terrain.terrains.size(), nullptr);

    TransformRows transform{ *table };
    PhysicsRows physics{ *table };
    GraphicsRows graphics{ *table };
    for(size_t i = 0; i < terrain.terrains.size(); ++i) {
      const Loader::Terrain& t = terrain.terrains[i];
      transform.read(i, t.transform);
      transform.read(i, t.scale);
      physics.read(i, t.collisionMask);
      physics.read(i, t.constraintMask);
      graphics.read(i, t.uv);
    }
    graphics.read(terrain.meshIndex);
    physics.read(terrain.thickness);

    publisher.broadcastNewElements(*table);
  }

  void instantiateScene(IAppBuilder& builder) {
    auto task = builder.createTask();
    ImportedSceneGlobals* globals = getImportedSceneGlobals(task);
    SceneView sceneView{ task };
    RuntimeDatabase* db = &task.getDatabase();
    GameDatabase::Tables tables{ task };

    task.setCallback([globals, sceneView, db, tables](AppTaskArgs& args) mutable {
      printf("instantiate scene\n");
      if(const Loader::SceneAsset* scene = sceneView.tryGet(globals->toLoad)) {
        EventPublisher publisher{ args };
        createPlayers(scene->player, *db, tables, publisher);
        createTerrain(scene->terrain, *db, tables, publisher);
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
      , sceneID{ SceneList::get(task)->imported }
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
        .onSuccess = sceneID,
        //TODO: error handling
        .onFailure = sceneID
      });
    }

    ImportedSceneGlobals* globals{};
    std::shared_ptr<Scenes::ILoadingNavigator> navigator;
    std::shared_ptr<Loader::IAssetLoader> loader;
    SceneNavigator::SceneID sceneID{};
    SceneNavigator::SceneID loadingScene{};
  };

  std::shared_ptr<IImportedSceneNavigator> createImportedSceneNavigator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<ImportedSceneNavigator>(task);
  }

  std::unique_ptr<SceneNavigator::IScene> createImportedScene() {
    return std::make_unique<ImportedScene>();
  }

  std::unique_ptr<IDatabase> createImportedSceneDB(StableElementMappings& mappings) {
    return DBReflect::createDatabase<ImportedSceneDB>(mappings);
  }
}
