#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "unity.h"

#include "glm/gtx/norm.hpp"
#include "Fragment.h"
#include "Serializer.h"
#include "stat/AllStatEffects.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "PhysicsSimulation.h"
#include "Player.h"
#include "DebugInput.h"
#include "World.h"

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

    std::vector<OwnedTask> tasks;
    current->mChildren.push_back(Player::updateInput({ db }).mBegin);

    current->mChildren.push_back(Fragment::updateFragmentGoals({ db }).mBegin);

    //Input and goals can happen at the same time. Input needs to finish before applyForces using point force table
    //Fragment goals need to finish before enforceWorldBoundary because it might remove objects with velocity
    auto sync = std::make_shared<TaskNode>();
    TaskBuilder::_addSyncDependency(*current, sync);
    current = sync;

    current->mChildren.push_back(TaskNode::create([&db](...) {
      World::enforceWorldBoundary({ db });
      Fragment::updateFragmentForces({ db });
    }));
    current = current->mChildren.back();

    TaskRange physics = PhysicsSimulation::updatePhysics({ db });
    current->mChildren.push_back(physics.mBegin);
    current = physics.mEnd;

    StatEffectDBOwned& statEffects = TableAdapters::getStatEffects({ db });
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

  auto root = TaskNode::create([](...){});

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

  auto current = root;
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
  phases.root.mEnd->mChildren.push_back(phases.renderRequests.mBegin);
  phases.renderRequests.mEnd->mChildren.push_back(phases.renderExtraction.mBegin);
  //Then do primary rendering
  phases.renderExtraction.mEnd->mChildren.push_back(phases.render.mBegin);
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

void Simulation::init(GameDatabase& db) {
  Scheduler& scheduler = Simulation::_getScheduler(db);
  scheduler.mScheduler.Initialize();
  GlobalGameData& globals = std::get<GlobalGameData>(db.mTables);

  std::get<ThreadLocalsRow>(globals.mRows).at().instance = std::make_unique<ThreadLocals>(scheduler.mScheduler.GetNumTaskThreads());
  StatEffect::initGlobals(TableAdapters::getStatEffects({ db }).db);
  PhysicsSimulation::init({ db });
}
