#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "unity.h"

#include "config/ConfigIO.h"
#include "File.h"
#include "glm/gtx/norm.hpp"
#include "Fragment.h"
#include "GameplayExtract.h"
#include "stat/AllStatEffects.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "PhysicsSimulation.h"
#include "Player.h"
#include "DebugInput.h"
#include "World.h"
#include "ability/PlayerAbility.h"
#include "TableService.h"
#include "AppBuilder.h"

PlayerInput::PlayerInput()
  : ability1(std::make_unique<Ability::AbilityInput>()) {
}

PlayerInput::PlayerInput(PlayerInput&&) = default;
PlayerInput& PlayerInput::operator=(PlayerInput&&) = default;

PlayerInput::~PlayerInput() = default;

namespace {
  using namespace Tags;

  StableElementMappings& _getStableMappings(GameDatabase& db) {
    return std::get<SharedRow<StableElementMappings>>(std::get<GlobalGameData>(db.mTables).mRows).at();
  }

  size_t _requestTextureLoad(Row<TextureLoadRequest>& textures, ITableModifier& textureModifier, const char* filename) {
    const size_t i = textureModifier.addElements(1);
    TextureLoadRequest& request = textures.at(i);
    request.mFileName = filename;
    request.mImageID = std::hash<std::string>()(request.mFileName);
    return request.mImageID;
  }

  void _initRequestAssets(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("request assets");
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
    auto textureRequests = task.query<Row<TextureLoadRequest>>();
    std::shared_ptr<ITableModifier> textureRequestModifier = task.getModifierForTable(textureRequests.matchingTableIDs.front());
    auto playerTextures = task.query<SharedRow<TextureReference>, const Row<PlayerInput>>();
    auto fragmentTextures = task.query<SharedRow<TextureReference>, const IsFragment>();

    task.setCallback([=](AppTaskArgs&) mutable {
      if(sceneState->mState != SceneState::State::InitRequestAssets) {
        return;
      }

      const std::string& root = fs->mRoot;
      sceneState->mBackgroundImage = _requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "background.png").c_str());
      sceneState->mPlayerImage = _requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "player.png").c_str());

      for(size_t i = 0; i < playerTextures.size(); ++i) {
        playerTextures.get<0>(i).at().mId = sceneState->mPlayerImage;
      }
      //Make all the objects use the background image as their texture
      for(size_t i = 0; i < fragmentTextures.size(); ++i) {
        fragmentTextures.get<0>(i).at().mId = sceneState->mBackgroundImage;
      }

      sceneState->mState = SceneState::State::InitAwaitingAssets;
    });
    builder.submitTask(std::move(task));
  }

  void _awaitAssetLoading(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Await Assets");
    auto textureRequests = task.query<const Row<TextureLoadRequest>>();
    auto requestModifiers = task.getModifiersForTables(textureRequests.matchingTableIDs);
    SceneState* sceneState = task.query<SharedRow<SceneState>>().tryGetSingletonElement();

    task.setCallback([textureRequests, requestModifiers, sceneState](AppTaskArgs&) mutable {
      if(sceneState->mState != SceneState::State::InitAwaitingAssets) {
        return;
      }
      for(size_t i = 0; i < textureRequests.size(); ++i) {
        for(const TextureLoadRequest& request : textureRequests.get<0>(i).mElements) {
          switch(request.mStatus) {
            case RequestStatus::InProgress:
              //If any requests are pending, keep waiting
              return;
            case RequestStatus::Failed:
              printf("failed to load texture %s", request.mFileName.c_str());
              continue;
            case RequestStatus::Succeeded:
              continue;
          }
        }
      }

      //If they're all done, clear them and continue on to the next phase
      for(auto&& modifier : requestModifiers) {
        modifier->resize(0);
      }
      sceneState->mState = SceneState::State::SetupScene;
    });
    builder.submitTask(std::move(task));
  }

  TaskRange _update(GameDatabase& db) {
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
    //Arbitrary place, probably doesn't need to be synchronous
    TaskRange fsm = FragmentStateMachine::update({ db });
    current->mChildren.push_back(fsm.mBegin);
    current = fsm.mEnd;

    //At the end of gameplay, turn any gameplay impulses into stat effects
    //Write (clear) GLinImpulse
    //TaskRange applyGameplayImpulse = GameplayExtract::applyGameplayImpulses({ db });
    //current->mChildren.push_back(applyGameplayImpulse.mBegin);
    //current = applyGameplayImpulse.mEnd;

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

    sync = TaskNode::createEmpty();
    TaskBuilder::_addSyncDependency(*current, sync);
    current = sync;

    TaskRange spatialQuery = SpatialQuery::gameplayUpdateQueries({ db });
    current->mChildren.push_back(spatialQuery.mBegin);
    current = spatialQuery.mEnd;

    current->mChildren.push_back(statTasks.synchronous.mBegin);
    current = statTasks.synchronous.mEnd;

    current->mChildren.push_back(TaskNode::create([&db](...) {
      Events::publishEvents({ db });
    }));
    current = current->mChildren.back();

    current = TaskBuilder::appendLinearRange(current, PhysicsSimulation::preProcessEvents({ db }));
    current = TaskBuilder::appendLinearRange(current, Fragment::processEvents({ db }));
    current = TaskBuilder::appendLinearRange(current, FragmentStateMachine::preProcessEvents({ db }));
    current = TaskBuilder::appendLinearRange(current, TableService::processEvents({ db }));

    current = TaskBuilder::appendLinearRange(current, PhysicsSimulation::postProcessEvents({ db }));

    return { root, current };
  }
}

