#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "unity.h"

#include "config/ConfigIO.h"
#include "File.h"
#include "glm/gtx/norm.hpp"
#include "Fragment.h"
#include "GameplayExtract.h"
#include "Serializer.h"
#include "stat/AllStatEffects.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "PhysicsSimulation.h"
#include "Player.h"
#include "DebugInput.h"
#include "World.h"
#include "ability/PlayerAbility.h"

PlayerInput::PlayerInput()
  : ability1(std::make_unique<Ability::AbilityInput>()) {
}

PlayerInput::PlayerInput(PlayerInput&&) = default;

PlayerInput::~PlayerInput() = default;

namespace {
  using namespace Tags;

  StableElementMappings& _getStableMappings(GameDatabase& db) {
    return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at();
  }

  size_t _requestTextureLoad(TextureRequestTable& requests, const char* filename) {
    TextureLoadRequest* request = &TableOperations::addToTable(requests).get<0>();
    request->mFileName = filename;
    request->mImageID = std::hash<std::string>()(request->mFileName);
    return request->mImageID;
  }

  SceneState::State _initRequestAssets(GameDatabase& db) {
    TextureRequestTable& textureRequests = std::get<TextureRequestTable>(db.mTables);

    auto& globals = std::get<GlobalGameData>(db.mTables);
    SceneState& scene = std::get<0>(globals.mRows).at();
    const std::string& root = std::get<SharedRow<FileSystem>>(globals.mRows).at().mRoot;
    scene.mBackgroundImage = _requestTextureLoad(textureRequests, (root + "background.png").c_str());
    scene.mPlayerImage = _requestTextureLoad(textureRequests, (root + "player.png").c_str());

    StaticGameObjectTable& staticObjects = std::get<StaticGameObjectTable>(db.mTables);
    GameObjectTable& gameobjects = std::get<GameObjectTable>(db.mTables);
    PlayerTable& players = std::get<PlayerTable>(db.mTables);
    std::get<SharedRow<TextureReference>>(players.mRows).at().mId = scene.mPlayerImage;
    //Make all the objects use the background image as their texture
    std::get<SharedRow<TextureReference>>(gameobjects.mRows).at().mId = scene.mBackgroundImage;
    std::get<SharedRow<TextureReference>>(staticObjects.mRows).at().mId = scene.mBackgroundImage;

    return SceneState::State::InitAwaitingAssets;
  }

  SceneState::State _awaitAssetLoading(GameDatabase& db) {
    //If there are any in progress requests keep waiting
    bool any = false;
    Queries::viewEachRow<Row<TextureLoadRequest>>(db, [&any](const Row<TextureLoadRequest>& requests) {
      for(const TextureLoadRequest& r : requests.mElements) {
        if(r.mStatus == RequestStatus::InProgress) {
          any = true;
        }
        else if(r.mStatus == RequestStatus::Failed) {
          printf("failed to load texture %s", r.mFileName.c_str());
        }
      }
    });

    //If any requests are pending, keep waiting
    if(any) {
      return SceneState::State::InitAwaitingAssets;
    }
    //If they're all done, clear them and continue on to the next phase
    //TODO: clear all tables containing row instead?
    TableOperations::resizeTable(std::get<TextureRequestTable>(db.mTables), 0);

    return SceneState::State::SetupScene;
  }

