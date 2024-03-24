#include "Precompile.h"
#include "Simulation.h"

#include "Queries.h"

#include "config/ConfigIO.h"
#include "File.h"
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
#include "FragmentStateMachine.h"
#include "SpatialQueries.h"
#include "ConstraintSolver.h"
#include "GameDatabase.h"
#include "SceneNavigator.h"

namespace {
  using namespace Tags;

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
    if(!sceneState || !fs) {
      task.discard();
      return;
    }
    auto textureRequests = task.query<Row<TextureLoadRequest>>();
    std::shared_ptr<ITableModifier> textureRequestModifier = task.getModifierForTable(textureRequests.matchingTableIDs.front());
    auto playerTextures = task.query<SharedRow<TextureReference>, const IsPlayer>();
    auto fragmentTextures = task.query<SharedRow<TextureReference>, const IsFragment>();
    auto terrain = task.query<SharedRow<TextureReference>, const Tags::TerrainRow>();

    task.setCallback([=](AppTaskArgs&) mutable {
      if(sceneState->mState != SceneState::State::InitRequestAssets) {
        return;
      }

      const std::string& root = fs->mRoot;
      sceneState->mBackgroundImage = _requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "background.png").c_str());
      sceneState->mPlayerImage = _requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "player.png").c_str());
      sceneState->mGroundImage = _requestTextureLoad(textureRequests.get<0>(0), *textureRequestModifier, (root + "ground.png").c_str());

      for(size_t i = 0; i < playerTextures.size(); ++i) {
        playerTextures.get<0>(i).at().mId = sceneState->mPlayerImage;
      }
      //Make all the objects use the background image as their texture
      for(size_t i = 0; i < fragmentTextures.size(); ++i) {
        fragmentTextures.get<0>(i).at().mId = sceneState->mBackgroundImage;
      }
      for(size_t i = 0; i < terrain.size(); ++i) {
        terrain.get<0>(i).at().mId = sceneState->mGroundImage;
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
    if(!sceneState) {
      task.discard();
      return;
    }

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
}

ThreadLocalsInstance::ThreadLocalsInstance() = default;
ThreadLocalsInstance::~ThreadLocalsInstance() = default;

//Currently this runs at the end of the setup steps and assume they all finish in one frame
void finishSetupState(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Finish Setup");
  SceneState* state = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
  if(!state) {
    task.discard();
    return;
  }
  task.setCallback([state](AppTaskArgs&) {
    if(state->mState == SceneState::State::SetupScene) {
      state->mState = SceneState::State::Update;
    }
  });
  builder.submitTask(std::move(task));
}

void Simulation::buildUpdateTasks(IAppBuilder& builder, const UpdateConfig& config) {
  GameplayExtract::extractGameplayData(builder);

  _initRequestAssets(builder);
  _awaitAssetLoading(builder);

  Player::setupScene(builder);
  Fragment::setupScene(builder);
  finishSetupState(builder);

  PhysicsSimulation::updatePhysics(builder);
  config.injectGameplayTasks(builder);
  SceneNavigator::update(builder);
  DebugInput::updateDebugCamera(builder);
  Player::updateInput(builder);
  Fragment::updateFragmentGoals(builder);
  World::enforceWorldBoundary(builder);
  if(config.enableFragmentStateMachine) {
    FragmentStateMachine::update(builder);
  }
  //At the end of gameplay, turn any gameplay impulses into stat effects
  GameplayExtract::applyGameplayImpulses(builder);

  //Synchronous transfer from all thread local stats to the central stats database
  StatEffect::moveThreadLocalToCentral(builder);
  SpatialQuery::gameplayUpdateQueries(builder);
  StatEffect::createTasks(builder);

  Events::publishEvents(builder);

  PhysicsSimulation::preProcessEvents(builder);
  Fragment::preProcessEvents(builder);
  FragmentStateMachine::preProcessEvents(builder);

  TableService::processEvents(builder);

  PhysicsSimulation::postProcessEvents(builder);
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

void Simulation::initScheduler(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("init scheduler");
  Scheduler* scheduler = task.query<SharedRow<Scheduler>>().tryGetSingletonElement();
  ThreadLocalsInstance* tls = task.query<ThreadLocalsRow>().tryGetSingletonElement();
  Events::EventsInstance* events = task.query<Events::EventsRow>().tryGetSingletonElement();
  StableElementMappings* mappings = &task.getDatabase().getMappings();
  task.setCallback([scheduler, tls, events, mappings](AppTaskArgs&) {
    enki::TaskSchedulerConfig cfg;
    struct ProfileStop {
      static void threadStop(uint32_t) {
        ON_PROFILE_THREAD_DESTROYED;
      }
    };
    cfg.profilerCallbacks.threadStop = &ProfileStop::threadStop;

    scheduler->mScheduler.Initialize(cfg);
    tls->instance = std::make_unique<ThreadLocals>(scheduler->mScheduler.GetNumTaskThreads(), events->impl.get(), mappings);
  });
  builder.submitTask(std::move(task));
}

void Simulation::init(IAppBuilder& builder) {
  GameDatabase::configureDefaults(builder);
  PhysicsSimulation::init(builder);
  Player::init(builder);

  {
    auto task = builder.createTask();
    task.setName("load config");
    Config::GameConfig* gameConfig = TableAdapters::getGameConfigMutable(task);
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
    task.setCallback([gameConfig, fs](AppTaskArgs&) mutable {
      if(std::optional<std::string> buffer = fs ? File::readEntireFile(*fs, Simulation::getConfigName()) : std::nullopt) {
        ConfigIO::Result result = ConfigIO::deserializeJson(*buffer, *Config::createFactory());
        std::visit([&](auto&& r) { tryInitFromConfig(*gameConfig, std::move(r)); }, std::move(result.value));
      }
      else if(gameConfig) {
        tryInitFromConfig(*gameConfig, ConfigIO::Result::Error{});
      }
    });
    builder.submitTask(std::move(task));
  }

  PhysicsSimulation::initFromConfig(builder);
}
