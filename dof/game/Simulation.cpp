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
#include "scenes/SceneList.h"

ThreadLocalsInstance::ThreadLocalsInstance() = default;
ThreadLocalsInstance::~ThreadLocalsInstance() = default;

void Simulation::buildUpdateTasks(IAppBuilder& builder, const UpdateConfig& config) {
  SceneList::registerScenes(builder);

  GameplayExtract::extractGameplayData(builder);

  PhysicsSimulation::updatePhysics(builder);
  config.injectGameplayTasks(builder);
  SceneNavigator::update(builder);
  DebugInput::updateDebugCamera(builder);
  Player::updateInput(builder);
  Fragment::updateFragmentGoals(builder);

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