  TaskRange _update(GameDatabase& db) {
    //Synchronous because it takes snapshots
    auto root = std::make_shared<TaskNode>();
    auto current = root;
    TaskRange debugInput = DebugInput::updateDebugCamera({ db });
    root->mChildren.push_back(debugInput.mBegin);
    current = debugInput.mEnd;

    //Write to GLinImpulse
    TaskRange playerInput = Player::updateInput({ db });
    current->mChildren.push_back(playerInput.mBegin);
    //Write to fragment FragmentGoalFoundRow
    current->mChildren.push_back(Fragment::updateFragmentGoals({ db }).mBegin);

    //Write GLinImpulse
    auto worldBoundary = World::enforceWorldBoundary({ db });
    //Let player velocity write finish before all others start, which may include players
    playerInput.mEnd->mChildren.push_back(worldBoundary.mBegin);

    auto sync = std::make_shared<TaskNode>();
    TaskBuilder::_addSyncDependency(*current, sync);
    current = sync;

    //At the end of gameplay, turn any gameplay impulses into stat effects
    //Write (clear) GLinImpulse
    TaskRange applyGameplayImpulse = GameplayExtract::applyGameplayImpulses({ db });
    current->mChildren.push_back(applyGameplayImpulse.mBegin);
    current = applyGameplayImpulse.mEnd;

    TaskRange physics = PhysicsSimulation::updatePhysics({ db });
    //Physics can start immediately in parallel with gameplay
    //Thread local stat effects and the GPos style values are used to avoid gameplay reading or
    //modifying position and velocity that physics looks at
    //Physics needs to finish before stat effects are processed which is what will apply the desired
    //position and/or velocity changes from gameplay, and updates that require unique access
    root->mChildren.push_back(physics.mBegin);
    physics.mEnd->mChildren.push_back(current);

    StatEffectDBOwned& statEffects = TableAdapters::getStatEffects({ db });
    //TODO: It probably makes sense to extend this idea into a category of effects that can happen earlier in the frame
    //after gameplay has finished moving anything around but before physics is done
    AllStatTasks statTasks = StatEffect::createTasks({ db }, statEffects.db);
    //Synchronous transfer from all thread local stats to the central stats database
    ThreadLocals& locals = TableAdapters::getThreadLocals({ db });
    for(size_t i = 0; i < locals.getThreadCount(); ++i) {
      TaskRange migradeThread = StatEffect::moveTo(locals.get(i).statEffects->db, statEffects.db);
      current->mChildren.push_back(migradeThread.mBegin);
      current = migradeThread.mEnd;
    }

    TaskBuilder::_addSyncDependency(*current, statTasks.synchronous.mBegin);
    current = statTasks.synchronous.mEnd;

    return { root, current };
  }
}

ExternalDatabases::ExternalDatabases()
  : statEffects(std::make_unique<StatEffectDBOwned>()) {
}

ExternalDatabases::~ExternalDatabases() = default;

ThreadLocalsInstance::ThreadLocalsInstance() = default;
ThreadLocalsInstance::~ThreadLocalsInstance() = default;

void Simulation::loadFromSnapshot(GameDatabase& db, const char* snapshotFilename) {
  if(std::basic_ifstream<uint8_t> stream(snapshotFilename, std::ios::binary); stream.good()) {
    DeserializeStream s(stream.rdbuf());
    Serializer<GameDatabase>::deserialize(s, db);
  }
}

void Simulation::snapshotInitGraphics(GameDatabase& db) {
  //Synchronously do the asset loading steps now which will load the assets and update any existing objects to point at them
  while(_initRequestAssets(db) != SceneState::State::InitAwaitingAssets) {}
  while(_awaitAssetLoading(db) != SceneState::State::SetupScene) {}
}

void Simulation::writeSnapshot(GameDatabase& db, const char* snapshotFilename) {
  if(std::basic_ofstream<uint8_t> stream(snapshotFilename, std::ios::binary); stream.good()) {
    SerializeStream s(stream.rdbuf());
    Serializer<GameDatabase>::serialize(db, s);
  }
}