ExternalDatabases::ExternalDatabases()
  : statEffects(std::make_unique<StatEffectDBOwned>()) {
}

ExternalDatabases::~ExternalDatabases() = default;

ThreadLocalsInstance::ThreadLocalsInstance() = default;
ThreadLocalsInstance::~ThreadLocalsInstance() = default;

//Currently this runs at the end of the setup steps and assume they all finish in one frame
void finishSetupState(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Finish Setup");
  SceneState* state = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
  task.setCallback([state](AppTaskArgs&) {
    if(state->mState == SceneState::State::SetupScene) {
      state->mState = SceneState::State::Update;
    }
  });
  builder.submitTask(std::move(task));
}

void Simulation::buildUpdateTasks(IAppBuilder& builder) {
  GameplayExtract::extractGameplayData(builder);

  _initRequestAssets(builder);
  _awaitAssetLoading(builder);

  Player::setupScene(builder);
  //Fragment::setupScene(builder);
  finishSetupState(builder);
  {
    //Update
  }
}

/* TODO: delete, here for order reference right now
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
*/

Scheduler& Simulation::_getScheduler(GameDatabase& db) {
  return std::get<SharedRow<Scheduler>>(std::get<GlobalGameData>(db.mTables).mRows).at();
}

const SceneState& Simulation::_getSceneState(GameDatabase& db) {
  return std::get<SharedRow<SceneState>>(std::get<GlobalGameData>(db.mTables).mRows).at();
}

void tryInitFromConfig(Config::GameConfig& toSet, const ConfigIO::Result::Error& error) {
  printf("Error reading config file, initializing with defaults. [%s]\n", error.message.c_str());
  ConfigIO::defaultInitialize(*Config::createFactory(), toSet);
}

void tryInitFromConfig(Config::GameConfig& toSet, Config::GameConfig&& loaded) {
  printf("Config found, initializing from file.\n");
  toSet = std::move(loaded);
}

const char* Simulation::getConfigName() {
  return "config.json";
}

void Simulation::init(GameDatabase& db) {
  Queries::viewEachRow(db, [](FloatRow<Tags::Rot, Tags::CosAngle>& r) {
      r.mDefaultValue = 1.0f;
  });
  Queries::viewEachRow(db, [](FloatRow<Tags::GRot, Tags::CosAngle>& r) {
      r.mDefaultValue = 1.0f;
  });
  Queries::viewEachRow(db, [](CollisionMaskRow& r) {
      r.mDefaultValue = uint8_t(~0);
  });

  //Fragments in particular start opaque then reveal the texture as they take damage
  std::get<Tint>(std::get<GameObjectTable>(db.mTables).mRows).mDefaultValue = glm::vec4(0, 0, 0, 1);

  Scheduler& scheduler = Simulation::_getScheduler(db);
  scheduler.mScheduler.Initialize();
  GlobalGameData& globals = std::get<GlobalGameData>(db.mTables);

  std::get<ThreadLocalsRow>(globals.mRows).at().instance = std::make_unique<ThreadLocals>(scheduler.mScheduler.GetNumTaskThreads());
  StatEffect::initGlobals(TableAdapters::getStatEffects({ db }).db);
  PhysicsSimulation::init({ db });
  Player::init({ db });

  Config::GameConfig* gameConfig = TableAdapters::getConfig({ db }).game;
  FileSystem* fileSystem = TableAdapters::getGlobals({ db }).fileSystem;
  if(std::optional<std::string> buffer = File::readEntireFile(*fileSystem, getConfigName())) {
    ConfigIO::Result result = ConfigIO::deserializeJson(*buffer, *Config::createFactory());
    std::visit([&](auto&& r) { tryInitFromConfig(*gameConfig, std::move(r)); }, std::move(result.value));
  }
  else {
    tryInitFromConfig(*gameConfig, ConfigIO::Result::Error{});
  }

  PhysicsSimulation::initFromConfig({ db });
}