void Simulation::buildUpdateTasks(GameDatabase& db, SimulationPhases& phases) {
  Scheduler& scheduler = _getScheduler(db);

  PROFILE_SCOPE("simulation", "update");
  //TODO: move to DebugInput
  constexpr bool enableDebugSnapshot = false;
  if constexpr(enableDebugSnapshot) {
    auto snapshot = std::make_shared<TaskNode>();
    snapshot->mTask = { std::make_unique<enki::TaskSet>([&db](...) {
      PROFILE_SCOPE("simulation", "snapshot");
      writeSnapshot(db, "recovery.snap");
    }) };
    phases.root.mEnd->mChildren.push_back(snapshot);
    phases.root.mEnd = snapshot;
  }

  GlobalGameData& globals = std::get<GlobalGameData>(db.mTables);
  SceneState& sceneState = std::get<0>(globals.mRows).at();

  //Gameplay extraction can start at the same time as render extraction but render extraction doesn't need to wait on gameplay to finish
  TaskRange gameplayExtract = GameplayExtract::extractGameplayData({ db });
  auto root = gameplayExtract.mBegin;
  phases.renderExtraction.mBegin->mChildren.push_back(root);
  auto current = gameplayExtract.mEnd;
  //Wait for gameplay extract and render request processing before continuing
  phases.renderRequests.mEnd->mChildren.push_back(current);

  //TODO: generalize this so varied scene types can be supplied
  current->mChildren.push_back(TaskNode::create([&globals, &sceneState, &db](...) {
    if(sceneState.mState == SceneState::State::InitRequestAssets) {
      sceneState.mState = _initRequestAssets(db);
    }
  }));

  current->mChildren.push_back(TaskNode::create([&globals, &sceneState, &db](...) {
    if(sceneState.mState == SceneState::State::InitAwaitingAssets) {
      sceneState.mState = _awaitAssetLoading(db);
    }
  }));

  current->mChildren.push_back(TaskNode::create([&globals, &sceneState, &db](...) {
    if(sceneState.mState == SceneState::State::SetupScene) {
      Fragment::setupScene({ db });
      sceneState.mState = SceneState::State::Update;
    }
  }));

  TaskRange update = _update(db);

  current->mChildren.push_back(TaskNode::create([&sceneState, &scheduler, update](...) {
    //Either run the update or skip to the end
    if(sceneState.mState == SceneState::State::Update) {
      update.mBegin->mTask.addToPipe(scheduler.mScheduler);
    }
    else {
      update.mEnd->mTask.addToPipe(scheduler.mScheduler);
    }
  }));
  auto endOfFrame = TaskNode::create([](...) {});

  TaskBuilder::_addSyncDependency(*current, endOfFrame);

  //Tie completion of update to the end of the frame. This will either run through the entire update via begin
  //or be just the end if skipped above
  update.mEnd->mChildren.push_back(endOfFrame);

  //Need to manually build these since they are queued in the above task rather than being a child in the tree
  TaskBuilder::buildDependencies(update.mBegin);

  phases.simulation.mBegin = root;
  phases.simulation.mEnd = endOfFrame;
}

void Simulation::linkUpdateTasks(SimulationPhases& phases) {
  //First process requests, then extract renderables
  phases.root.mEnd->mChildren.push_back(phases.renderExtraction.mBegin);
  //Then requests because they might rely on extracted data
  phases.renderExtraction.mEnd->mChildren.push_back(phases.renderRequests.mBegin);
  //Then do primary rendering
  phases.renderRequests.mEnd->mChildren.push_back(phases.render.mBegin);
  //Then render imgui, which currently also depends on simulation being complete due to requiring access to DB
  phases.render.mEnd->mChildren.push_back(phases.imgui.mBegin);
  phases.simulation.mEnd->mChildren.push_back(phases.imgui.mBegin);
  //Once imgui is complete, buffers can be swapped
  phases.imgui.mEnd->mChildren.push_back(phases.swapBuffers.mBegin);
}

Scheduler& Simulation::_getScheduler(GameDatabase& db) {
  return std::get<SharedRow<Scheduler>>(std::get<GlobalGameData>(db.mTables).mRows).at();
}

const SceneState& Simulation::_getSceneState(GameDatabase& db) {
  return std::get<SharedRow<SceneState>>(std::get<GlobalGameData>(db.mTables).mRows).at();
}

void tryInitFromConfig(GameConfig&, const ConfigIO::Result::Error& error) {
  printf("Error reading config file, initializing with defaults. [%s]\n", error.message.c_str());
}

void tryInitFromConfig(GameConfig& toSet, const Config::RawGameConfig& loaded) {
  printf("Config found, initializing from file.\n");
  toSet = ConfigConvert::toGame(loaded);
}

const char* Simulation::getConfigName() {
  return "config.json";
}


void Simulation::init(GameDatabase& db) {
  Scheduler& scheduler = Simulation::_getScheduler(db);
  scheduler.mScheduler.Initialize();
  GlobalGameData& globals = std::get<GlobalGameData>(db.mTables);

  std::get<ThreadLocalsRow>(globals.mRows).at().instance = std::make_unique<ThreadLocals>(scheduler.mScheduler.GetNumTaskThreads());
  StatEffect::initGlobals(TableAdapters::getStatEffects({ db }).db);
  PhysicsSimulation::init({ db });
  Player::init({ db });

  GameConfig* gameConfig = TableAdapters::getConfig({ db }).game;
  FileSystem* fileSystem = TableAdapters::getGlobals({ db }).fileSystem;
  if(std::optional<std::string> buffer = File::readEntireFile(*fileSystem, getConfigName())) {
    ConfigIO::Result result = ConfigIO::deserializeJson(*buffer);
    std::visit([&](const auto& r) { tryInitFromConfig(*gameConfig, r); }, result.value);
  }
}
